/*
 * resample.c
 *
 */

#include "globals.h"
#include "resample.h"
#include "dig_pins.h"

//-O0: 	1.000ms at 4.0
// 		0.340ms at 1.0
// 		0.200ms at 0.33

//-O1: 	0.372ms at 4.0 (7%)
// 		0.136ms at 1.0 (2.5%)
// 		0.075ms at 0.33 (1.4%)
//runs every 5.3ms


int32_t nosample(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out)
{
	uint32_t i;

	int16_t xm1;
	int16_t x0,x1,x2;
	float a,b,c;
	float finpos=0;
	float floatpos;
	uint32_t outpos=0;


	for (	i=0;
			i<num_samples_out;
			i++
		)
	{
		out[i] = in[i];
		//if (outpos==num_samples_out) break;
	}

	return (outpos); //return how many samples of *in were used

}


int32_t resample(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out)
{
	uint32_t i;

	int16_t xm1;
	int16_t x0,x1,x2;
	float a,b,c;
	float finpos=0;
	float floatpos;
	uint32_t outpos=0;

	//uint32_t num_samples_in;
	//num_samples_in = num_samples_out * rs + 2;

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

	//return (i-1); //return how many samples of *in were used
	return (outpos); //return how many samples of *in were used

}




