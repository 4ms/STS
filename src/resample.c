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
#include "sdram_driver.h"

#include "circular_buffer.h"

extern uint8_t flags[NUM_FLAGS];

extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

void safe_inc_play_addr(CircularBuffer* buf, uint8_t blockAlign,  uint8_t chan);

inline void safe_inc_play_addr(CircularBuffer* buf, uint8_t blockAlign, uint8_t chan)
{
	if (!i_param[chan][REV])
	{
		buf->out +=blockAlign;

		if (buf->out >= buf->max) //This will not work if buf->max==0xFFFFFFFF, but luckily this is never the case with the STS!
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

int16_t get_16b_sample_avg(uint32_t addr);
inline int16_t get_16b_sample_avg(uint32_t addr)
{
	uint32_t rd;
	int16_t a,b;

	while(SDRAM_IS_BUSY){;}
//	if (addr<SDRAM_BASE || addr>=(SDRAM_BASE+SDRAM_SIZE)) return(0);

	rd = (*((uint32_t *)addr));

	a = (int16_t)(rd >> 16);
	b = (int16_t)(rd & 0x0000FFFF);
	return ((a+b)>>1);
}

int16_t get_16b_sample_right(uint32_t addr);
inline int16_t get_16b_sample_right(uint32_t addr)
{
	while(SDRAM_IS_BUSY){;}
//	if (addr<SDRAM_BASE || addr>=(SDRAM_BASE+SDRAM_SIZE)) return(0);
	return(*((int16_t *)(addr+2)));
}

int16_t get_16b_sample_left(uint32_t addr);
inline int16_t get_16b_sample_left(uint32_t addr)
{
	while(SDRAM_IS_BUSY){;}
//	if (addr<SDRAM_BASE || addr>=(SDRAM_BASE+SDRAM_SIZE)) return(0);
	return(*((int16_t *)addr));
}


void resample_read16_avg(float rs, CircularBuffer* buf, uint32_t buff_len, uint8_t block_align, uint8_t chan, int32_t *out)
{
	static float fractional_pos[4] = {0,0,0,0};
	static float xm1[4], x0[4], x1[4], x2[4];
	float a,b,c;
	uint32_t outpos;
	float t_out;
	uint8_t ch;

	ch = chan * 2; //0 or 2

	if (rs == 1.0)
	{
		for(outpos=0;outpos<buff_len;outpos++)
		{
			safe_inc_play_addr(buf, block_align, chan);
			out[outpos] = get_16b_sample_avg(buf->out);
		}
		flags[PlayBuff1_Discontinuity+chan] = 1;


	} else {

		//fill the resampling buffer with three points
		if (flags[PlayBuff1_Discontinuity+chan])
		{
			flags[PlayBuff1_Discontinuity+chan] = 0;

			safe_inc_play_addr(buf, block_align, chan);
			x0[ch] = get_16b_sample_avg(buf->out);

			safe_inc_play_addr(buf, block_align, chan);
			x1[ch] = get_16b_sample_avg(buf->out);

			safe_inc_play_addr(buf, block_align, chan);
			x2[ch] = get_16b_sample_avg(buf->out);

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
				safe_inc_play_addr(buf, block_align, chan);
				xm1[ch] 	= get_16b_sample_avg(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x0[ch] 	= get_16b_sample_avg(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_avg(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_avg(buf->out);

			}
			//Optimize for resample rates >= 3
			if (fractional_pos[ch] >= 3.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 3.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x2[ch];

				safe_inc_play_addr(buf, block_align, chan);
				x0[ch] 	= get_16b_sample_avg(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_avg(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_avg(buf->out);

			}
			//Optimize for resample rates >= 2
			if (fractional_pos[ch] >= 2.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 2.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x1[ch];
				x0[ch] 	= x2[ch];

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_avg(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_avg(buf->out);

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

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_avg(buf->out);

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


void resample_read16_right(float rs, CircularBuffer* buf, uint32_t buff_len, uint8_t block_align, uint8_t chan, int32_t *out)
{
	static float fractional_pos[4] = {0,0,0,0};
	static float xm1[4], x0[4], x1[4], x2[4];
	float a,b,c;
	uint32_t outpos;
	float t_out;
	uint8_t ch;

	ch = chan * 2 + 1; //1 or 3

	if (rs == 1.0)
	{
		for(outpos=0;outpos<buff_len;outpos++)
		{
			safe_inc_play_addr(buf, block_align, chan);
			out[outpos] = get_16b_sample_right(buf->out);
		}
		flags[PlayBuff1_Discontinuity+chan] = 1;


	} else {

		//fill the resampling buffer with three points
		if (flags[PlayBuff1_Discontinuity+chan])
		{
			flags[PlayBuff1_Discontinuity+chan] = 0;

			safe_inc_play_addr(buf, block_align, chan);
			x0[ch] = get_16b_sample_right(buf->out);

			safe_inc_play_addr(buf, block_align, chan);
			x1[ch] = get_16b_sample_right(buf->out);

			safe_inc_play_addr(buf, block_align, chan);
			x2[ch] = get_16b_sample_right(buf->out);

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
				safe_inc_play_addr(buf, block_align, chan);
				xm1[ch] 	= get_16b_sample_right(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x0[ch] 	= get_16b_sample_right(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_right(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_right(buf->out);

			}
			//Optimize for resample rates >= 3
			if (fractional_pos[ch] >= 3.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 3.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x2[ch];

				safe_inc_play_addr(buf, block_align, chan);
				x0[ch] 	= get_16b_sample_right(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_right(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_right(buf->out);

			}
			//Optimize for resample rates >= 2
			if (fractional_pos[ch] >= 2.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 2.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x1[ch];
				x0[ch] 	= x2[ch];

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_right(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_right(buf->out);

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

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_right(buf->out);

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


void resample_read16_left(float rs, CircularBuffer* buf, uint32_t buff_len, uint8_t block_align, uint8_t chan, int32_t *out)
{
	static float fractional_pos[4] = {0,0,0,0};
	static float xm1[4], x0[4], x1[4], x2[4];
	float a,b,c;
	uint32_t outpos;
	float t_out;
	uint8_t ch;


	ch = chan * 2; //0 or 2

	if (rs == 1.0)
	{
		for(outpos=0;outpos<buff_len;outpos++)
		{
			safe_inc_play_addr(buf, block_align, chan);
			out[outpos] = get_16b_sample_left(buf->out);
		}
		flags[PlayBuff1_Discontinuity+chan] = 1;


	} else {

		//fill the resampling buffer with three points
		if (flags[PlayBuff1_Discontinuity+chan])
		{
			flags[PlayBuff1_Discontinuity+chan] = 0;

			safe_inc_play_addr(buf, block_align, chan);
			x0[ch] = get_16b_sample_left(buf->out);

			safe_inc_play_addr(buf, block_align, chan);
			x1[ch] = get_16b_sample_left(buf->out);

			safe_inc_play_addr(buf, block_align, chan);
			x2[ch] = get_16b_sample_left(buf->out);

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
				safe_inc_play_addr(buf, block_align, chan);
				xm1[ch] 	= get_16b_sample_left(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x0[ch] 	= get_16b_sample_left(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_left(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_left(buf->out);

			}
			//Optimize for resample rates >= 3
			if (fractional_pos[ch] >= 3.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 3.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x2[ch];

				safe_inc_play_addr(buf, block_align, chan);
				x0[ch] 	= get_16b_sample_left(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_left(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_left(buf->out);

			}
			//Optimize for resample rates >= 2
			if (fractional_pos[ch] >= 2.0)
			{
				fractional_pos[ch] = fractional_pos[ch] - 2.0;

				//shift samples back one
				//and read a new sample
				xm1[ch] 	= x1[ch];
				x0[ch] 	= x2[ch];

				safe_inc_play_addr(buf, block_align, chan);
				x1[ch] 	= get_16b_sample_left(buf->out);

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_left(buf->out);

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

				safe_inc_play_addr(buf, block_align, chan);
				x2[ch] 	= get_16b_sample_left(buf->out);

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

