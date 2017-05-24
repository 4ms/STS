#include "globals.h"
#include "sampler.h"
#include "wav_recording.h"
#include "params.h"
#include "calibration.h"

extern SystemCalibrations *system_calibrations;

extern uint8_t SAMPLINGBYTES;

extern uint8_t global_mode[NUM_GLOBAL_MODES];

//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];

void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	int32_t outL[2][HT16_CHAN_BUFF_LEN];
	int32_t outR[2][HT16_CHAN_BUFF_LEN];
	uint16_t i;
	int32_t t;

	//
	// Incoming audio
	//
	record_audio_to_buffer(src);

	//
	// Outgoing audio
	//
	//play_audio_from_buffer(out[0], 0);
	//play_audio_from_buffer(out[1], 1);

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
	else if (global_mode[MONITOR_RECORDING])
	{
		for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		{
			*dst++ = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[0];
			*dst++ = *src++;
			*dst++ = (*src++) + system_calibrations->codec_dac_calibration_dcoffset[1];
			*dst++ = *src++;
		}
	}
	else if (SAMPLINGBYTES==2)
	{
		if (global_mode[STEREO_MODE])
		{
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				*dst++ = ((outL[0][i] + outL[1][i]) /2) + system_calibrations->codec_dac_calibration_dcoffset[0];
				*dst++ = 0;
				*dst++ = ((outR[0][i] + outR[1][i]) /2) + system_calibrations->codec_dac_calibration_dcoffset[1];
				*dst++ = 0;
			}
		}
		else
		{
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				*dst++ = ((outL[0][i] + outR[0][i]) /2) + system_calibrations->codec_dac_calibration_dcoffset[0];
			//	*dst++ = outL[0][i]  + system_calibrations->codec_dac_calibration_dcoffset[0];
				*dst++ = 0;
				*dst++ = ((outL[1][i] + outR[1][i]) /2) + system_calibrations->codec_dac_calibration_dcoffset[0];
			//	*dst++ = outL[1][i] + system_calibrations->codec_dac_calibration_dcoffset[0];
				*dst++ = 0;
			}
		}

	}
	else //SAMPLINGBYTES==4
	{
		if (global_mode[STEREO_MODE])
		{
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//L out
				t = ((outL[0][i] + outL[1][i]) >> 1);
				*dst++ = (int16_t)(t>>16) + (int16_t)system_calibrations->codec_dac_calibration_dcoffset[0];
				*dst++ = (int16_t)(t & 0x0000FF00);

				//R out
				t = ((outR[0][i] + outR[1][i]) >> 1);
				*dst++ = (int16_t)(t>>16) + (int16_t)system_calibrations->codec_dac_calibration_dcoffset[1];
				*dst++ = (int16_t)(t & 0x0000FF00);
			}
		}
		else
		{
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			{
				//L out
				t = ((outL[0][i] + outR[0][i]) >> 1);
				*dst++ = (int16_t)(t>>16) + (int16_t)system_calibrations->codec_dac_calibration_dcoffset[0];
				*dst++ = (int16_t)(t & 0x0000FF00);

				//R out
				t = ((outL[1][i] + outR[1][i]) >> 1);
				*dst++ = (int16_t)(t>>16) + (int16_t)system_calibrations->codec_dac_calibration_dcoffset[1];
				*dst++ = (int16_t)(t & 0x0000FF00);
			}
		}
	}

#else
	for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
	{
		*dst++ = potadc_buffer[channel+2]*4;
		*dst++ = 0;

		//*dst++ = cvadc_buffer[channel+4]*4;
		*dst++ = cvadc_buffer[channel+0]*4;
		*dst++ = 0;
	}
#endif

}

