#include "globals.h"
#include "sampler.h"
#include "wav_recording.h"
#include "params.h"
#include "calibration.h"
#include "adc.h"

extern SystemCalibrations *system_calibrations;

extern uint8_t SAMPLINGBYTES;

extern uint8_t global_mode[NUM_GLOBAL_MODES];


//extern volatile float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t bracketed_cvadc[8];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];

void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	CCMDATA static int32_t outL[2][HT16_CHAN_BUFF_LEN];
	CCMDATA static int32_t outR[2][HT16_CHAN_BUFF_LEN];
	uint16_t i;
	int16_t dummy;
	int32_t t_i32;

	//
	// Incoming audio
	//
	record_audio_to_buffer(src);

	//
	// Outgoing audio
	//

	play_audio_from_buffer(outL[0], outR[0], 0);
	play_audio_from_buffer(outL[1], outR[1], 1);


#ifndef DEBUG_ADC_TO_CODEC

	if (global_mode[CALIBRATE])
	{
		for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		{
			*dst++ = system_calibrations->codec_dac_calibration_dcoffset[0];
			*dst++ = 0;
			*dst++ = system_calibrations->codec_dac_calibration_dcoffset[1];
			*dst++ = 0;
		}
	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_BOTH)
	{
		for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		{
			t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[0];
			asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
			*dst++ = t_i32;
			*dst++ = *src++;

			t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[1];
			asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
			*dst++ = t_i32;
			*dst++ = *src++;

		}
	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_OFF)
	{
		if (SAMPLINGBYTES==2)
		{
			if (global_mode[STEREO_MODE])
			{
				//Left Out = Sum of both L channels
				//Right Out = Sum of both R channels
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					//Chan 1 L + Chan 2 L clipped at signed 16-bits
					t_i32 = outL[0][i] + outL[1][i] + system_calibrations->codec_dac_calibration_dcoffset[0];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;

					//Chan 1 R + Chan 2 R clipped at signed 16-bits
					t_i32 = outR[0][i] + outR[1][i] + system_calibrations->codec_dac_calibration_dcoffset[1];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;
				}
			}
			else
			{
				//in mono mode, outL is the average of L+R
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					//Chan 1 L+R clipped at signed 16-bits
					t_i32 = outL[0][i] + system_calibrations->codec_dac_calibration_dcoffset[0];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;

					//Chan 2 L+R clipped at signed 16-bits
					t_i32 = outL[1][i] + system_calibrations->codec_dac_calibration_dcoffset[1];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;
				}
			}

		}
	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_RIGHT)
	{
		if (SAMPLINGBYTES==2)
		{
			if (global_mode[STEREO_MODE])
			{
				//Left Out = Sum of both L channels
				//Right Out = right input
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					//Chan 1 L + Chan 2 L clipped at signed 16-bits
					t_i32 = outL[0][i] + outL[1][i] + system_calibrations->codec_dac_calibration_dcoffset[0];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;

					dummy = *src++;dummy = *src++; //ignore L input
					//copy R input signal to R output
					t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[1];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = *src++;
				}
			}
			else
			{
				//in mono mode, outL is the average of L+R
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					//Chan 1 L+R clipped at signed 16-bits
					t_i32 = outL[0][i] + system_calibrations->codec_dac_calibration_dcoffset[0];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;

					dummy = *src++;dummy = *src++; //ignore L input
					//copy R input signal to R output
					t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[1];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = *src++;
				}
			}

		}
	}
	else if (global_mode[MONITOR_RECORDING] == MONITOR_LEFT)
	{
		if (SAMPLINGBYTES==2)
		{
			if (global_mode[STEREO_MODE])
			{
				//Left Out = L input signal
				//Right Out = Sum of both R channels
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					//L input signal
					t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[0];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = *src++;
					//ignore R input
					dummy = *src++;dummy = *src++;

					//Chan 1 R + Chan 2 R clipped at signed 16-bits
					t_i32 = outR[0][i] + outR[1][i] + system_calibrations->codec_dac_calibration_dcoffset[1];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;
				}
			}
			else
			{
				//in mono mode, outL is the average of L+R
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					//L input signal
					t_i32 = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[0];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = *src++;
					//ignore R input
					dummy = *src++;dummy = *src++;

					//Chan 2 L+R clipped at signed 16-bits
					t_i32 = outL[1][i] + system_calibrations->codec_dac_calibration_dcoffset[1];
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					*dst++ = t_i32;
					*dst++ = 0;
				}
			}

		}
	}


		// else //SAMPLINGBYTES==4
		// {
		// 	if (global_mode[STEREO_MODE])
		// 	{
		// 		for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		// 		{
		// 			//L out clipped at signed 24 bits
		// 			t_i32 = ((outL[0][i] + outL[1][i]) >> 1) + (int16_t)system_calibrations->codec_dac_calibration_dcoffset[0];
		//			asm("ssat %[dst], #24, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
		// 			*dst++ = (int16_t)(t_i32>>16) ;
		// 			*dst++ = (int16_t)(t_i32 & 0x0000FF00);

		// 			//R out clipped at signed 24 bits
		// 			t_i32 = ((outR[0][i] + outR[1][i]) >> 1) + (int16_t)system_calibrations->codec_dac_calibration_dcoffset[0];
		//			asm("ssat %[dst], #24, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
		// 			*dst++ = (int16_t)(t_i32>>16) ;
		// 			*dst++ = (int16_t)(t_i32 & 0x0000FF00);
		// 		}
		// 	}
		// 	else
		// 	{
		// 		for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		// 		{
		// 			//Chan 1 L+R out clipped at signed 24 bits
		//			t_i32 = outL[0][i] + system_calibrations->codec_dac_calibration_dcoffset[0];
		//			asm("ssat %[dst], #24, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
		// 			*dst++ = (int16_t)(t_i32>>16) ;
		// 			*dst++ = (int16_t)(t_i32 & 0x0000FF00);

		// 			//Chan 2 L+R out clipped at signed 24 bits
		//			t_i32 = outL[1][i] + system_calibrations->codec_dac_calibration_dcoffset[1];
		//			asm("ssat %[dst], #24, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
		// 			*dst++ = (int16_t)(t_i32>>16) ;
		// 			*dst++ = (int16_t)(t_i32 & 0x0000FF00);
		// 		}
		// 	}
		// }


#else //DEBUG_ADC_TO_CODEC
	for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
	{
		//*dst++ = potadc_buffer[channel+2]*4;
		*dst++ = 0;

		//*dst++ = cvadc_buffer[channel+0]*4;
		*dst++ = 0;
	}
#endif

}

