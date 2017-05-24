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
	CB_offset_out_address(buf, blockAlign, i_param[chan][REV]);

	//check for crossing of buf->in
//	if (buf->in==buf->out)
//		DEBUG2_ON;

}


int16_t get_16b_sample(uint32_t addr, uint8_t stereomode);
inline int16_t get_16b_sample(uint32_t addr, uint8_t stereomode)
{
	uint32_t rd;
	//int16_t a,b;

	rd = memory_read_32bword(addr);

	//This is more efficient to sum the L and R here, since we would only read SDRAM once,
	//and calculate the resampled data only once (on the sum instead of once on each channel).
	//However, it's more versatile/portable to take two outputs from the SDRAM,
	//resample them independently, and then combine them later when we feed the codec
	//Perhaps an improvement would be to change methods depending on global_mode[STEREO_MODE]
	// if (stereomode==STEREO_SUM)
	// {
	// 	a = (int16_t)(rd >> 16);
	// 	b = (int16_t)(rd & 0x0000FFFF);
	// 	return ((a+b)>>1);
	// }

	if (stereomode==STEREO_RIGHT)
		return((int16_t)((rd & 0xFFFF0000)>>16));

	//STEREO_LEFT or MONO
	return((int16_t)(rd));
}


//Todo: Make this return int32_t (actual 24-bit data) and resample based on that
//Would only need to change the clipping value 32768 in resample_read16(),
//and then make sure 24-bit output math works in audio_codec.c
//
//For now, we just use the top 16 bits
//
int16_t get_24b_sample(uint32_t addr, uint8_t stereomode);
inline int16_t get_24b_sample(uint32_t addr, uint8_t stereomode)
{
	int16_t t;

	if (stereomode==STEREO_RIGHT)
		t = (int16_t)((memory_read_32bword(addr + 3) & 0x00FFFF00) >> 8);

		//return( (int16_t)((memory_read_32bword(addr + 3) & 0xFFFF0000) >> 16) );

	//STEREO_LEFT or MONO
	else
		t = (int16_t)((memory_read_32bword(addr) & 0x00FFFF00) >> 8);

	//return( (int16_t)((memory_read_32bword(addr)) >> 16) );

	return t;
}


//Todo: Make this return int32_t (actual 24-bit data) and resample based on that
//Would only need to change the clipping value 32768 in resample_read16(),
//and then make sure 24-bit output math works in audio_codec.c
//
//For now, we just use the top 16 bits
//
int16_t get_32f_sample(uint32_t addr, uint8_t stereomode);
inline int16_t get_32f_sample(uint32_t addr, uint8_t stereomode)
{
	uint32_t t;
	float f;

	if (stereomode==STEREO_RIGHT)
		t = memory_read_32bword(addr + 4);
	else
		t = memory_read_32bword(addr);

	f = *((float *)&t); //convert raw 32 bits to float

	if (f>1.0) return(32767);
	else if (f<-1.0) return(-32767);
	else return ((int16_t)(f * 32767.0));
}

//ToDo: Optimize this by having a different resample_read function for 16/24/32 bit...
//Optimize further by having a different routine for STEREO_SUM, LEFT, and RIGHT (will save the if branch for each sample read)
//Then we also need a different memory_read_sample function and safe_inc_num_play_addr function
//This will save the "if" for every memory read, and the "while" in every safe_inc_num_play_addr

void resample_read16(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, float *fractional_pos, int32_t *out)
{
	static float xm1[4], x0[4], x1[4], x2[4];
	float a,b,c;
	uint32_t outpos;
	float t_out;
	uint8_t ch;

	if (stereomode==STEREO_RIGHT)
		ch = chan * 2 + 1; //1 or 3
	else
		ch = chan * 2; //0 or 2

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




void resample_read24(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, int32_t *out)
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

	//fill the resampling buffer with three points
	if (flags[PlayBuff1_Discontinuity+chan])
	{
		flags[PlayBuff1_Discontinuity+chan] = 0;

		x0[ch] = get_24b_sample(buf->out, stereomode);
		safe_inc_play_addr(buf, block_align, chan);

		x1[ch] = get_24b_sample(buf->out, stereomode);
		safe_inc_play_addr(buf, block_align, chan);

		x2[ch] = get_24b_sample(buf->out, stereomode);
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
			xm1[ch]	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x0[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x1[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x2[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

		}
		//Optimize for resample rates >= 3
		if (fractional_pos[ch] >= 3.0)
		{
			fractional_pos[ch] = fractional_pos[ch] - 3.0;

			//shift samples back one
			//and read a new sample
			xm1[ch] = x2[ch];

			x0[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x1[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x2[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

		}
		//Optimize for resample rates >= 2
		if (fractional_pos[ch] >= 2.0)
		{
			fractional_pos[ch] = fractional_pos[ch] - 2.0;

			//shift samples back one
			//and read a new sample
			xm1[ch] = x1[ch];
			x0[ch] 	= x2[ch];

			x1[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x2[ch] 	= get_24b_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

		}
		//Optimize for resample rates >= 1
		if (fractional_pos[ch] >= 1.0)
		{
			fractional_pos[ch] = fractional_pos[ch] - 1.0;

			//shift samples back one
			//and read a new sample
			xm1[ch] = x0[ch];
			x0[ch] 	= x1[ch];
			x1[ch] 	= x2[ch];

			x2[ch] 	= get_24b_sample(buf->out, stereomode);
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

void resample_read32f(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, int32_t *out)
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

	//fill the resampling buffer with three points
	if (flags[PlayBuff1_Discontinuity+chan])
	{
		flags[PlayBuff1_Discontinuity+chan] = 0;

		x0[ch] = get_32f_sample(buf->out, stereomode);
		safe_inc_play_addr(buf, block_align, chan);

		x1[ch] = get_32f_sample(buf->out, stereomode);
		safe_inc_play_addr(buf, block_align, chan);

		x2[ch] = get_32f_sample(buf->out, stereomode);
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
			xm1[ch]	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x0[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x1[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x2[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

		}
		//Optimize for resample rates >= 3
		if (fractional_pos[ch] >= 3.0)
		{
			fractional_pos[ch] = fractional_pos[ch] - 3.0;

			//shift samples back one
			//and read a new sample
			xm1[ch] = x2[ch];

			x0[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x1[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x2[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

		}
		//Optimize for resample rates >= 2
		if (fractional_pos[ch] >= 2.0)
		{
			fractional_pos[ch] = fractional_pos[ch] - 2.0;

			//shift samples back one
			//and read a new sample
			xm1[ch] = x1[ch];
			x0[ch] 	= x2[ch];

			x1[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

			x2[ch] 	= get_32f_sample(buf->out, stereomode);
			safe_inc_play_addr(buf, block_align, chan);

		}
		//Optimize for resample rates >= 1
		if (fractional_pos[ch] >= 1.0)
		{
			fractional_pos[ch] = fractional_pos[ch] - 1.0;

			//shift samples back one
			//and read a new sample
			xm1[ch] = x0[ch];
			x0[ch] 	= x1[ch];
			x1[ch] 	= x2[ch];

			x2[ch] 	= get_32f_sample(buf->out, stereomode);
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


