/*
 * audio_codec.c - audio i/o processing routine
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#include "globals.h"
#include "sampler.h"
#include "wav_recording.h"
#include "params.h"
#include "calibration.h"
#include "adc.h"

extern SystemCalibrations *system_calibrations;

extern uint8_t global_mode[NUM_GLOBAL_MODES];


// This routine uses one assembly instruction (ssat) to saturate, or clip, 
// a 32-bit signed integer to a 16-bit signed value.
// Without using this, we would get nasty-sounding wraparound clipping
// See the STM32 assembly instruction set documentation for more information
static inline int32_t HARD_CLIP_16BITS(int32_t x);
static inline int32_t HARD_CLIP_16BITS(int32_t x) {asm("ssat %[dst], #16, %[src]" : [dst] "=r" (x) : [src] "r" (x)); return x;}

void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	CCMDATA static int32_t outL[2][HT16_CHAN_BUFF_LEN];
	CCMDATA static int32_t outR[2][HT16_CHAN_BUFF_LEN];
	uint16_t i;
	int16_t dummy;
	int32_t t_i32;

	//
	// Send the incoming audio (src) to be handled
	// by the recording features
	//

	record_audio_to_buffer(src);

	//
	// Fill outL and outR with outgoing audio
	//

	play_audio_from_buffer(outL[CHAN1], outR[CHAN1], CHAN1);
	play_audio_from_buffer(outL[CHAN2], outR[CHAN2], CHAN2);


	// The following could be made more readable and maintainable by having one for() loop
	// and putting the if(global_mode[...]) statements inside the loop, but execution
	// time increases dramatically, since multiple compares are done in the inner loop
	//
	if (global_mode[CALIBRATE])
	{
		// Calibrate mode: output a null signal plus the calibration offset
		// This allows us to use a scope or meter while finely adjusting the 
		// calibration value, until we get a DC level of zero volts on the 
		// audio outut jacks.
		//
		for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		{
			*dst++ = system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
			*dst++ = 0;
			*dst++ = system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
			*dst++ = 0;
		}
	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_BOTH)
	{
		// Monitor both channels:
		// Send the input signal (src) to the output jacks (dst)
		//
		for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		{
			//Left Out
			t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
			*dst++ = HARD_CLIP_16BITS(t_i32);
			*dst++ = *src++;

			//Right out
			t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
			*dst++ = HARD_CLIP_16BITS(t_i32);
			*dst++ = *src++;

		}
	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_OFF)
	{
		// Monitoring off (playback on both channels):
		// 
		if (global_mode[STEREO_MODE])
		{
			// Stereo mode
			// Left Out = Sum of channel 1 and channel 2's L
			// Right Out = Sum of channel 1 and channel 2' R
			//
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//Chan 1 L + Chan 2 L clipped at signed 16-bits
				t_i32 = outL[CHAN1][i] + outL[CHAN2][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;

				//Chan 1 R + Chan 2 R clipped at signed 16-bits
				t_i32 = outR[CHAN1][i] + outR[CHAN2][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;
			}
		}
		else
		{
			// Mono mode
			// Left Out = average of L+R of Channel 1's playback
			// Right Out = average of L+R of Channel 2's playback
			// Note: the average is calculated in play_audio_from_buffer() and stored in outL[]
			// 
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//Chan 1 L+R average gclipped at signed 16-bits
				t_i32 = outL[CHAN1][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;

				//Chan 2 L+R average clipped at signed 16-bits
				t_i32 = outL[CHAN2][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;
			}
		}

	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_RIGHT)
	{
		// Split Monitoring:
		// Channel 1 = Playback
		// Channel 2 = Monitor
		//
		if (global_mode[STEREO_MODE])
		{
			// Stereo Mode
			// Left Out = Sum of channel 1 and channel 2's L
			// Right Out = Monitor right input
			//
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//Chan 1 L + Chan 2 L clipped at signed 16-bits
				t_i32 = outL[CHAN1][i] + outL[CHAN2][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;

				src+=2; //Ignore Left input

				//R input signal
				t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = *src++;
			}
		}
		else
		{
			// Mono mode
			// Left Out = average of L+R of Channel 1's playback
			// Right Out = Monitor right input
			//
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//Chan 1 L+R clipped at signed 16-bits
				t_i32 = outL[CHAN1][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;

				src+=2; //Ignore Left input

				//R input signal
				t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = *src++;
			}
		}
	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_LEFT)
	{
		// Split Monitoring:
		// Channel 1 = Playback
		// Channel 2 = Monitor
		//
		if (global_mode[STEREO_MODE])
		{
			// Stereo mode
			// Left Out = L input signal
			// Right Out = Sum of channel 1 and channel 2's R
			//
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//L input signal
				t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = *src++;

				//Chan 1 R + Chan 2 R clipped at signed 16-bits
				t_i32 = outR[CHAN1][i] + outR[CHAN2][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;
				src+=2; //Ignore Right input
			}
		}
		else
		{
			// Mono mode
			// Left Out = Monitor left input
			// Right Out = average of L+R of Channel 2's playback
			//
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//L input signal
				t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[CHAN1];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = *src++;

				//Chan 2 L+R clipped at signed 16-bits
				t_i32 = outL[CHAN2][i] + system_calibrations->codec_dac_calibration_dcoffset[CHAN2];
				*dst++ = HARD_CLIP_16BITS(t_i32);
				*dst++ = 0;
				src+=2; //Ignore Right input
			}
		}
	}
}

