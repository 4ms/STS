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


extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 	settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

extern float global_param[NUM_GLOBAL_PARAMS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern uint8_t 	flags[NUM_FLAGS];
extern uint32_t g_error;

extern uint8_t recording_enabled;
extern uint8_t is_recording;
volatile uint8_t play_state[NUM_PLAY_CHAN]={SILENT,SILENT};

extern uint8_t play_led_state[NUM_PLAY_CHAN];
//extern uint8_t clip_led_state[NUM_PLAY_CHAN];
uint8_t ButLED_state[NUM_RGBBUTTONS];


uint8_t SAMPLINGBYTES=2;

extern int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
//extern int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];

extern const uint32_t AUDIO_MEM_BASE[4];

//
//SDRAM buffer addresses for playing from sdcard
//SD Card --> SDARM @play_buff_in_addr[] ... SDRAM @play_buff_out_addr[] --> Codec
//
uint32_t play_buff_in_addr[NUM_PLAY_CHAN];
uint32_t play_buff_out_addr[NUM_PLAY_CHAN];

uint32_t play_storage_addr[NUM_PLAY_CHAN]={0,0};


//
// SDRAM buffer address for recording to sdcard
// Codec --> SDRAM (@rec_buff_in_addr) .... SDRAM (@rec_buff_out_addr) --> SD Card (@rec_storage_addr)
//
uint32_t	rec_buff_in_addr;
uint32_t	rec_buff_out_addr;

uint32_t	rec_storage_addr=0;


//
//Cross-fading:
//
uint32_t fade_queued_dest_read_addr[NUM_PLAY_CHAN];
uint32_t fade_dest_read_addr[NUM_PLAY_CHAN];
float read_fade_pos[NUM_PLAY_CHAN];

float decay_amp_i[NUM_PLAY_CHAN];


//debug:
//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];


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

	rec_buff_in_addr = AUDIO_MEM_BASE[RECCHAN];
	rec_buff_out_addr = AUDIO_MEM_BASE[RECCHAN];

	if (SAMPLINGBYTES==2){
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
			rec_storage_addr=0;
			is_recording=1;
			ButLED_state[RecButtonLED]=1;
		}
	}
}


void toggle_playing(uint8_t chan)
{

	play_storage_addr[chan]= SAMPLE_SIZE * f_param[chan][START];

	//Start playing, or re-start if we have a short length
	if (	play_state[chan]==SILENT
			|| play_state[chan]==PLAY_FADEDOWN
			|| (play_state[chan]==PLAYING && f_param[chan][LENGTH]>0.01)
		)
	{
		play_state[chan]=PREBUFFERING;

		decay_amp_i[chan]=1.0;

		play_led_state[chan]=1;
		ButLED_state[Play1ButtonLED+chan]=1;
	}

	//Stop it if we're playing
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
	uint32_t start_of_sample_addr;
	uint8_t addr_exceeded;
	uint32_t i;
	int32_t t_buff32[BUFF_LEN];
	uint16_t t_buff16[BUFF_LEN];


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

		addr_exceeded = memory_read(&rec_buff_out_addr, RECCHAN, t_buff32, BUFF_LEN, rec_buff_in_addr, 0);

		// Stop the circular buffer if we wrote it all out to storage
		if (addr_exceeded) rec_buff_out_addr = rec_buff_in_addr;

		//convert i32 to u16 (we shouldn't have to waste time doing this manually!)
		for (i=0;i<BUFF_LEN;i++) t_buff16[i] = t_buff32[i];

		start_of_sample_addr = (i_param[REC][SAMPLE] * SAMPLE_SIZE) + (i_param[REC][BANK] * BANK_SIZE);

		err = write_sdcard(t_buff16, start_of_sample_addr + rec_storage_addr);

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
	uint32_t i;
	uint16_t t_buff16[BUFF_LEN];
	int32_t t_buff32[BUFF_LEN];
	uint32_t start_of_sample_addr;

//	DEBUG1_ON;

	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank1Changed]){
		flags[PlayBank1Changed]=0;
		play_storage_addr[0]= SAMPLE_SIZE * f_param[0][START];
	}

	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank2Changed]){
		flags[PlayBank2Changed]=0;
		play_storage_addr[1]= SAMPLE_SIZE * f_param[1][START];
	}

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample1Changed]){
		flags[PlaySample1Changed]=0;
		play_storage_addr[0]= SAMPLE_SIZE * f_param[0][START];
	}

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample2Changed]){
		flags[PlaySample2Changed]=0;
		play_storage_addr[1]= SAMPLE_SIZE * f_param[1][START];
	}


	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{

		if (play_state[chan] != SILENT)
		{

			// If our "in" pointer is not far enough ahead of the "out" pointer, then we need to buffer some more:
			if (diff_circular(play_buff_in_addr[chan], play_buff_out_addr[chan], MEM_SIZE) < (PRE_BUFF_SIZE*256))
			{
				start_of_sample_addr = (i_param[chan][SAMPLE] * SAMPLE_SIZE) + (i_param[chan][BANK]*BANK_SIZE);

				err = read_sdcard( t_buff16, start_of_sample_addr + play_storage_addr[chan]);

				if (err==0){

					//convert i32 to u16 (we shouldn't have to waste time doing this manually!)
					for (i=0;i<BUFF_LEN;i++) t_buff32[i] = t_buff16[i];

					err = memory_write(&(play_buff_in_addr[chan]), chan, t_buff32, BUFF_LEN, play_buff_out_addr[chan], 0);

					play_storage_addr[chan]++;

					if (play_storage_addr[chan] > (SAMPLE_SIZE-4))
						//g_error|=OUT_OF_MEM;
						play_state[chan] = PLAY_FADEDOWN;

				} else {
					g_error |= READ_MEM_ERROR;
				}


			} else //Otherwise, we've buffered enough
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
			play_buff_out_addr[channel] = fade_dest_read_addr[channel];

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
	uint8_t overrun;

	int32_t t_buff[BUFF_LEN];
	static uint32_t write_buff_sample_i=0;
	static float f_t[NUM_PLAY_CHAN]={0,0};


	//
	// Incoming audio
	//
	// Dump BUFF_LEN samples of the rx buffer from codec (src) into t_buff
	// Then write t_buff to sdram at rec_buff_in_addr
	//
	if (is_recording)
	{
		DEBUG1_ON;
		//every 5.3us for 0.248us
		for (i=0;i<BUFF_LEN;i++)
		{
			// Split incoming stereo audio into the two channels
			//
			if (SAMPLINGBYTES==2){
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


			t_buff[i] = in[0];

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
		DEBUG1_OFF;
	}



	//
	// Outgoing audio
	//
	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{
		 switch (play_state[chan])
		 {

			 case (PLAY_FADEUP):
				 //fill the next buffer with a linear fade-up
				overrun = memory_read(&(play_buff_out_addr[chan]), chan, out[chan], BUFF_LEN, play_buff_in_addr[chan], 0);
			  	for (i=0;i<BUFF_LEN;i++) out[chan][i] = ((float)out[chan][i] * (float)i / (float)BUFF_LEN);

				 if (overrun)
				 {
					 g_error |= (READ_BUFF1_OVERRUN<<chan);
				 }


				play_state[chan]=PLAYING;
				break;


			 case (PLAY_FADEDOWN):
				overrun = memory_read(&(play_buff_out_addr[chan]), chan, out[chan], BUFF_LEN, play_buff_in_addr[chan], 0);
			  	for (i=0;i<BUFF_LEN;i++) out[chan][i] = ((float)out[chan][i] * (float)(BUFF_LEN-i) / (float)BUFF_LEN);
				if (overrun)
				{
					g_error |= (READ_BUFF1_OVERRUN<<chan);
				}

				play_led_state[chan] = 0;
				ButLED_state[Play1ButtonLED+chan] = 0;

				//end_out_ctr[chan]=4000;
				play_state[chan]=SILENT;

				break;

			 case (PLAYING):
				 if (f_param[chan][LENGTH]>0.002)
				 {
					if (decay_amp_i[chan] == 1.0f)
						f_t[chan] = 1.0f/((1.0f-f_param[chan][LENGTH])*10240.0f);
					else
						f_t[chan] += 1.0f/((1.0f-f_param[chan][LENGTH])*10240.0f);

					decay_amp_i[chan] -= f_t[chan];

					if (decay_amp_i[chan] <= 0.0f)
					{
						decay_amp_i[chan] = 0;
						play_state[chan] = SILENT;
						//end_out_ctr[chan] = 4000;

						play_led_state[chan] = 0;
						ButLED_state[Play1ButtonLED+chan] = 0;
					}

					overrun = memory_read(&(play_buff_out_addr[chan]), chan, out[chan], BUFF_LEN, play_buff_in_addr[chan], 0);

					for (i=0;i<BUFF_LEN;i++) out[chan][i] = (float)out[chan][i] * decay_amp_i[chan];


				 } else //play the whole sample, length = full
				 {
					 overrun = memory_read(&(play_buff_out_addr[chan]), chan, out[chan], BUFF_LEN, play_buff_in_addr[chan], 0);
				 }

				if (overrun)
				{
					g_error |= (READ_BUFF1_OVERRUN<<chan);
				}

				break;


			 case (SILENT):
			 case (PREBUFFERING):
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


