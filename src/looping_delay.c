/*
 * looping_delay.c

 */
#include "globals.h"
#include "looping_delay.h"
#include "sdram.h"
#include "adc.h"
#include "params.h"
#include "audio_memory.h"
#include "timekeeper.h"
#include "compressor.h"
#include "dig_pins.h"


//debug:
//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];

extern uint8_t mode[NUM_CHAN][NUM_CHAN_MODES];
extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern float global_param[NUM_GLOBAL_PARAMS];


uint8_t SAMPLESIZE=2;

extern int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
//extern int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];

volatile uint32_t ping_time;
uint32_t locked_ping_time[NUM_CHAN];
volatile uint32_t divmult_time[NUM_CHAN];

uint32_t write_addr[NUM_CHAN];
uint32_t read_addr[NUM_CHAN];

const uint32_t AUDIO_MEM_BASE[NUM_CHAN] = {SDRAM_BASE, SDRAM_BASE + MEM_SIZE};

uint32_t fade_queued_dest_read_addr[NUM_CHAN];
uint32_t fade_dest_read_addr[NUM_CHAN];
float read_fade_pos[NUM_CHAN];

void audio_buffer_init(void)
{
	uint32_t i;

//	if (MODE_24BIT_JUMPER)
//		SAMPLESIZE=4;
//	else
		SAMPLESIZE=2;

	if (!ping_time)
		ping_time=0x00002000*SAMPLESIZE;


	for(i=0;i<NUM_CHAN;i++){
		memory_clear(i);
	}


	if (SAMPLESIZE==2)
	{
		//min_vol = 10;
		init_compressor(1<<15, 0.75);
	}
	else
	{
		//min_vol = 10 << 16;
		init_compressor(1<<31, 0.75);
	}


}

inline uint32_t offset_samples(uint8_t channel, uint32_t base_addr, uint32_t offset, uint8_t subtract)
{
	uint32_t t_addr;

	//convert samples to addresses
	offset*=SAMPLESIZE;

	if (subtract == 0){

		t_addr = base_addr + offset;

		while (t_addr >= (AUDIO_MEM_BASE[channel] + MEM_SIZE))
			t_addr = t_addr - MEM_SIZE;

	} else {

		t_addr = base_addr - offset;

		while (t_addr < AUDIO_MEM_BASE[channel])
			t_addr = t_addr + MEM_SIZE;

	}

	if (SAMPLESIZE==2)
		t_addr = t_addr & 0xFFFFFFFE; //addresses must be even
	else
		t_addr = t_addr & 0xFFFFFFFC; //addresses must end in 00

	return (t_addr);
}


inline uint32_t calculate_read_addr(uint8_t channel, uint32_t new_divmult_time){
	uint32_t t_read_addr;

	t_read_addr = offset_samples(channel, write_addr[channel], new_divmult_time, 1-mode[channel][REV]);
	return (t_read_addr);
}


inline uint32_t inc_addr(uint32_t addr, uint8_t channel, uint8_t direction)
{

	if (direction == 0)
	{
		addr+=SAMPLESIZE;
		if (addr >= (AUDIO_MEM_BASE[channel] + MEM_SIZE))
			addr = AUDIO_MEM_BASE[channel];
	}
	else
	{
		addr-=SAMPLESIZE;
		if (addr <= AUDIO_MEM_BASE[channel])
			addr = AUDIO_MEM_BASE[channel] + MEM_SIZE - SAMPLESIZE;
	}

	return(addr & 0xFFFFFFFE);

	//return (offset_samples(channel, addr, 1, mode[channel][REV]));
}



// Utility function to determine if address mid is in between addresses beg and end in a circular (ring) buffer.
// ToDo: draw a truth table and condense this into as few as possible boolean logic functions
// b<m
// m<e
// b<m
// b==m
// b==e
// m==e
uint8_t in_between(uint32_t mid, uint32_t beg, uint32_t end, uint8_t reverse)
{
	uint32_t t;

	if (beg==end) //zero length, trivial case
	{
		if (mid!=beg) return(0);
		else return(1);
	}

	if (reverse) { //swap beg and end if we're reversed
		t=end;
		end=beg;
		beg=t;
	}

	if (end>beg) //not wrapped around
	{
		if ((mid>=beg) && (mid<=end)) return(1);
		else return(0);

	}
	else //end has wrapped around
	{
		if ((mid<=end) || (mid>=beg)) return(1);
		else return(0);
	}
}




//If we're fading, increment the fade position
//If we've cross-faded 100%:
//	-Stop the cross-fade
//	-Set read_addr to the destination
//	-Load the next queued fade (if it exists)
inline void increment_read_fade(uint8_t channel){

	if (read_fade_pos[channel]>0.0){

		read_fade_pos[channel] += global_param[SLOW_FADE_INCREMENT];

		if (read_fade_pos[channel] > 1.0)
		{
			read_fade_pos[channel] = 0.0;

			read_addr[channel] = fade_dest_read_addr[channel];

			if (fade_queued_dest_read_addr[channel])
			{
				fade_dest_read_addr[channel] = fade_queued_dest_read_addr[channel];
				fade_queued_dest_read_addr[channel]=0;
				read_fade_pos[channel] = global_param[SLOW_FADE_INCREMENT];
			}

		}
	}
}

//returns the absolute difference between the values
uint32_t abs_diff(uint32_t a1, uint32_t a2)
{
	if (a1>a2) return (a1-a2);
	else return (a2-a1);
}



void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	static uint32_t mute_on_boot_ctr=96000;

	int32_t mainin;
	int32_t auxin;
	int32_t dummy;

	uint16_t i;
	uint16_t topbyte, bottombyte;



	for (i=0;i<(codec_BUFF_LEN/4);i++){

		// Split incoming stereo audio into the two channels: Left=>Main input (clean), Right=>Aux Input

		if (SAMPLESIZE==2){
			mainin = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+0]*/;
			dummy=*src++;

			auxin = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+2]*/;
			dummy=*src++;
		}
		else
		{
			topbyte = (uint16_t)(*src++);
			bottombyte = (uint16_t)(*src++);
			mainin = (topbyte << 16) + (uint16_t)bottombyte;

			topbyte = (uint16_t)(*src++);
			bottombyte = (uint16_t)(*src++);
			auxin = (topbyte << 16) + (uint16_t)bottombyte;
		}

		if (mute_on_boot_ctr)
		{
			mute_on_boot_ctr--;
			mainin=0;
			auxin=0;
		}

		if (global_mode[CALIBRATE])
		{
			*dst++ = CODEC_DAC_CALIBRATION_DCOFFSET[0];
			*dst++ = 0;

			*dst++ = CODEC_DAC_CALIBRATION_DCOFFSET[1];
			*dst++ = 0;
		}
		else
		{

#ifdef DEBUG_POTADC_TO_CODEC
			*dst++ = potadc_buffer[channel+0]*4;
			*dst++ = 0;

			if (TIMESW_CH1==SWITCH_CENTER) *dst++ = potadc_buffer[channel+2]*4;
			else if (TIMESW_CH1==SWITCH_UP) *dst++ = potadc_buffer[channel+4]*4;
			else *dst++ = potadc_buffer[channel+6]*4;
			*dst++ = 0;
#else
#ifdef DEBUG_CVADC_TO_CODEC
			*dst++ = potadc_buffer[channel+2]*4;
			*dst++ = 0;

			if (TIMESW_CH1==SWITCH_CENTER) *dst++ = cvadc_buffer[channel+4]*4;
			else if (TIMESW_CH1==SWITCH_UP) *dst++ = cvadc_buffer[channel+0]*4;
			else *dst++ = cvadc_buffer[channel+2]*4;
			*dst++ = 0;

#else


			if (SAMPLESIZE==2){
				//Main out
				*dst++ = mainin + CODEC_DAC_CALIBRATION_DCOFFSET[0];
				*dst++ = 0;

				//Send out
				*dst++ = auxin + CODEC_DAC_CALIBRATION_DCOFFSET[1];
				*dst++ = 0;
			}
			else
			{
				//Main out
				*dst++ = (int16_t)(mainin>>16) + (int16_t)CODEC_DAC_CALIBRATION_DCOFFSET[0];
				*dst++ = (int16_t)(mainin & 0x0000FF00);

				//Send out
				*dst++ = (int16_t)(auxin>>16) + (int16_t)CODEC_DAC_CALIBRATION_DCOFFSET[1];
				*dst++ = (int16_t)(auxin & 0x0000FF00);
			}
#endif
#endif
		}

	}


}

