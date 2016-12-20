/*
 * sampler.c

 */
#include "globals.h"
#include "audio_sdram.h"
#include "sampler.h"
#include "audio_util.h"
#include "audio_sdcard.h"

#include "adc.h"
#include "params.h"
#include "timekeeper.h"
#include "compressor.h"
#include "dig_pins.h"
#include "rgb_leds.h"
#include "resample.h"

#include "ff.h"

#include "wavefmt.h"


//
// DEBUG
//
//volatile uint32_t tdebug[256];
//uint8_t t_i;
//uint32_t debug_i=0x80000000;
//uint32_t debug_tri_dir=0;
//debug:
//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];


//
// Parameters
//
extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
//extern uint8_t 	settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

extern float global_param[NUM_GLOBAL_PARAMS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern uint8_t 	flags[NUM_FLAGS];
extern enum g_Errors g_error;

extern uint8_t recording_enabled;
extern uint8_t is_recording;
enum PlayStates play_state[NUM_PLAY_CHAN]={SILENT,SILENT};

extern uint8_t play_led_state[NUM_PLAY_CHAN];
//extern uint8_t clip_led_state[NUM_PLAY_CHAN];
uint8_t ButLED_state[NUM_RGBBUTTONS];


uint8_t SAMPLINGBYTES=2;

extern int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
//extern int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];



//
// Memory
//
extern const uint32_t AUDIO_MEM_BASE[4];

//
// SDRAM buffer addresses for playing from sdcard
// SD Card --> SDARM @play_buff_in_addr[] ... SDRAM @play_buff_out_addr[] --> Codec
//
uint32_t play_buff_in_addr[NUM_PLAY_CHAN];
uint32_t play_buff_out_addr[NUM_PLAY_CHAN];

uint32_t sample_data_played[NUM_PLAY_CHAN]={0,0};
//uint32_t sample_data_position[NUM_PLAY_CHAN]={0,0};

//
// SDRAM buffer address for recording to sdcard
// Codec --> SDRAM (@rec_buff_in_addr) .... SDRAM (@rec_buff_out_addr) --> SD Card (@rec_storage_addr)
//
uint32_t	rec_buff_in_addr;
uint32_t	rec_buff_out_addr;

uint32_t	rec_storage_addr=0;


//
// Cross-fading:
//
uint32_t fade_queued_dest_read_addr[NUM_PLAY_CHAN];
uint32_t fade_dest_read_addr[NUM_PLAY_CHAN];
float read_fade_pos[NUM_PLAY_CHAN];

float decay_amp_i[NUM_PLAY_CHAN];
float decay_inc[NUM_PLAY_CHAN]={0,0};


//
// Filesystem
//
FIL fil[NUM_PLAY_CHAN];

//
// Sample info
//
Sample samples[NUM_SAMPLES_PER_BANK];




void audio_buffer_init(void)
{
	uint32_t i;

//	if (MODE_24BIT_JUMPER)
//		SAMPLINGBYTES=4;
//	else
		SAMPLINGBYTES=2;

	memory_clear(0);
	memory_clear(1);
	memory_clear(2);
	memory_clear(3);

	rec_buff_in_addr 		= AUDIO_MEM_BASE[RECCHAN];
	rec_buff_out_addr 		= AUDIO_MEM_BASE[RECCHAN];
	play_buff_out_addr[0] 	= AUDIO_MEM_BASE[0];
	play_buff_in_addr[0] 	= AUDIO_MEM_BASE[0];
	play_buff_out_addr[1] 	= AUDIO_MEM_BASE[1];
	play_buff_in_addr[1] 	= AUDIO_MEM_BASE[1];

	flags[PlayBuff1_Discontinuity] = 1;
	flags[PlayBuff2_Discontinuity] = 1;

	if (SAMPLINGBYTES==2){
		//min_vol = 10;
		init_compressor(1<<15, 0.75);
	}
	else{
		//min_vol = 10 << 16;
		init_compressor(1<<31, 0.75);
	}


	for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
	{
		samples[i].filename="";
		samples[i].sampleSize=0;
	}

	samples[0].filename = "A440-16bmono.wav";
	samples[1].filename = "A440-A400-16bstereo.wav";
	samples[2].filename = "Risset-drum-16bmono.wav";
	samples[3].filename = "SYNTH1.WAV";
	samples[4].filename = "Risset-drum-16bmono.wav";
	samples[5].filename = "Risset-drum-16bmono.wav";
}

void toggle_recording(void)
{
	if (is_recording)
	{
		is_recording=0;
		ButLED_state[RecButtonLED]=0;

	} else
	{
		if (recording_enabled)
		{
			rec_storage_addr=0;
			is_recording=1;
			ButLED_state[RecButtonLED]=1;
		}
	}
}

//
// Load the sample header and open it into the channel's file
//
uint8_t load_sample_header(uint32_t sample, uint8_t chan)
{
	WaveHeader sample_header;
	FRESULT res;
	uint32_t br;
	uint32_t rd;
	WaveChunk chunk;

	//need to load the sample header
	if (samples[sample].sampleSize == 0)
	{

		rd = sizeof(WaveHeader);
		res = f_read(&fil[chan], &sample_header, rd, &br);

		if (res != FR_OK)
		{ //file not opened
			g_error |= FILE_READ_FAIL;
			return(res);
		}
		else if (br < rd)
		{ //file ended unexpectedly
			g_error |= FILE_WAVEFORMATERR;
			f_close(&fil[chan]);
			return(FR_INT_ERR);
		}
		else if (	sample_header.RIFFId 			!= ccRIFF		//'RIFF'
				|| sample_header.fileSize			 < 16			//File size - 8
				|| sample_header.WAVEId 			!= ccWAVE		//'WAVE'
				|| sample_header.fmtId 				!= ccFMT		//'fmt '
				|| sample_header.fmtSize 			 < 16			//Format Chunk size
				|| sample_header.audioFormat		!= 0x0001		//PCM format
				|| sample_header.numChannels 		 > 0x0002		//Stereo or mono allowed
				|| sample_header.sampleRate 	 	> 48000		//Between 8k and 48k sampling rate allowed
				|| sample_header.sampleRate		 	< 8000
				|| (sample_header.bitsPerSample		!= 8 		//Only 8,16,24,and 32 bit samplerate allowed
					&& sample_header.bitsPerSample	!= 16
					&& sample_header.bitsPerSample	!= 24
					&& sample_header.bitsPerSample	!= 32)
					//sample_header.blockAlign
					//sample_header.byteRate
			)
		{ //first header error
			g_error |= FILE_WAVEFORMATERR;
			f_close(&fil[chan]);
			return(FR_INT_ERR);
		}

		else //no file or first header error
		{
			chunk.chunkId = 0;
			rd = sizeof(WaveChunk);

			while (chunk.chunkId != ccDATA)
			{
				res = f_read(&fil[chan], &chunk, rd, &br);

				if (res != FR_OK) {
					g_error |= FILE_READ_FAIL;
					f_close(&fil[chan]);
					return(FR_INT_ERR);
				}
				if (br < rd) {
					g_error |= FILE_WAVEFORMATERR;
					f_close(&fil[chan]);
					return(FR_INT_ERR);
				}

				//fast-forward to the next chunk
				if (chunk.chunkId != ccDATA)
					f_lseek(&fil[chan], f_tell(&fil[chan]) + chunk.chunkSize);

				//Set the sampleSize as defined in the chunk
				else
				{
					samples[sample].sampleSize = chunk.chunkSize;
					samples[sample].sampleBitSize = sample_header.bitsPerSample;
					samples[sample].sampleRate = sample_header.sampleRate;
					samples[sample].numChannels = sample_header.numChannels;
					samples[sample].blockAlign = sample_header.numChannels * sample_header.bitsPerSample/8;
					samples[sample].startOfData = f_tell(&fil[chan]);


				} //else chunk
			} //while chunk
		}//no file error
	}//if samples[].samplesize==0

	return(FR_OK);
}

void toggle_playing(uint8_t chan)
{
	uint8_t sample;
	FRESULT res;


	sample_data_played[chan] = 0;

	//Start playing, or re-start if we have a short length
	if (	play_state[chan]==SILENT
			|| play_state[chan]==PLAY_FADEDOWN
			|| play_state[chan]==PLAYING_PERC)
	{
		play_buff_in_addr[chan] = play_buff_out_addr[chan];
		flags[PlayBuff1_Discontinuity+chan] = 1;


		sample = i_param[chan][SAMPLE];

		f_close(&fil[chan]);

		DEBUG1_ON; //750us at -O0
		res = f_open(&fil[chan], samples[sample].filename, FA_READ);
		DEBUG1_OFF;

		if (res != FR_OK)
		{
			g_error |= FILE_OPEN_FAIL;
			return;
		}

		DEBUG2_ON;
		res = load_sample_header( i_param[chan][SAMPLE], chan );
		DEBUG2_OFF;

		if (res == FR_OK)
		{
			//seek the beginning of data
			DEBUG3_ON;
			f_lseek(&fil[chan], samples[sample].startOfData); // ToDo: add +LENGTH param (as a function of percentage of samples[sample].sampleSize)
			DEBUG3_OFF;

		}


		//loads the sample data, and opens the file into fil[chan]
		//improvement idea #1: load all 19 samples in the current bank, opening their files. Then we when play, set the channel's file to the sample file (does this work for two channels playing the same sample?)
		//#2: load all 19 sample headers and store info into samples[], then close the files. When we play, open the file and jump right to startOfData.
		//??? can we open the same file twice (RO)?


		decay_amp_i[chan]=1.0;
		decay_inc[chan]=0.0;

		play_led_state[chan]=1;
		ButLED_state[Play1ButtonLED+chan]=1;

		play_state[chan]=PREBUFFERING;
	}

	//Stop it if we're playing a full sample
	else if (play_state[chan]==PLAYING)
	{
		play_state[chan]=PLAY_FADEDOWN;
		f_close(&fil[chan]);
//		decay_amp_i[chan]=0.0;

		play_led_state[chan]=0;
		ButLED_state[Play1ButtonLED+chan]=0;
	}
}


void write_buffer_to_storage(void)
{
	uint32_t err;
	uint32_t start_of_sample_addr;
	uint8_t addr_exceeded;
	//uint32_t i;
	//int32_t t_buff32[BUFF_LEN];
	int16_t t_buff16[BUFF_LEN];


	//If user changed the record sample slot, start recording from the beginning of that slot
	if (flags[RecSampleChanged])
	{
		flags[RecSampleChanged]=0;
		rec_storage_addr = 0;
	}

	//If user changed the record bank, start recording from the beginning of that slot
	if (flags[RecBankChanged])
	{
		flags[RecBankChanged]=0;
		rec_storage_addr = 0;
	}



	// Handle write buffers (transfer SDRAM to SD card)


	if (rec_buff_out_addr != rec_buff_in_addr)
	{

		addr_exceeded = memory_read16(&rec_buff_out_addr, RECCHAN, t_buff16, BUFF_LEN, rec_buff_in_addr, 0);

		// Stop the circular buffer if we wrote it all out to storage
		if (addr_exceeded) rec_buff_out_addr = rec_buff_in_addr;

		start_of_sample_addr = (i_param[REC][SAMPLE] * SAMPLE_SIZE) + (i_param[REC][BANK] * BANK_SIZE);

		err = write_sdcard((uint16_t *)t_buff16, start_of_sample_addr + rec_storage_addr);

		if (err==0)
		{
			rec_storage_addr++;

			//if (rec_storage_addr>=(BANK_SIZE){
			if (rec_storage_addr>=SAMPLE_SIZE)
			{
				rec_storage_addr=0; //reset and stop at the end of the sample
				is_recording=0;
				ButLED_state[RecButtonLED]=0;
			}
		}
		else
		{
			g_error |= WRITE_SDCARD_ERROR;
			is_recording=0;
			ButLED_state[RecButtonLED]=0;
		}


	}

}

void read_storage_to_buffer(void)
{
	uint8_t chan=0;
	uint32_t err;
	//uint32_t i;
	int16_t t_buff16[BUFF_LEN];
	//int32_t t_buff32[BUFF_LEN];
	//uint32_t start_of_sample_addr;

	FRESULT res;
	uint32_t br;
	uint32_t rd;

	//
	//Reset to beginning of sample if we changed sample or bank
	//ToDo: Move this to a routine that checks for flags, and call it before read_storage_to_buffer() is called
	//
	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank1Changed]){
		flags[PlayBank1Changed]=0;
		sample_data_played[0] = 0;
	}

	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank2Changed]){
		flags[PlayBank2Changed]=0;
		sample_data_played[1] = 0;
	}

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample1Changed]){
		flags[PlaySample1Changed]=0;
		sample_data_played[0] = 0;

	}

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample2Changed]){
		flags[PlaySample2Changed]=0;
		sample_data_played[1] = 0;
	}


	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{

		if (play_state[chan] != SILENT && play_state[chan] != PLAY_FADEDOWN)
		{

			// If our "in" pointer is not far enough ahead of the "out" pointer, then we need to buffer some more:
			if (diff_circular(play_buff_in_addr[chan], play_buff_out_addr[chan], MEM_SIZE) < (PRE_BUFF_SIZE*256))
			{

				if (sample_data_played[0] == samples[ i_param[chan][SAMPLE] ].sampleSize) //then we've buffered the entire file
				{
					play_state[chan] = PLAY_FADEDOWN; //this is premature!
					f_close(&fil[chan]);

					//actually we shoudl set a flag here that the sample is fully buffered
					//then set play_state to FADEDOWN in the playback routine

				}
				else if (sample_data_played[0] > samples[ i_param[chan][SAMPLE] ].sampleSize) //we read too much data somehow
					g_error |= FILE_WAVEFORMATERR;

				else
				{
					rd = samples[ i_param[chan][SAMPLE] ].sampleSize -  sample_data_played[chan];
					if (rd > 512) rd = 512;

					res = f_read(&fil[chan], t_buff16, rd, &br);


					if (res != FR_OK)
						g_error |= FILE_READ_FAIL;
					else
					{
						if (rd<br)
							g_error |= FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read

						err = memory_write16(&(play_buff_in_addr[chan]), chan, t_buff16, rd>>1, play_buff_out_addr[chan], 0);

						if (err) g_error |= READ_BUFF1_OVERRUN*(1+chan);
						sample_data_played[chan] += br;

					}

				}

			}
			else //Otherwise, we've buffered enough
			{
				if (play_state[chan] == PREBUFFERING)
					play_state[chan] = PLAY_FADEUP;
			}

		}
	}


}


//If we're fading, increment the fade position
//If we've cross-faded 100%:
//	-Stop the cross-fade
//	-Set read_addr to the destination
//	-Load the next queued fade (if it exists)
void increment_read_fade(uint8_t channel)
{

	if (read_fade_pos[channel]>0.0)
	{
		read_fade_pos[channel] += global_param[SLOW_FADE_INCREMENT];

		//If we faded 100%, stop fading by setting read_fade_pos to 0
		if (read_fade_pos[channel] > 1.0)
		{
			read_fade_pos[channel] = 0.0;
			play_buff_out_addr[channel] = fade_dest_read_addr[channel];
			flags[PlayBuff1_Discontinuity+channel] = 1;

			//Load the next queued fade (if it exists)
			if (fade_queued_dest_read_addr[channel])
			{
				fade_dest_read_addr[channel] = fade_queued_dest_read_addr[channel];
				fade_queued_dest_read_addr[channel]=0;
				read_fade_pos[channel] = global_param[SLOW_FADE_INCREMENT];
			}
		}
	}
}


void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	int32_t in[2];
	int32_t out[2][BUFF_LEN];
	int32_t dummy;

	uint16_t i;
	uint16_t topbyte, bottombyte;
	uint8_t chan;
	uint8_t overrun;

	//int32_t t_buff[BUFF_LEN];
	//static uint32_t write_buff_sample_i=0;

	//Resampling:
	static float resampling_fpos[NUM_PLAY_CHAN]={0.0f, 0.0f};
	float rs;


	//
	// Incoming audio
	//
	// Dump BUFF_LEN samples of the rx buffer from codec (src) into t_buff
	// Then write t_buff to sdram at rec_buff_in_addr
	//
	if (is_recording)
	{
		for (i=0;i<BUFF_LEN;i++)
		{
			//
			// Split incoming stereo audio into the two channels
			//
			if (SAMPLINGBYTES==2)
			{
				in[0] = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+0]*/;
				dummy=*src++;

				in[1] = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+2]*/;
				dummy=*src++;
			}
			else
			{
				topbyte = (uint16_t)(*src++);
				bottombyte = (uint16_t)(*src++);
				in[0] = (topbyte << 16) + (uint16_t)bottombyte;

				topbyte = (uint16_t)(*src++);
				bottombyte = (uint16_t)(*src++);
				in[1] = (topbyte << 16) + (uint16_t)bottombyte;
			}


			//t_buff[i] = in[0];

			overrun = memory_write(&rec_buff_in_addr, RECCHAN, &(in[0]), 1, rec_buff_out_addr, 0);
			if (overrun) break;
		}

		//overrun = memory_write(&rec_buff_in_addr, RECCHAN, t_buff, BUFF_LEN, rec_buff_out_addr, 0);

		if (overrun)
		{
			g_error |= WRITE_BUFF_OVERRUN;
			is_recording=0;
			ButLED_state[RecButtonLED]=0;
		}
	}



	//
	// Outgoing audio
	//
	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{

		// Fill buffer with silence
		if (play_state[chan] == PREBUFFERING || play_state[chan] == SILENT)
		{
			 for (i=0;i<BUFF_LEN;i++)
				 out[chan][i]=0;
		}
		else
		{

			//Read from SDRAM into out[chan][]

			rs = f_param[chan][PITCH];

			resample_read(rs, play_buff_out_addr, play_buff_in_addr[chan], BUFF_LEN, chan, resampling_fpos, out[chan]);

			 switch (play_state[chan])
			 {

				 case (PLAY_FADEUP):

					for (i=0;i<BUFF_LEN;i++)
						out[chan][i] = ((float)out[chan][i] * (float)i / (float)BUFF_LEN);

					play_state[chan]=PLAYING;
					break;


				 case (PLAY_FADEDOWN):

					for (i=0;i<BUFF_LEN;i++)
						out[chan][i] = ((float)out[chan][i] * (float)(BUFF_LEN-i) / (float)BUFF_LEN);

					play_led_state[chan] = 0;
					ButLED_state[Play1ButtonLED+chan] = 0;

					//end_out_ctr[chan]=4000;
					play_state[chan]=SILENT;

					break;

				 case (PLAYING):
						if (f_param[chan][LENGTH]>0.5) //Play a longer portion of the sample, and go to PLAY_FADEDOWN when done
						{
//							if ( play_storage_addr[chan] >= (uint32_t)((float)SAMPLE_SIZE * f_param[chan][LENGTH]) )

							//We really should also check here if the sample is indeed fully buffered
							if (diff_circular(play_buff_in_addr[chan], play_buff_out_addr[chan], MEM_SIZE) < (2*256))
								play_state[chan] = PLAY_FADEDOWN;

						}
						else
							play_state[chan] = PLAYING_PERC;
				 	 	break;

				 case (PLAYING_PERC):

					//if ((play_state[chan] == PLAYING_PERC) || f_param[chan][LENGTH]<=0.5) //Play a short portion of the sample, with an envelope on the whole thing
					//{
						play_state[chan] = PLAYING_PERC;

						decay_inc[chan] += 1.0f/((f_param[chan][LENGTH])*2621440.0f);


						for (i=0;i<BUFF_LEN;i++)
						{
							decay_amp_i[chan] -= decay_inc[chan];
							if (decay_amp_i[chan] < 0.0f) decay_amp_i[chan] = 0.0f;

							out[chan][i] = ((float)out[chan][i]) * decay_amp_i[chan];
						}

						if (decay_amp_i[chan] <= 0.0f)
						{
							decay_amp_i[chan] = 0.0f;
							play_state[chan] = SILENT;
							//end_out_ctr[chan] = 4000;

							play_led_state[chan] = 0;
							ButLED_state[Play1ButtonLED+chan] = 0;
						}


					//}
					break;

				 default:
					 break;

			 }//switch play_state

		} //if play_state

	}

	for (i=0;i<BUFF_LEN;i++)
	{

#ifndef DEBUG_ADC_TO_CODEC

		if (global_mode[CALIBRATE])
		{
			*dst++ = CODEC_DAC_CALIBRATION_DCOFFSET[0];
			*dst++ = 0;

			*dst++ = CODEC_DAC_CALIBRATION_DCOFFSET[1];
			*dst++ = 0;
		}
		else
		{
			if (SAMPLINGBYTES==2)
			{

				//L out
				*dst++ = out[0][i] + CODEC_DAC_CALIBRATION_DCOFFSET[0];
				*dst++ = 0;

				//R out
				*dst++ = out[1][i] + CODEC_DAC_CALIBRATION_DCOFFSET[1];
				*dst++ = 0;
			}
			else
			{
				//L out
				*dst++ = (int16_t)(out[0][i]>>16) + (int16_t)CODEC_DAC_CALIBRATION_DCOFFSET[0];
				*dst++ = (int16_t)(out[0][i] & 0x0000FF00);

				//R out
				*dst++ = (int16_t)(out[1][i]>>16) + (int16_t)CODEC_DAC_CALIBRATION_DCOFFSET[1];
				*dst++ = (int16_t)(out[1][i] & 0x0000FF00);
			}
		}

#else
		*dst++ = potadc_buffer[channel+2]*4;
		*dst++ = 0;

		if (STEREOSW==SWITCH_CENTER) *dst++ = cvadc_buffer[channel+4]*4;
		else if (STEREOSW==SWITCH_LEFT) *dst++ = cvadc_buffer[channel+0]*4;
		else *dst++ = cvadc_buffer[channel+2]*4;
		*dst++ = 0;

#endif
	}


}


