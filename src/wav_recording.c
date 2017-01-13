#include "globals.h"
#include "params.h"
#include "wavefmt.h"
#include "timekeeper.h"
#include "audio_sdram.h"
#include "ff.h"
#include "sampler.h"
#include "circular_buffer.h"
#include "file_util.h"
#include "rgb_leds.h"
#include "audio_util.h"
#include "wav_recording.h"
#include "sts_filesystem.h"



extern volatile uint32_t sys_tmr;
extern enum g_Errors g_error;

extern const uint32_t AUDIO_MEM_BASE[4];
extern uint8_t SAMPLINGBYTES;

extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern uint8_t 	flags[NUM_FLAGS];

extern enum PlayLoadTriage play_load_triage;

extern Sample samples[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

#define WRITE_BLOCK_SIZE 8192

//
// SDRAM buffer address for recording to sdcard
// Codec --> SDRAM (@rec_buff->in) .... SDRAM (@rec_buff->out) --> SD Card:recfil@rec_storage_addr
//

CircularBuffer srec_buff;
CircularBuffer* rec_buff;

enum RecStates	rec_state;
uint32_t		samplebytes_recorded;
uint8_t			sample_num_now_recording;
uint8_t			sample_bank_now_recording;
char 			sample_fname_now_recording[255];

uint8_t 		recording_enabled;

FIL 			recfil;


void init_rec_buff(void)
{
	rec_buff = &srec_buff;
	rec_buff->in 		= AUDIO_MEM_BASE[REC_CHAN];
	rec_buff->out 		= AUDIO_MEM_BASE[REC_CHAN];
	rec_buff->min		= AUDIO_MEM_BASE[REC_CHAN];
	rec_buff->max		= AUDIO_MEM_BASE[REC_CHAN] + MEM_SIZE;
	rec_buff->size		= MEM_SIZE;
	rec_buff->wrapping	= 0;


}


void toggle_recording(void)
{
	if (rec_state==RECORDING || rec_state==CREATING_FILE)
	{
		rec_state=CLOSING_FILE;

	} else
	{
		if (recording_enabled)
		{
			CB_init(rec_buff, 0);
			rec_state = CREATING_FILE;
		}
	}
}

void record_audio_to_buffer(int16_t *src)
{
	uint32_t i;
	int16_t t_buff16[HT16_BUFF_LEN<<1]; //stereo
	uint8_t overrun;

	//convenience vars:
	uint16_t topbyte, bottombyte;
	int32_t dummy;

	//
	// Dump BUFF_LEN samples of the rx buffer from codec (src) into t_buff
	// Then write t_buff to sdram at rec_buff
	//
	if (rec_state==RECORDING || rec_state==CREATING_FILE)
	{
		for (i=0; i<HT16_BUFF_LEN; i++)
		{
			//
			// Split incoming stereo audio into the two channels
			//
			if (SAMPLINGBYTES==2)
			{
				t_buff16[i*2] = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+0]*/;
				dummy=*src++;

				t_buff16[i*2+1] = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+2]*/;
				dummy=*src++;
			}
			else
			{
				topbyte 		= (uint16_t)(*src++);
				bottombyte		= (uint16_t)(*src++);
				t_buff16[i*2] 	= (topbyte << 16) + (uint16_t)bottombyte;

				topbyte 		= (uint16_t)(*src++);
				bottombyte 		= (uint16_t)(*src++);
				t_buff16[i*2+1] = (topbyte << 16) + (uint16_t)bottombyte;

			}

		}

		overrun = memory_write16_cb(rec_buff, t_buff16, HT16_BUFF_LEN, 0);

		if (overrun)
		{
			g_error |= WRITE_BUFF_OVERRUN;
			rec_state=REC_PAUSED;
		}
	}

}




void write_buffer_to_storage(void)
{
	int16_t t_buff16[WRITE_BLOCK_SIZE>>1];
	uint32_t buffer_lead;
	uint32_t addr_exceeded;
	uint32_t written;
	uint32_t data[1];
	uint32_t samplenum;
	uint32_t chan;

	FRESULT res;
	DIR dir;
	char path[10];


//	WaveHeader wavh;
	WaveHeaderAndChunk whac;

	uint32_t sz;

	//If user changed the record sample slot, start recording from the beginning of that slot
	if (flags[RecSampleChanged])
	{
		flags[RecSampleChanged]=0;
	}

	//If user changed the record bank, start recording from the beginning of that slot
	if (flags[RecBankChanged])
	{
		flags[RecBankChanged]=0;
	}



	// Handle write buffers (transfer SDRAM to SD card)
	switch (rec_state)
	{
		case (CREATING_FILE):	//first time, create a new file
			if (recfil.obj.fs!=0)
			{
				rec_state = CLOSING_FILE;
				//f_close(&recfil);
				//f_sync(&recfil);
			}

			sample_num_now_recording = i_param[REC_CHAN][SAMPLE];
			sample_bank_now_recording = i_param[REC_CHAN][BANK];

			sz = bank_to_color(sample_bank_now_recording, path);
			res = f_opendir(&dir, path);
			//If that's not found, create it
			if (res==FR_NO_PATH)
				res = f_mkdir(path);


			sz = bank_to_color(sample_bank_now_recording, sample_fname_now_recording);
			sample_fname_now_recording[sz++] = '/';
			sz += intToStr(sample_num_now_recording, &(sample_fname_now_recording[sz]), 2);
			sample_fname_now_recording[sz++] = '-';
			sz += intToStr(sys_tmr, &(sample_fname_now_recording[sz]), 0);
			sample_fname_now_recording[sz++] = '.';
			sample_fname_now_recording[sz++] = 'w';
			sample_fname_now_recording[sz++] = 'a';
			sample_fname_now_recording[sz++] = 'v';
			sample_fname_now_recording[sz++] = 0;






			res = f_open(&recfil, sample_fname_now_recording, FA_WRITE | FA_CREATE_NEW);
			if (res!=FR_OK)		{g_error |= FILE_REC_OPEN_FAIL; check_errors(); break;}

			create_waveheader(&whac.wh);
			create_chunk(ccDATA, 0, &whac.wc);

			sz = sizeof(WaveHeaderAndChunk);

			res = f_write(&recfil, &whac.wh, sz, &written);
			if (res!=FR_OK)		{g_error |= FILE_WRITE_FAIL; check_errors(); break;}
			if (sz!=written)	{g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors(); break;}

			samplebytes_recorded = 0;

			//f_close(&recfil);
			f_sync(&recfil);

			enable_bank(sample_bank_now_recording);

			rec_state=RECORDING;
		break;


		case (RECORDING):
		//read a block from rec_buff->out
			if (play_load_triage==0)
			{
				//buffer_lead = diff_wrap(rec_buff->in, rec_buff->out,  rec_buff->wrapping, MEM_SIZE);
				buffer_lead = CB_distance(rec_buff, 0);

				if (buffer_lead > WRITE_BLOCK_SIZE)
				{

					addr_exceeded = memory_read16_cb(rec_buff, t_buff16, WRITE_BLOCK_SIZE>>1, 0);

					if (!addr_exceeded)
					{
						sz = WRITE_BLOCK_SIZE;
						res = f_write(&recfil, t_buff16, sz, &written);
						f_sync(&recfil);
						if (res!=FR_OK)		{g_error |= FILE_WRITE_FAIL; check_errors(); break;}
						if (sz!=written)	{g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors(); break;}
						samplebytes_recorded += written;

					}
					else
					{
						rec_state = CLOSING_FILE;
						//ButLED_state[RecButtonLED] = 2; //flash it?
					}
				}
			}

		break;

		case (CLOSING_FILE):
			//See if we have more in the buffer to write
			//buffer_lead = diff_wrap(rec_buff->in, rec_buff->out,  rec_buff->wrapping, MEM_SIZE);
			buffer_lead = CB_distance(rec_buff, 0);

			if (buffer_lead)
			{
				//Write out remaining data in buffer, one WRITE_BLOCK_SIZE at a time
				if (buffer_lead > WRITE_BLOCK_SIZE) buffer_lead = WRITE_BLOCK_SIZE;

				addr_exceeded = memory_read16_cb(rec_buff, t_buff16, buffer_lead>>1, 0);

				if (!addr_exceeded)
				{
					res = f_write(&recfil, t_buff16, buffer_lead, &written);
					f_sync(&recfil);
					if (res!=FR_OK)				{g_error |= FILE_WRITE_FAIL; check_errors(); break;}
					if (written!=buffer_lead)	{g_error |= FILE_UNEXPECTEDEOF_WRITE; check_errors(); break;}

					samplebytes_recorded += written;

				}
				else
				{
					g_error = 999; //math error
					check_errors();
				}
			}
			else //Write the file size into the header, and close the file
			{

				 //file size - 8
				data[0] = samplebytes_recorded+sizeof(WaveHeader)+sizeof(WaveChunk)-8;
				res = f_lseek(&recfil, 4);
				res = f_write(&recfil, data, 4, &written);
				f_sync(&recfil);

				//data chunk size
				data[0] = samplebytes_recorded;
				res = f_lseek(&recfil, 40);
				res = f_write(&recfil, data, 4, &written);
				f_close(&recfil);

				rec_state=REC_OFF;

				for (chan=0;chan<NUM_PLAY_CHAN;chan++)
				{
					if (i_param[chan][BANK] == sample_bank_now_recording)
					{
						str_cpy(samples[chan][sample_num_now_recording].filename, sample_fname_now_recording);
						samples[chan][sample_num_now_recording].sampleSize = samplebytes_recorded;
						samples[chan][sample_num_now_recording].sampleByteSize = 2;
						samples[chan][sample_num_now_recording].sampleRate = 44100;
						samples[chan][sample_num_now_recording].numChannels = 2;
						samples[chan][sample_num_now_recording].blockAlign = 4;
						samples[chan][sample_num_now_recording].startOfData = 44;

					}
				}

				sample_fname_now_recording[0] = 0;
				sample_num_now_recording = 0;
				sample_num_now_recording = 0;
			}

		break;


		case (REC_OFF):
			if (recfil.obj.fs!=0)
			{
				//rec_state = CLOSING_FILE;
				f_close(&recfil);
			}
		break;

		case (REC_PAUSED):
		break;

	}


}
