/*
 * looping_delay.c

 */
#include "globals.h"
#include <audio_sdram.h>
#include <sampler.h>
#include "audio_util.h"
#include <audio_sdcard.h>

#include "adc.h"
#include "params.h"
#include "timekeeper.h"
#include "compressor.h"
#include "dig_pins.h"
#include "rgb_leds.h"

//debug:
//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];

extern uint8_t mode[NUM_CHAN+1][NUM_CHAN_MODES];
extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern float global_param[NUM_GLOBAL_PARAMS];
extern float param[NUM_CHAN][NUM_PARAMS];

extern uint8_t 	flags[NUM_FLAGS];
extern uint32_t g_error;

extern uint8_t recording_enabled;
extern uint8_t is_recording;
volatile uint8_t play_state[NUM_CHAN]={SILENT,SILENT};
extern uint8_t play_led_state[NUM_CHAN];
extern uint8_t clip_led_state[NUM_CHAN];
uint8_t ButLED_state[NUM_RGBBUTTONS];


uint8_t SAMPLESIZE=2;

extern int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
//extern int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];


//SDRAM buffer addresses for playing from sdcard
//SD Card --> playbuff_write_addr[] ... playbuff_read_addr[] --> Codec
uint32_t playbuff_write_addr[NUM_CHAN]; //not used?
uint32_t playbuff_read_addr[NUM_CHAN];

uint32_t storage_read_addr[NUM_CHAN]={0,0};


uint32_t fade_queued_dest_read_addr[NUM_CHAN];
uint32_t fade_dest_read_addr[NUM_CHAN];
float read_fade_pos[NUM_CHAN];

//SDRAM buffer address for recording to sdcard
//Codec --> rec_write_sdram_addr .... rec_read_sdram_addr --> SD Card
uint32_t rec_write_sdram_addr;
uint32_t rec_read_sdram_addr;
uint32_t rec_sdcard_addr=0;


//Circular buffers between buffer (SDRAM) and storage (SDCard)
uint8_t write_out_i=0;
uint8_t write_in_i=0;
uint32_t read_out_i[NUM_CHAN]={0,0};
uint32_t read_in_i[NUM_CHAN]={0,0};

//These should get moved to SDRAM!!
//uint16_t write_buff[WRITE_BUFF_SIZE][BUFF_LEN] __attribute__ ((section (".ccmdata")));
uint16_t write_buff[WRITE_BUFF_SIZE][BUFF_LEN];
int16_t read_buff[NUM_CHAN][READ_BUFF_SIZE][BUFF_LEN];


float decay_amp_i[NUM_CHAN];

void audio_buffer_init(void)
{
	uint32_t i;

//	if (MODE_24BIT_JUMPER)
//		SAMPLESIZE=4;
//	else
		SAMPLESIZE=2;

	for(i=0;i<NUM_CHAN;i++)
		memory_clear(i);

	rec_write_sdram_addr = SDRAM_BASE;

	if (SAMPLESIZE==2){
		//min_vol = 10;
		init_compressor(1<<15, 0.75);
	}
	else{
		//min_vol = 10 << 16;
		init_compressor(1<<31, 0.75);
	}


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
			rec_sdcard_addr=0;
			is_recording=1;
			ButLED_state[RecButtonLED]=1;
		}
	}
}


void toggle_playing(uint8_t chan)
{

	storage_read_addr[chan]= param[chan][START];

	if (play_state[chan]==SILENT || play_state[chan]==PLAY_FADEDOWN || (play_state[chan]==PLAYING && param[chan][LENGTH]>50))
	{
		read_in_i[chan]=0;
		read_out_i[chan]=0;
		play_state[chan]=PREBUFFERING;

		decay_amp_i[chan]=1.0;

		play_led_state[chan]=1;
		ButLED_state[Play1ButtonLED+chan]=1;
	}
	else if (play_state[chan]==PLAYING)
	{
		play_state[chan]=PLAY_FADEDOWN;
		decay_amp_i[chan]=0.0;

		play_led_state[chan]=0;
		ButLED_state[Play1ButtonLED+chan]=0;
	}
}


void write_buffer_to_storage(void)
{
	uint32_t err;

	//If user changed the record sample slot, start recording from the beginning of that slot
	if (flags[RecSampleChanged])
	{
		flags[RecSampleChanged]=0;
		rec_sdcard_addr = 0;
	}

	//If user changed the record bank, start recording from the beginning of that slot
	if (flags[RecBankChanged])
	{
		flags[RecBankChanged]=0;
		rec_sdcard_addr = 0;
	}

	/*** Handle SDIO write buffers (transfer SRAM to SDIO) ***/
	if (write_in_i != write_out_i)
	{

		//Write a block to the SD CARD
		//err = write_sdcard(write_buff[write_out_i], rec_sdcard_addr + (mode[REC][SAMPLE] * SAMPLE_SIZE) + (mode[REC][BANK] * BANK_SIZE));
		DEBUG1_ON;
		err = write_sdcard(write_buff[write_out_i], rec_sdcard_addr);
		DEBUG1_OFF;

		if (err==0)
		{
			if (write_out_i==(WRITE_BUFF_SIZE))
				write_out_i=0; //circular buffer
			else
				write_out_i++;

			rec_sdcard_addr++;

			//if (rec_sdcard_addr>=(BANK_SIZE){
			if (rec_sdcard_addr>=SAMPLE_SIZE)
			{
				rec_sdcard_addr=0; //reset and stop at the end of the sample
				is_recording=0;
			}
		}
		else
		{
			g_error |= WRITE_SDCARD_ERROR;
			is_recording=0;
		}


	}
}


void read_storage_to_buffer(void)
{
	uint8_t chan=0;
	uint32_t err;
	uint32_t t_addr;

//	DEBUG1_ON;

	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank1Changed]){
		flags[PlayBank1Changed]=0;
		storage_read_addr[0] = 0;
	}

	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank2Changed]){
		flags[PlayBank2Changed]=0;
		storage_read_addr[1] = 0;
	}

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample1Changed]){
		flags[PlaySample1Changed]=0;
		storage_read_addr[0] = 0;
	}

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample2Changed]){
		flags[PlaySample2Changed]=0;
		storage_read_addr[1] = 0;
	}


	for (chan=0;chan<NUM_CHAN;chan++)
	{

		if (play_state[chan] != SILENT)
		{
			//
			// See if read_in_i is at least PRE_BUFF_SIZE blocks ahead of read_out_i
			// ...If so, read another block from the sdcard and increment read_in_i
			// ...If not, then do nothing (or if we are prebuffering, start fading up)
			//
			if (diff_circular(read_in_i[chan], read_out_i[chan], (READ_BUFF_SIZE-2)) < PRE_BUFF_SIZE)
			{
				t_addr = storage_read_addr[chan] + (mode[chan][SAMPLE] * SAMPLE_SIZE) + (mode[chan][BANK]*BANK_SIZE);

				err = read_sdcard( read_buff[chan][ read_in_i[chan] ], t_addr);

//				while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}
//				rd_buff[i] = *((int16_t *)addr);

				if (err==0){
					//ToDo: Check for buffer overrun, that is, what if we fill up read_buff before we can empty it into the codec? (not likely, but would wreck havoc!)

					if (read_in_i[chan] == (READ_BUFF_SIZE-2)) read_in_i[chan]=0; //circular buffer
					else read_in_i[chan]++;

					storage_read_addr[chan]++;

					if (storage_read_addr[chan] > (SAMPLE_SIZE-4))
						//g_error|=OUT_OF_MEM;
						play_state[chan] = PLAY_FADEDOWN;

				} else {
					g_error |= READ_MEM_ERROR;
				}

			} else // we have buffered PRE_BUFF_SIZE frames
			{
				if (play_state[chan] == PREBUFFERING)
					play_state[chan] = PLAY_FADEUP;
			}
		}
	}
//	DEBUG1_OFF;

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
			playbuff_read_addr[channel] = fade_dest_read_addr[channel];

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
	int32_t v;
	uint16_t topbyte, bottombyte;
	uint8_t chan;

	static uint32_t write_buff_sample_i=0;
	static float f_t[NUM_CHAN]={0,0};


	//
	// Incoming audio
	//
	if (is_recording)
	{
		for (i=0;i<BUFF_LEN;i++)
		{
			// Split incoming stereo audio into the two channels
			//
			if (SAMPLESIZE==2){
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



			// Write the incoming audio to the write buffer
			// For now we ignore in[1]

			write_buff[write_in_i][write_buff_sample_i++] = in[0];

			if (write_buff_sample_i==BUFF_LEN)
			{
				write_buff_sample_i=0;

				if (write_in_i==(WRITE_BUFF_SIZE))  /// why -8???
					write_in_i=0; //circular buffer
				else
					write_in_i++;

				if (write_in_i == write_out_i)
				{
					g_error |= WRITE_BUFF_OVERRUN;
					is_recording=0;
				}
			}
		}
	}


	for (chan=0;chan<NUM_CHAN;chan++)
	{
		 switch (play_state[chan])
		 {

			 case (PLAY_FADEUP):
		//if (chan==0) DEBUG1_ON;
				 //fill the next buffer with a linear fade-up
				for (i=0;i<BUFF_LEN;i++)
				{
					 v=(read_buff[chan][read_out_i[chan]][i] * i)/ BUFF_LEN; //fade from 0 to read_buff
					 out[chan][i]=v;
				 }

				if (read_out_i==read_in_i)
				{
					g_error |= (READ_BUFF1_OVERRUN<<chan);
				}

				if (read_out_i[chan] == (READ_BUFF_SIZE-2))
					read_out_i[chan] = 0; //circular buffer
				else
					read_out_i[chan]++;


				play_state[chan]=PLAYING;
				break;


			 case (PLAY_FADEDOWN):

				for (i=0;i<BUFF_LEN;i++)
				{
					 v=(read_buff[chan][read_out_i[chan]][i] * (BUFF_LEN-i)) / BUFF_LEN; //fade from buffer to 0
					 out[chan][i]=v;
				}

				if (read_out_i[chan]==read_in_i[chan])
					g_error |= (READ_BUFF1_OVERRUN<<chan);


				 //? Do we need to increment read_out_i? Next is silence, so it might not matter
				if (read_out_i[chan]==(READ_BUFF_SIZE-2))
					read_out_i[chan]=0; //circular buffer
				else
					read_out_i[chan]++;

				play_led_state[chan] = 0;
				ButLED_state[Play1ButtonLED+chan] = 0;

				//end_out_ctr[chan]=4000;
				play_state[chan]=SILENT;

				break;

			 case (PLAYING):
			//if (chan==0) DEBUG2_ON;
				 if (param[chan][LENGTH]>5)
				 {
					if (decay_amp_i[chan] == 1.0f)
						f_t[chan] = 1.0f/((4096.0f-param[chan][LENGTH])*2.50f);
					else
						f_t[chan] += 1.0f/((4096.0f-param[chan][LENGTH])*2.50f);

					 decay_amp_i[chan] -= f_t[chan];

					 if (decay_amp_i[chan] <= 0.0f){
						decay_amp_i[chan] = 0;
						play_state[chan] = SILENT;
						//end_out_ctr[chan] = 4000;

						play_led_state[chan] = 0;
						ButLED_state[Play1ButtonLED+chan] = 0;
					 }

					for (i=0;i<BUFF_LEN;i++){

						v=((float)read_buff[chan][read_out_i[chan]][i]) * decay_amp_i[chan];

						out[chan][i]=v;
					}

				 } else //play the whole sample, length = full
				 {
					 for (i=0;i<BUFF_LEN;i++)
						 out[chan][i] = read_buff[chan][read_out_i[chan]][i];
				 }

				if (read_out_i[chan]==read_in_i[chan])
					g_error |= (READ_BUFF1_OVERRUN<<chan);

				if (read_out_i[chan]==(READ_BUFF_SIZE-2))
					read_out_i[chan] = 0; //circular buffer
				else
					read_out_i[chan]++;

				break;


			 case (SILENT):
			 case (PREBUFFERING):
			 //if (chan==0)  DEBUG0_ON;
				 for (i=0;i<BUFF_LEN;i++)
				 {
					 out[chan][i]=0;
				 }
				 break;

		}

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
			if (SAMPLESIZE==2)
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

	//DEBUG0_OFF;
	DEBUG1_OFF;
	DEBUG2_OFF;

}


