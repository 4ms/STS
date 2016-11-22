/*
 * resample.c
 *
 */

#include "globals.h"
#include "resample.h"
#include "dig_pins.h"
#include "audio_sdram.h"
#include "audio_util.h"

extern uint8_t flags[NUM_FLAGS];
extern enum g_Errors g_error;


//-O0: 	1.000ms at 4.0
// 		0.340ms at 1.0
// 		0.200ms at 0.33

//-O1: 	0.372ms at 4.0 (7%)
// 		0.136ms at 1.0 (2.5%)
// 		0.075ms at 0.33 (1.4%)
//runs every 5.3ms


int32_t resample(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out, int32_t last_sample)
{
	uint32_t i;

	int16_t xm1;
	int16_t x0,x1,x2;
	float a,b,c;
	float finpos=0;
	float floatpos;
	uint32_t outpos=0;


	xm1 = last_sample;

	for (	i=0;
			outpos<num_samples_out;
			i++
		)
	{
		x0  = in[i];
		x1  = in[i+1];
		x2  = in[i+2];
		a = (3 * (x0-x1) - xm1 + x2) / 2;
		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
		c = (x1 - xm1) / 2;

		for (;finpos<(i+1);finpos+=rs)
		{
			floatpos = finpos - i;
			out[outpos++] = (((a * floatpos) + b) * floatpos + c) * floatpos + x0;

			if (outpos==num_samples_out) break;
		}

		xm1 = in[i];
	}

	return (x0);

}




int32_t nosample(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out)
{
	uint32_t i;


	for (	i=0;
			i<num_samples_out;
			i++)
	{
		out[i] = in[i];
	}

	return (i);

}


float resample_nom1(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out, float start_fpos)
{
	uint32_t i;

	int16_t xm1;
	int16_t x0,x1,x2;
	float a,b,c;
	float finpos;
	float floatpos;
	uint32_t outpos=0;

	//uint32_t num_samples_in;
	//num_samples_in = num_samples_out * rs + 2;

	finpos=start_fpos+rs;

	for (	i=0;
			outpos<num_samples_out;
			i++
		)
	{
		xm1 = in[i];
		x0  = in[i+1];
		x1  = in[i+2];
		x2  = in[i+3];
		a = (3 * (x0-x1) - xm1 + x2) / 2;
		b = 2*x1 + xm1 - (5*x0 + x2) / 2;
		c = (x1 - xm1) / 2;

		for (;finpos<(i+1);finpos+=rs)
		{
			floatpos = finpos - i;
			out[outpos++] = (((a * floatpos) + b) * floatpos + c) * floatpos + x0;

			if (outpos==num_samples_out) break;
		}
	}

	finpos = floatpos;

	return (finpos); //return the interpolation position.

}


void resample_read(float rs, uint32_t *play_buff_out_addr, uint32_t limit_addr, uint32_t buff_len, uint8_t chan, float *fractional_pos, int32_t *out)
{
	static float xm1[2], x0[2], x1[2], x2[2];
	float a,b,c;
	uint32_t outpos;


	//fill the resampling buffer with three points
	if (flags[PlayBuff1_Discontinuity+chan])
	{
		flags[PlayBuff1_Discontinuity+chan] = 0;

		x0[chan] = memory_read_sample(play_buff_out_addr[chan]);
		play_buff_out_addr[chan] = inc_addr(play_buff_out_addr[chan], chan, 0);

		x1[chan] = memory_read_sample(play_buff_out_addr[chan]);
		play_buff_out_addr[chan] = inc_addr(play_buff_out_addr[chan], chan, 0);

		x2[chan] = memory_read_sample(play_buff_out_addr[chan]);
		play_buff_out_addr[chan] = inc_addr(play_buff_out_addr[chan], chan, 0);

		fractional_pos[chan] = 0.0;
	}

	outpos=0;
	while (outpos < buff_len)
	{
		if (fractional_pos[chan] >= 1.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 1.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= x0[chan];
			x0[chan] 	= x1[chan];
			x1[chan] 	= x2[chan];
			x2[chan] 	= memory_read_sample(play_buff_out_addr[chan]);

			play_buff_out_addr[chan] = inc_addr(play_buff_out_addr[chan], chan, 0);

			//flag error condition
			if (play_buff_out_addr[chan] == limit_addr)
			{
				g_error |= (READ_BUFF1_OVERRUN<<chan);
				break;
			}
		}

		//calculate coefficients
		a = (3 * (x0[chan]-x1[chan]) - xm1[chan] + x2[chan]) / 2;
		b = 2*x1[chan] + xm1[chan] - (5*x0[chan] + x2[chan]) / 2;
		c = (x1[chan] - xm1[chan]) / 2;

		//calculate as many fractionally placed output points as we can
		while ( fractional_pos[chan] < 1.0 && outpos<buff_len)
		{
			out[outpos++] = (((a * fractional_pos[chan]) + b) * fractional_pos[chan] + c) * fractional_pos[chan] + x0;

			fractional_pos[chan] += rs;
		}


	}

}

