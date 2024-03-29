/*
 * wav_recording.c - wav file recording routines
 * also contains recording-related functions for Stereo Triggered Sampler application
 *
 * Authors: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplo@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#include "wav_recording.h"
#include "audio_sdram.h"
#include "audio_util.h"
#include "bank.h"
#include "calibration.h"
#include "circular_buffer.h"
#include "dig_pins.h"
#include "fatfs_util.h"
#include "ff.h"
#include "globals.h"
#include "params.h"
#include "rgb_leds.h"
#include "sampler.h"
#include "sdram_driver.h"
#include "str_util.h"
#include "sts_filesystem.h"
#include "sts_sampleindex.h"
#include "timekeeper.h"
#include "wavefmt.h"

extern volatile uint32_t sys_tmr;
extern enum g_Errors g_error;

extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern GlobalParams global_params;

extern uint8_t flags[NUM_FLAGS];

extern enum PlayLoadTriage play_load_triage;

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

extern SystemCalibrations *system_calibrations;

// uint32_t WATCH_REC_BUFF;
// uint32_t WATCH_REC_BUFF_IN;
// uint32_t WATCH_REC_BUFF_OUT;

#define WRITE_BLOCK_SIZE 9216

// MAX_REC_SAMPLES = Maximum bytes of sample data
// WAV file specification limits sample data to 4GB = 0xFFFFFFFF Bytes
//
// Assuming our rec buffer is full, we should stop recording when we reach
// 4GB minus the size of SDRAM record buffer.
// Then we shorten this by two WRITE_BLOCK_SIZEs just to be safe.
// Note: the wav file itself can exceed 4GB, just the 'data' chunk size must be <=4GB
//
#define MAX_REC_SAMPLES (0xFFFFFFFF - REC_BUFF_SIZE - (WRITE_BLOCK_SIZE * 2))

//#define MAX_REC_SAMPLES 	(0x00A17FC0)  /*DEBUGGING: about 1 minute*/

//
// SDRAM buffer address for recording to sdcard
// Codec --> SDRAM (@rec_buff->in) .... SDRAM (@rec_buff->out) --> SD Card:recfil@rec_storage_addr
//

CircularBuffer srec_buff;
CircularBuffer *rec_buff;

//temporary SRAM buffer for storing received audio data until it can be written to SDRAM
int16_t rec_buff16[WRITE_BLOCK_SIZE >> 1];

enum RecStates rec_state;
uint32_t samplebytes_recorded;
uint8_t sample_num_now_recording;
uint8_t sample_num_to_record_in;
uint8_t sample_bank_now_recording;
uint8_t sample_bytesize_now_recording;
char sample_fname_now_recording[_MAX_LFN];
WaveHeaderAndChunk whac_now_recording; // whac = "Wave Header And Chunk"

uint8_t recording_enabled;

FIL recfil;

void init_rec_buff(void) {
	rec_buff = &srec_buff;
	rec_buff->in = REC_BUFF_START;
	rec_buff->out = REC_BUFF_START;
	rec_buff->min = REC_BUFF_START;
	rec_buff->max = REC_BUFF_START + REC_BUFF_SIZE;
	rec_buff->size = REC_BUFF_SIZE;
	rec_buff->wrapping = 0;

	sample_num_to_record_in = i_param[REC][SAMPLE];
}

void stop_recording(void) {
	if (rec_state == RECORDING || rec_state == CREATING_FILE) {
		rec_state = CLOSING_FILE;
	}
}

void toggle_recording(void) {
	if (rec_state == RECORDING || rec_state == CREATING_FILE) {
		rec_state = CLOSING_FILE;

	} else {
		if (global_mode[ENABLE_RECORDING]) {
			CB_init(rec_buff, 0);
			// WATCH_REC_BUFF_IN = rec_buff->in;
			// WATCH_REC_BUFF_OUT = rec_buff->out;

			rec_state = CREATING_FILE;
		}
	}
}

// Main routine that handles recording codec input stream (src)
// to the SDRAM buffer (rec_buff)
//
void record_audio_to_buffer(int16_t *src) {
	uint32_t i;
	uint32_t overrun;
	int32_t dummy;
	uint16_t topword, bottomword;
	uint8_t bottombyte;

	if (rec_state == RECORDING || rec_state == CREATING_FILE) {
		// WATCH_REC_BUFF = CB_distance(rec_buff, 0);
		// if (WATCH_REC_BUFF == 0)
		// 	{DEBUG0_ON;DEBUG0_OFF;}

		overrun = 0;

		//
		// Dump HT16_BUFF_LEN samples of the rx buffer from codec (src) into rec_buff
		// This code is optimized by using the SDRAM_IS_BUSY macro, at the expense
		// of code readibility. The processor cycles saved were crucial!
		// Todo: additional cycles could be saved by inlining CB_offset_in_address()

		for (i = 0; i < HT16_BUFF_LEN; i++) {
			//
			// Split incoming stereo audio into the two channels
			//
			if (!global_mode[REC_24BITS]) {
				// 16-bit recording:
				//

				*((int16_t *)rec_buff->in) = *src++;
				dummy = *src++; //ignore bottom bits

				while (SDRAM_IS_BUSY) {
					;
				}

				CB_offset_in_address(rec_buff, 2, 0);

				// WATCH_REC_BUFF_IN = rec_buff->in;

				// Flag an buffer overrun condition if the in and out pointers cross
				// But, don't consider the heads being crossed if they end at the same place
				if ((rec_buff->in == rec_buff->out) && i != (HT16_BUFF_LEN - 1))
					overrun = rec_buff->out;
			} else {
				// 24-bit recording:
				//
				topword = (uint16_t)(*src++);
				bottomword = (uint16_t)(*src++);

				bottombyte = bottomword >> 8;

				*((uint8_t *)rec_buff->in) = bottombyte;
				CB_offset_in_address(rec_buff, 1, 0);
				while (SDRAM_IS_BUSY) {
					;
				}

				*((uint16_t *)rec_buff->in) = topword;
				CB_offset_in_address(rec_buff, 2, 0);
				while (SDRAM_IS_BUSY) {
					;
				}

				// WATCH_REC_BUFF_IN = rec_buff->in;

				// Flag an buffer overrun condition if the in and out pointers cross
				// But, don't consider the heads being crossed if they end at the same place
				if ((rec_buff->in == rec_buff->out) && i != (HT16_BUFF_LEN - 1))
					overrun = rec_buff->out;
			}
		}

		if (overrun) {
			g_error |= WRITE_BUFF_OVERRUN;
			check_errors();
		}
	}
}

// Creates a new file and writes a wave header and header chunk to it
// If it succeeds, it sets the recording state to RECORDING
// Otherwise, it sets a g_error and returns
//
void create_new_recording(uint8_t bitsPerSample, uint8_t numChannels, uint32_t sample_rate) {
	uint32_t sz;
	FRESULT res;
	uint32_t written;
	DIR dir;

	//Make a file with a temp name (tmp-XXXXX.wav), inside the temp dir

	//Open the temp directory
	res = f_opendir(&dir, TMP_DIR);

	//If it doesn't exist, create it
	if (res == FR_NO_PATH)
		res = f_mkdir(TMP_DIR);

	//If we got an error opening or creating a dir
	//try reloading the SDCard, then opening the dir (and creating if needed)
	if (res != FR_OK) {
		res = reload_sdcard();
		if (res == FR_OK) {
			res = f_opendir(&dir, TMP_DIR);
			if (res == FR_NO_PATH)
				res = f_mkdir(TMP_DIR);
		}
	}

	//If we just can't open or create the tmp dir, just put it in the root dir
	if (res != FR_OK)
		str_cpy(sample_fname_now_recording, "tmp-");

	else
		str_cat(sample_fname_now_recording, TMP_DIR_SLASH, "tmp-");

	sz = str_len(sample_fname_now_recording);
	sz += intToStr(sys_tmr, &(sample_fname_now_recording[sz]), 0);
	sample_fname_now_recording[sz++] = '.';
	sample_fname_now_recording[sz++] = 'w';
	sample_fname_now_recording[sz++] = 'a';
	sample_fname_now_recording[sz++] = 'v';
	sample_fname_now_recording[sz++] = 0;

	create_waveheader(&whac_now_recording.wh, &whac_now_recording.fc, bitsPerSample, numChannels, sample_rate);
	create_chunk(ccDATA, 0, &whac_now_recording.wc);

	sz = sizeof(WaveHeaderAndChunk);

	//Try to create the tmp file and write to it, reloading the sd card if needed

	res = f_open(&recfil, sample_fname_now_recording, FA_WRITE | FA_CREATE_NEW | FA_READ);
	if (res == FR_OK)
		res = f_write(&recfil, &whac_now_recording.wh, sz, &written);

	if (res != FR_OK) {
		f_close(&recfil);
		res = reload_sdcard();
		if (res == FR_OK) {
			res = f_open(&recfil, sample_fname_now_recording, FA_WRITE | FA_CREATE_NEW);
			f_sync(&recfil);
			res = f_write(&recfil, &whac_now_recording.wh, sz, &written);
			f_sync(&recfil);
		} else {
			f_close(&recfil);
			rec_state = REC_OFF;
			g_error |= FILE_WRITE_FAIL;
			check_errors();
			return;
		}
	}

	if (sz != written) {
		f_close(&recfil);
		rec_state = REC_OFF;
		g_error |= FILE_UNEXPECTEDEOF_WRITE;
		check_errors();
		return;
	}

	samplebytes_recorded = 0;

	rec_state = RECORDING;
}

// Writes the data chunk size and the file fize into the given wave file
// The former is written into the RIFF chunk, and the latter is written
// to the 'data' chunk.
//
FRESULT write_wav_size(FIL *wavfil, uint32_t data_chunk_bytes, uint32_t file_size_bytes) {
	uint32_t orig_pos;
	uint32_t written;
	FRESULT res;

	//cache the original file position
	orig_pos = f_tell(wavfil);

	//RIFF file size
	whac_now_recording.wh.fileSize = file_size_bytes;

	//data chunk size
	whac_now_recording.wc.chunkSize = data_chunk_bytes;

	res = f_lseek(wavfil, 0);
	if (res == FR_OK) {
		res = f_write(wavfil, &whac_now_recording, sizeof(WaveHeaderAndChunk), &written);
	}

	if (res != FR_OK) {
		g_error |= FILE_WRITE_FAIL;
		check_errors();
		return (res);
	}
	if (written != sizeof(WaveHeaderAndChunk)) {
		g_error |= FILE_UNEXPECTEDEOF_WRITE;
		check_errors();
		return (FR_INT_ERR);
	}

	//restore the original file position
	res = f_lseek(wavfil, orig_pos);
	return (res);
}

// Writes comments into the INFO chunk of the given wav file
// ToDo: This only works for firmware versions 0.0 to 9.9
// ToDo: firmware tag limited to _MAX_LFN char
// The firmware version and comment string are set in wav_recording.h
// and globals.h
//
FRESULT write_wav_info_chunk(FIL *wavfil, uint32_t *total_written) {

	uint32_t written;
	FRESULT res;
	uint8_t chunkttl_len = 4;
	uint32_t num32;
	char firmware[_MAX_LFN + 1];
	char temp_a[_MAX_LFN + 1];
	uint8_t pad_zeros[4] = {0, 0, 0, 0};

	// format firmware version string with version info
	intToStr(FW_MAJOR_VERSION, temp_a, 1);
	str_cat(firmware, WAV_SOFTWARE, temp_a);
	str_cat(firmware, firmware, ".");
	intToStr(FW_MINOR_VERSION, temp_a, 1);
	str_cat(firmware, firmware, temp_a);

	struct Chunk {
		uint32_t len;
		uint32_t padlen;
	};

	struct Chunk list_ck, comment_ck, firmware_ck;

	// COMPUTE CHUNK LENGTHS
	// wav comment
	comment_ck.len = str_len(WAV_COMMENT);
	if (comment_ck.len % 4)
		comment_ck.padlen = 4 - (comment_ck.len % 4);

	// firmware version
	firmware_ck.len = str_len(firmware);
	if ((firmware_ck.len) % 4)
		firmware_ck.padlen = 4 - (firmware_ck.len % 4);

	// list chunk header
	list_ck.len = comment_ck.len + comment_ck.padlen + firmware_ck.len + firmware_ck.padlen + 3 * chunkttl_len +
				  2 * chunkttl_len; // 3x: INFO ICMT ISFT  2x: lenght entry, after title

	*total_written = 0;

	//WRITE DATA TO WAV FILE
	res = f_write(wavfil, "LIST", chunkttl_len, &written);
	if (res != FR_OK)
		return (res);
	if (written != chunkttl_len)
		return (FR_INT_ERR);
	*total_written += written;

	res = f_write(wavfil, &list_ck.len, chunkttl_len, &written);
	if (res != FR_OK)
		return (res);
	if (written != chunkttl_len)
		return (FR_INT_ERR);
	*total_written += written;

	res = f_write(wavfil, "INFOICMT", chunkttl_len * 2, &written);
	if (res != FR_OK)
		return (res);
	if (written != chunkttl_len * 2)
		return (FR_INT_ERR);
	*total_written += written;

	num32 = comment_ck.len + comment_ck.padlen;
	res = f_write(wavfil, &num32, sizeof(num32), &written);
	if (res != FR_OK)
		return (res);
	if (written != sizeof(num32))
		return (FR_INT_ERR);
	*total_written += written;

	res = f_write(wavfil, WAV_COMMENT, comment_ck.len, &written);
	if (res != FR_OK)
		return (res);
	if (written != comment_ck.len)
		return (FR_INT_ERR);
	*total_written += written;

	res = f_write(wavfil, &pad_zeros, comment_ck.padlen, &written);
	if (res != FR_OK)
		return (res);
	if (written != comment_ck.padlen)
		return (FR_INT_ERR);
	*total_written += written;

	res = f_write(wavfil, "ISFT", chunkttl_len, &written);
	if (res != FR_OK)
		return (res);
	if (written != chunkttl_len)
		return (FR_INT_ERR);
	*total_written += written;

	num32 = firmware_ck.len + firmware_ck.padlen;
	res = f_write(wavfil, &num32, sizeof(num32), &written);
	if (res != FR_OK)
		return (res);
	if (written != sizeof(num32))
		return (FR_INT_ERR);
	*total_written += written;

	res = f_write(wavfil, &firmware, firmware_ck.len, &written);
	if (res != FR_OK)
		return (res);
	if (written != firmware_ck.len)
		return (FR_INT_ERR);
	*total_written += written;

	res = f_write(wavfil, &pad_zeros, firmware_ck.padlen, &written);
	if (res != FR_OK)
		return (res);
	if (written != firmware_ck.padlen)
		return (FR_INT_ERR);
	*total_written += written;

	f_sync(wavfil);

	return (FR_OK);
}

// Main routine to handle the recording process from SDRAM to SD Card
//
// If we just requested to start recording, it handles creating the new file
// If we're recording, it writes a block of data from the SDRAM buffer onto the SD Card
// If we're done recording, it handles closing the file, adding tags, renaming it
// If we need to close the file because we reached the size limit, but need to
// continue recording, it handles all of that.
//
void write_buffer_to_storage(void) {
	uint32_t buffer_lead;
	uint32_t addr_exceeded;
	uint32_t written;
	static uint32_t write_size_ctr = 0;

	FRESULT res;
	char final_filepath[_MAX_LFN];
	uint32_t sz;

	if (flags[RecSampleChanged]) {
		sample_num_to_record_in = i_param[REC_CHAN][SAMPLE];
		flags[RecSampleChanged] = 0;
	}

	//if (flags[RecBankChanged]) {
	//	//Currently, nothing happens if you change record bank
	//	flags[RecBankChanged] = 0;
	//}

	// Handle write buffers (transfer SDRAM to SD card)
	switch (rec_state) {
		case (CREATING_FILE): //first time, create a new file
			if (recfil.obj.fs != 0) {
				rec_state = CLOSING_FILE;
			}

			sample_num_now_recording = sample_num_to_record_in; //i_param[REC_CHAN][SAMPLE];
			sample_bank_now_recording = i_param[REC_CHAN][BANK];

			if (global_mode[REC_24BITS])
				sample_bytesize_now_recording = 3;
			else
				sample_bytesize_now_recording = 2;

			create_new_recording(8 * sample_bytesize_now_recording, 2, global_params.record_sample_rate);

			break;

		case (RECORDING):
			//read a block from rec_buff->out

			//FixMe: Enable load triaging
			// if (play_load_triage==0)
			if (1) {
				buffer_lead = CB_distance(rec_buff, 0);

				if (buffer_lead > WRITE_BLOCK_SIZE) {
					if (sample_bytesize_now_recording == 3)
						addr_exceeded = memory_read24_cb(rec_buff, (uint8_t *)rec_buff16, WRITE_BLOCK_SIZE / 3, 0);
					else
						addr_exceeded = memory_read16_cb(rec_buff, rec_buff16, WRITE_BLOCK_SIZE >> 1, 0);

					// WATCH_REC_BUFF_OUT = rec_buff->out;

					if (addr_exceeded) {
						g_error |= WRITE_BUFF_OVERRUN;
						check_errors();
					}

					sz = WRITE_BLOCK_SIZE;
					res = f_write(&recfil, rec_buff16, sz, &written);

					if (res != FR_OK) {
						if (g_error & FILE_WRITE_FAIL) {
							f_close(&recfil);
							rec_state = REC_OFF;
						}
						g_error |= FILE_WRITE_FAIL;
						check_errors();
						break;
					}
					if (sz != written) {
						if (g_error & FILE_UNEXPECTEDEOF_WRITE) {
							f_close(&recfil);
							rec_state = REC_OFF;
						}
						g_error |= FILE_UNEXPECTEDEOF_WRITE;
						check_errors();
					}

					samplebytes_recorded += written;

					//Update the wav file size in the wav header
					if (write_size_ctr++ > 20) {
						write_size_ctr = 0;
						res = write_wav_size(
							&recfil, samplebytes_recorded, samplebytes_recorded + sizeof(WaveHeaderAndChunk) - 8);
						if (res != FR_OK) {
							f_close(&recfil);
							rec_state = REC_OFF;
							g_error |= FILE_WRITE_FAIL;
							check_errors();
							break;
						}
					}
					f_sync(&recfil);

					if (res != FR_OK) {
						if (g_error & FILE_WRITE_FAIL) {
							f_close(&recfil);
							rec_state = REC_OFF;
						}
						g_error |= FILE_WRITE_FAIL;
						check_errors();
						break;
					}

					//Stop recording in this file, if we are close the maximum
					//Then we will start recording again in a new file --there is a ~20ms gap between files
					//
					if (samplebytes_recorded >= MAX_REC_SAMPLES)
						rec_state = CLOSING_FILE_TO_REC_AGAIN;
				}
			}

			break;

		case (CLOSING_FILE):
		case (CLOSING_FILE_TO_REC_AGAIN):
			//See if we have more in the buffer to write
			buffer_lead = CB_distance(rec_buff, 0);

			if (buffer_lead) {
				//Write out remaining data in buffer, one WRITE_BLOCK_SIZE at a time
				if (buffer_lead > WRITE_BLOCK_SIZE)
					buffer_lead = WRITE_BLOCK_SIZE;

				if (sample_bytesize_now_recording == 3)
					addr_exceeded = memory_read24_cb(rec_buff, (uint8_t *)rec_buff16, buffer_lead / 3, 0);
				else
					addr_exceeded = memory_read16_cb(rec_buff, rec_buff16, buffer_lead >> 1, 0);

				// WATCH_REC_BUFF_OUT = rec_buff->out;

				if (!addr_exceeded) {
					g_error = 999; //math error
					check_errors();
				}

				res = f_write(&recfil, rec_buff16, buffer_lead, &written);
				f_sync(&recfil);

				if (res != FR_OK) {
					if (g_error & FILE_WRITE_FAIL) {
						f_close(&recfil);
						rec_state = REC_OFF;
					}
					g_error |= FILE_WRITE_FAIL;
					check_errors();
					break;
				}
				if (written != buffer_lead) {
					if (g_error & FILE_UNEXPECTEDEOF_WRITE) {
						f_close(&recfil);
						rec_state = REC_OFF;
					}
					g_error |= FILE_UNEXPECTEDEOF_WRITE;
					check_errors();
				}

				samplebytes_recorded += written;

				//Update the wav file size in the wav header
				res = write_wav_size(
					&recfil, samplebytes_recorded, samplebytes_recorded + sizeof(WaveHeaderAndChunk) - 8);
				if (res != FR_OK) {
					f_close(&recfil);
					rec_state = REC_OFF;
					g_error |= FILE_WRITE_FAIL;
					check_errors();
					break;
				}

			} else {

				// Write comment and Firmware chunks at bottom of wav file
				res = write_wav_info_chunk(&recfil, &written);
				if (res != FR_OK) {
					f_close(&recfil);
					rec_state = REC_OFF;
					g_error |= FILE_WRITE_FAIL;
					check_errors();
					break;
				}

				// Write new file size and data chunk size
				res = write_wav_size(
					&recfil, samplebytes_recorded, samplebytes_recorded + written + sizeof(WaveHeaderAndChunk) - 8);
				if (res != FR_OK) {
					f_close(&recfil);
					rec_state = REC_OFF;
					g_error |= FILE_WRITE_FAIL;
					check_errors();
					break;
				}

				f_close(&recfil);

				//Rename the tmp file as the proper file in the proper directory
				res = new_filename(sample_bank_now_recording, sample_num_now_recording, final_filepath);
				if (res != FR_OK) {
					rec_state = REC_OFF;
					g_error |= SDCARD_CANT_MOUNT;
				} else {
					res = f_rename(sample_fname_now_recording, final_filepath);
					if (res == FR_OK)
						str_cpy(sample_fname_now_recording, final_filepath);
				}

				str_cpy(samples[sample_bank_now_recording][sample_num_now_recording].filename,
						sample_fname_now_recording);

				samples[sample_bank_now_recording][sample_num_now_recording].sampleSize = samplebytes_recorded;
				samples[sample_bank_now_recording][sample_num_now_recording].sampleByteSize =
					sample_bytesize_now_recording;
				samples[sample_bank_now_recording][sample_num_now_recording].sampleRate =
					global_params.record_sample_rate;
				samples[sample_bank_now_recording][sample_num_now_recording].numChannels = 2;
				samples[sample_bank_now_recording][sample_num_now_recording].blockAlign =
					2 * sample_bytesize_now_recording;
				samples[sample_bank_now_recording][sample_num_now_recording].startOfData = 44;
				samples[sample_bank_now_recording][sample_num_now_recording].PCM = 1;

				samples[sample_bank_now_recording][sample_num_now_recording].file_found = 1;

				samples[sample_bank_now_recording][sample_num_now_recording].inst_start = 0;
				samples[sample_bank_now_recording][sample_num_now_recording].inst_end = samplebytes_recorded;
				samples[sample_bank_now_recording][sample_num_now_recording].inst_size = samplebytes_recorded;
				samples[sample_bank_now_recording][sample_num_now_recording].inst_gain = 1.0f;

				enable_bank(sample_bank_now_recording);

				sample_fname_now_recording[0] = 0;
				sample_num_now_recording = 0xFF;
				sample_bank_now_recording = 0xFF;
				flags[ForceFileReload1] = 1;
				flags[ForceFileReload2] = 1;
				// Force re-calculating if Play button should be dim red:
				flags[PlaySample1Changed] = 1;
				flags[PlaySample2Changed] = 1;

				if (global_params.auto_inc_slot_num_after_rec_trig && flags[RecStartedWithTrigger]) {
					flags[RecStartedWithTrigger] = 0;
					if (sample_num_to_record_in < 9)
						sample_num_to_record_in++;
					else
						sample_num_to_record_in = 0;
				}

				if (rec_state == CLOSING_FILE)
					rec_state = REC_OFF;

				else if (rec_state == CLOSING_FILE_TO_REC_AGAIN)
					rec_state = CREATING_FILE;
			}

			break;

		case (REC_OFF):
			break;
	}
}
