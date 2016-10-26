/*
 * params.c
 *
 *  Created on: Mar 27, 2015
 *      Author: design
 */

#define QUANTIZE_TIMECV_CH1 1
#define QUANTIZE_TIMECV_CH2 0

#include "globals.h"
#include "adc.h"
#include "dig_pins.h"
#include "params.h"
#include "equal_pow_pan_padded.h"
#include "exp_1voct.h"
#include "timekeeper.h"
#include "looping_delay.h"
#include "log_taper_padded.h"


extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern uint8_t flag_rev_change[NUM_CHAN];

extern uint8_t disable_mode_changes;


float param[NUM_CHAN][NUM_PARAMS];
float global_param[NUM_GLOBAL_PARAMS];
uint8_t mode[NUM_CHAN][NUM_CHAN_MODES];
uint8_t global_mode[NUM_GLOBAL_MODES];

uint8_t flag_time_param_changed[2];

int32_t MIN_POT_ADC_CHANGE[NUM_POT_ADCS];
int32_t MIN_CV_ADC_CHANGE[NUM_CV_ADCS];

extern int16_t CV_CALIBRATION_OFFSET[NUM_CV_ADCS];

float POT_LPF_COEF[NUM_POT_ADCS];
float CV_LPF_COEF[NUM_CV_ADCS];

//Low Pass filtered adc values:
float smoothed_potadc[NUM_POT_ADCS];
float smoothed_cvadc[NUM_CV_ADCS];

//Integer-ized low pass filtered adc values:
int16_t i_smoothed_potadc[NUM_POT_ADCS];
int16_t i_smoothed_cvadc[NUM_CV_ADCS];

int16_t old_i_smoothed_cvadc[NUM_CV_ADCS];
int16_t old_i_smoothed_potadc[NUM_POT_ADCS];

float smoothed_rawcvadc[NUM_CV_ADCS];
int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

//Change in pot since last process_adc
int32_t pot_delta[NUM_POT_ADCS];
int32_t cv_delta[NUM_POT_ADCS];


void init_params(void)
{
	uint8_t channel=0;

	for (channel=0;channel<NUM_CHAN;channel++){
		param[channel][PITCH] = 1.0;
		param[channel][START] = 0.0;
		param[channel][LENGTH] = 1.0;

		param[channel][TRACKING_COMP] = 1.00;
	}

	global_param[SLOW_FADE_INCREMENT] = 0.001;
}

//initializes modes that aren't read from flash ram
void init_modes(void)
{
	uint8_t channel=0;

	for (channel=0;channel<NUM_CHAN;channel++){
		mode[channel][BANK] = 0;
		mode[channel][SAMPLE] = 0;
		mode[channel][REV] = 0;
	}

	global_mode[CALIBRATE] = 0;
	global_mode[SYSTEM_SETTINGS] = 0;

}


inline float LowPassSmoothingFilter(float current_value, float new_value, float coef)
{
	return (current_value * coef) + (new_value * (1.0f-coef));
}

void init_LowPassCoefs(void)
{
	float t;
	uint8_t i;

	t=50.0;

	CV_LPF_COEF[PITCH*2] = 1.0-(1.0/t);
	CV_LPF_COEF[PITCH*2+1] = 1.0-(1.0/t);

	CV_LPF_COEF[START*2] = 1.0-(1.0/t);
	CV_LPF_COEF[START*2+1] = 1.0-(1.0/t);

	CV_LPF_COEF[LENGTH*2] = 1.0-(1.0/t);
	CV_LPF_COEF[LENGTH*2+1] = 1.0-(1.0/t);

	CV_LPF_COEF[SAMPLE*2] = 1.0-(1.0/t);
	CV_LPF_COEF[SAMPLE*2+1] = 1.0-(1.0/t);

	MIN_CV_ADC_CHANGE[PITCH*2] = 30;
	MIN_CV_ADC_CHANGE[PITCH*2+1] = 30;

	MIN_CV_ADC_CHANGE[START*2] = 20;
	MIN_CV_ADC_CHANGE[START*2+1] = 20;

	MIN_CV_ADC_CHANGE[LENGTH*2] = 20;
	MIN_CV_ADC_CHANGE[LENGTH*2+1] = 20;

	MIN_CV_ADC_CHANGE[SAMPLE*2] = 20;
	MIN_CV_ADC_CHANGE[SAMPLE*2+1] = 20;


	t=20.0; //50.0 = about 100ms to turn a knob fully

	POT_LPF_COEF[PITCH_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[PITCH_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[START_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[START_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[LENGTH_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[LENGTH_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[SAMPLE_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[SAMPLE_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[RECSAMPLE_POT] = 1.0-(1.0/t);



	MIN_POT_ADC_CHANGE[PITCH_POT*2] = 60;
	MIN_POT_ADC_CHANGE[PITCH_POT*2+1] = 60;

	MIN_POT_ADC_CHANGE[START_POT*2] = 60;
	MIN_POT_ADC_CHANGE[START_POT*2+1] = 60;

	MIN_POT_ADC_CHANGE[LENGTH_POT*2] = 60;
	MIN_POT_ADC_CHANGE[LENGTH_POT*2+1] = 60;

	MIN_POT_ADC_CHANGE[SAMPLE_POT*2] = 60;
	MIN_POT_ADC_CHANGE[SAMPLE_POT*2+1] = 60;

	MIN_POT_ADC_CHANGE[RECSAMPLE_POT] = 60;


	for (i=0;i<NUM_POT_ADCS;i++)
	{
		smoothed_potadc[i]=0;
		old_i_smoothed_potadc[i]=0;
		i_smoothed_potadc[i]=0x7FFF;
		pot_delta[i]=0;
	}
	for (i=0;i<NUM_CV_ADCS;i++)
	{
		smoothed_cvadc[i]=0;
		smoothed_rawcvadc[i]=0;
		old_i_smoothed_cvadc[i]=0;
		i_smoothed_cvadc[i]=0x7FFF;
		i_smoothed_rawcvadc[i]=0x7FFF;
		cv_delta[i]=0;
	}
}


void process_adc(void)
{
	uint8_t i;
	int32_t t;

	uint8_t flag_pot_changed[NUM_POT_ADCS];

	static uint32_t track_moving_pot[NUM_POT_ADCS]={0,0,0,0,0,0,0,0,0};

	//
	// Run a LPF on the pots and CV jacks
	//
	for (i=0;i<NUM_POT_ADCS;i++)
	{
		flag_pot_changed[i]=0;

		smoothed_potadc[i] = LowPassSmoothingFilter(smoothed_potadc[i], (float)potadc_buffer[i], POT_LPF_COEF[i]);
		i_smoothed_potadc[i] = (int16_t)smoothed_potadc[i];

		t=i_smoothed_potadc[i] - old_i_smoothed_potadc[i];


		if ((t>MIN_POT_ADC_CHANGE[i]) || (t<-MIN_POT_ADC_CHANGE[i]))
			track_moving_pot[i]=250;

		if (track_moving_pot[i])
		{
			track_moving_pot[i]--;

			flag_pot_changed[i]=1;

			pot_delta[i] = t;

			old_i_smoothed_potadc[i] = i_smoothed_potadc[i];
		}
	}


	for (i=0;i<NUM_CV_ADCS;i++)
	{
		smoothed_cvadc[i] = LowPassSmoothingFilter(smoothed_cvadc[i], (float)(cvadc_buffer[i]+CV_CALIBRATION_OFFSET[i]), CV_LPF_COEF[i]);
		i_smoothed_cvadc[i] = (int16_t)smoothed_cvadc[i];
		if (i_smoothed_cvadc[i] < 0) i_smoothed_cvadc[i] = 0;
		if (i_smoothed_cvadc[i] > 4095) i_smoothed_cvadc[i] = 4095;

		if (global_mode[CALIBRATE])
		{
			smoothed_rawcvadc[i] = LowPassSmoothingFilter(smoothed_rawcvadc[i], (float)(cvadc_buffer[i]), CV_LPF_COEF[i]);
			i_smoothed_rawcvadc[i] = (int16_t)smoothed_rawcvadc[i];
			if (i_smoothed_rawcvadc[i] < 0) i_smoothed_rawcvadc[i] = 0;
			if (i_smoothed_rawcvadc[i] > 4095) i_smoothed_rawcvadc[i] = 4095;
		}

		t=i_smoothed_cvadc[i] - old_i_smoothed_cvadc[i];
		if ((t>MIN_CV_ADC_CHANGE[i]) || (t<-MIN_CV_ADC_CHANGE[i]))
		{
			cv_delta[i] = t;
			old_i_smoothed_cvadc[i] = i_smoothed_cvadc[i];
		}
	}

}




void update_params(void)
{
	uint8_t channel;


	for (channel=0;channel<2;channel++)
	{



	}
}


//
// Handle all flags to change modes: INF, REV, and ping or div/mult time
//
void process_mode_flags(uint8_t channel)
{
	if (!disable_mode_changes)
	{

		if (flag_rev_change[channel])
		{

		}
	}


}

