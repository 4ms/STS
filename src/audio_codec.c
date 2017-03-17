#include "globals.h"
#include "sampler.h"
#include "wav_recording.h"
#include "params.h"

extern int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
//extern int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];

extern uint8_t SAMPLINGBYTES;

extern uint8_t global_mode[NUM_GLOBAL_MODES];

//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];

void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	int32_t out[2][HT16_CHAN_BUFF_LEN];
	uint16_t i;

	//
	// Incoming audio
	//
	record_audio_to_buffer(src);

	//
	// Outgoing audio
	//


	play_audio_from_buffer(out[0], 0);
	play_audio_from_buffer(out[1], 1);


	for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
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
				if (global_mode[MONITOR_RECORDING])
				{
					*dst++ = (*src++) + out[0][i] + CODEC_DAC_CALIBRATION_DCOFFSET[0];
					*dst++ = *src++;
					*dst++ = (*src++) + out[1][i] + CODEC_DAC_CALIBRATION_DCOFFSET[1];
					*dst++ = *src++;
				}
				else
				{
					//L out
					*dst++ = out[0][i] + CODEC_DAC_CALIBRATION_DCOFFSET[0];
					*dst++ = 0;

					//R out
					*dst++ = out[1][i] + CODEC_DAC_CALIBRATION_DCOFFSET[1];
					*dst++ = 0;
				}
			}
			else
			{
				if (global_mode[MONITOR_RECORDING])
				{
					*dst++ = (*src++) + (int16_t)(out[0][i]>>16) + CODEC_DAC_CALIBRATION_DCOFFSET[0];
					*dst++ = (*src++) + (int16_t)(out[0][i] & 0x0000FF00);
					*dst++ = (*src++) + (int16_t)(out[1][i]>>16) + CODEC_DAC_CALIBRATION_DCOFFSET[1];
					*dst++ = (*src++) + (int16_t)(out[1][i] & 0x0000FF00);
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
		}

#else
		*dst++ = potadc_buffer[channel+2]*4;
		*dst++ = 0;

		//*dst++ = cvadc_buffer[channel+4]*4;
		*dst++ = cvadc_buffer[channel+0]*4;
		*dst++ = 0;

#endif
	}


}

