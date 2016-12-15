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

extern uint32_t play_buff_out_addr[NUM_PLAY_CHAN];

inline void safe_inc_play_addr(uint8_t chan, uint32_t limit_addr)
{
	play_buff_out_addr[chan] = inc_addr(play_buff_out_addr[chan], chan, 0);
	if (play_buff_out_addr[chan] == limit_addr)		{g_error |= (READ_BUFF1_OVERRUN<<chan);}
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
		safe_inc_play_addr(chan, limit_addr);

		x1[chan] = memory_read_sample(play_buff_out_addr[chan]);
		safe_inc_play_addr(chan, limit_addr);

		x2[chan] = memory_read_sample(play_buff_out_addr[chan]);
		safe_inc_play_addr(chan, limit_addr);

		fractional_pos[chan] = 0.0;
	}

	outpos=0;
	while (outpos < buff_len)
	{
		//Optimize for resample rates >= 4
		if (fractional_pos[chan] >= 4.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 4.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

			x0[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

			x1[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

			x2[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

		}
		//Optimize for resample rates >= 3
		if (fractional_pos[chan] >= 3.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 3.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= x2[chan];

			x0[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

			x1[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

			x2[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

		}
		//Optimize for resample rates >= 2
		if (fractional_pos[chan] >= 2.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 2.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= x1[chan];
			x0[chan] 	= x2[chan];

			x1[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

			x2[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

		}
		//Optimize for resample rates >= 1
		if (fractional_pos[chan] >= 1.0)
		{
			fractional_pos[chan] = fractional_pos[chan] - 1.0;

			//shift samples back one
			//and read a new sample
			xm1[chan] 	= x0[chan];
			x0[chan] 	= x1[chan];
			x1[chan] 	= x2[chan];

			x2[chan] 	= memory_read_sample(play_buff_out_addr[chan]);
			safe_inc_play_addr(chan, limit_addr);

		}

		//calculate coefficients
		a = (3 * (x0[chan]-x1[chan]) - xm1[chan] + x2[chan]) / 2;
		b = 2*x1[chan] + xm1[chan] - (5*x0[chan] + x2[chan]) / 2;
		c = (x1[chan] - xm1[chan]) / 2;

		//calculate as many fractionally placed output points as we can
		while ( fractional_pos[chan] < 1.0 && outpos<buff_len)
		{
			out[outpos++] = (((a * fractional_pos[chan]) + b) * fractional_pos[chan] + c) * fractional_pos[chan] + x0[chan];
			fractional_pos[chan] += rs;
		}


	}

}

