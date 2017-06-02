/*
 * resample.c
 *
 */

#include "globals.h"
#include "dig_pins.h"
#include "audio_sdram.h"
#include "audio_util.h"
#include "sampler.h"
#include "params.h"
#include "resample.h"

#include "circular_buffer.h"

extern uint8_t flags[NUM_FLAGS];
//extern enum g_Errors g_error;

extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

void safe_inc_play_addr(CircularBuffer* buf, uint8_t blockAlign,  uint8_t chan);

inline void safe_inc_play_addr(CircularBuffer* buf, uint8_t blockAlign, uint8_t chan)
{
	if (!i_param[chan][REV])
	{
		buf->out +=blockAlign;

		if (buf->out > buf->max) //This will not work if buf->max==0xFFFFFFFF, but luckily this is never the case with the STS!
		{
			buf->wrapping=0;
			buf->out-=buf->size;
		}
	}
	else
	{
		if ((buf->out - buf->min) < blockAlign)
		{
			buf->out += buf->size - blockAlign;
			buf->wrapping = 1;
		}
		else
			buf->out -= blockAlign;
	}
}


int16_t get_16b_sample(uint32_t addr, uint8_t stereomode);
inline int16_t get_16b_sample(uint32_t addr, uint8_t stereomode)
{
	uint32_t rd;
	int16_t a,b;

	//rd = memory_read_32bword(addr);
	//while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}
	while((FMC_Bank5_6->SDSR & FMC_FLAG_Busy) == FMC_FLAG_Busy){;}
	rd = (*((uint32_t *)addr));

	if (stereomode==STEREO_AVERAGE)
	{
		a = (int16_t)(rd >> 16);
		b = (int16_t)(rd & 0x0000FFFF);
		return ((a+b)>>1);
	}

	else if (stereomode==STEREO_RIGHT)
		return((int16_t)(rd >> 16));

	else
		return((int16_t)(rd));
}


//ToDo: Optimize this by having a different resample_read function for 16/24/32 bit...
//Optimize further by having a different routine for STEREO_AVERAGE, LEFT, and RIGHT (will save the if branch for each sample read)
//Then we also need a different memory_read_sample function and safe_inc_num_play_addr function
//This will save the "if" for every memory read, and the "while" in every safe_inc_num_play_addr

void resample_read16(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, int32_t *out)
{
	static float fractional_pos[4] = {0,0,0,0};
	static float xm1[4], x0[4], x1[4], x2[4];
	float a,b,c;
	uint32_t outpos;
	float t_out;
	uint8_t ch;

	if (stereomode==STEREO_RIGHT)
		ch = chan * 2 + 1; //1 or 3
	else
		ch = chan * 2; //0 or 2

	if (rs == 1.0)
	{
		for(outpos==0;outpos<buff_len;outpos++)
		{
			out[outpos] = get_16b_sample(buf->out, stereomode);
			CB_offset_out_address(buf, block_align, i_param[chan][REV]);
		}
		flags[PlayBuff1_Discontinuity+chan] = 1;


	} else {

		//fill the resampling buffer with three points
		if (flags[PlayBuff1_Discontinuity+chan])
		{
			flags[PlayBuff1_Discontinuity+chan] = 0;

			x0[ch] = get_16b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x1[ch] = get_16b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x2[ch] = get_16b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			fractional_pos[ch] = 0.0;
		}

		outpos=0;
		while (outpos < buff_len)
		{
			//Optimize for resample rates >= 4
			if (fractional_pos[ch] >= 4.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 4.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

				x0[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

				x1[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

				x2[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

			}
			//Optimize for resample rates >= 3
			if (fractional_pos[ch] >= 3.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 3.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x2[ch];

				x0[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

				x1[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

				x2[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

			}
			//Optimize for resample rates >= 2
			if (fractional_pos[ch] >= 2.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 2.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x1[ch];
				x0[ch] 	= x2[ch];

				x1[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

				x2[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

			}
			//Optimize for resample rates >= 1
			if (fractional_pos[ch] >= 1.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 1.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x0[ch];
				x0[ch] 	= x1[ch];
				x1[ch] 	= x2[ch];

				x2[ch] 	= get_16b_sample(buf->out, stereomode);
				safe_inc_play_addr(buf, block_align, chan);

			}

			//calculate coefficients
			a = (3 * (x0[ch]-x1[ch]) - xm1[ch] + x2[ch]) / 2;
			b = 2*x1[ch] + xm1[ch] - (5*x0[ch] + x2[ch]) / 2;
			c = (x1[ch] - xm1[ch]) / 2;

			//calculate as many fractionally placed output points as we need
			while ( fractional_pos[ch]<1.0 && outpos<buff_len)
			{
				t_out = (((a * fractional_pos[ch]) + b) * fractional_pos[ch] + c) * fractional_pos[ch] + x0[ch];
				if (t_out >= 32767.0)		out[outpos++] = 32767;
				else if (t_out <= -32767.0)	out[outpos++] = -32767;
				else						out[outpos++] = t_out;

				fractional_pos[ch] += rs;
			}
		}
	}
}


