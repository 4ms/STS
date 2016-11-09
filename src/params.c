/*
 * params.c
 *
 *  Created on: Mar 27, 2015
 *      Author: design
 */


#include "globals.h"
#include "adc.h"
#include "dig_pins.h"
#include "params.h"
#include "buttons.h"
#include "sampler.h"
#include "rgb_leds.h"

#include "equal_pow_pan_padded.h"
#include "exp_1voct.h"
#include "log_taper_padded.h"


extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern uint8_t disable_mode_changes;

float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
uint8_t settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

float	global_param[NUM_GLOBAL_PARAMS];
uint8_t	global_mode[NUM_GLOBAL_MODES];

uint8_t flags[NUM_FLAGS];
uint8_t flag_pot_changed[NUM_POT_ADCS];

uint8_t recording_enabled;
uint8_t is_recording;


extern uint8_t ButLED_state[NUM_RGBBUTTONS];




 /*** Move to adc.c interrupts ***/
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

/* end move to adc.c */


void init_params(void)
{
	uint8_t channel,i;

	for (channel=0;channel<NUM_PLAY_CHAN;channel++){
		f_param[channel][PITCH] 	= 1.0;
		f_param[channel][START] 	= 0.0;
		f_param[channel][LENGTH] 	= 1.0;

		f_param[channel][TRACKING_COMP] = 1.00;

		i_param[channel][BANK] 		= 0;
		i_param[channel][SAMPLE] 	= 0;
		i_param[channel][REV] 		= 0;
	}

	i_param[REC][BANK] = 0;
	i_param[REC][SAMPLE] = 0;

	global_param[SLOW_FADE_INCREMENT] = 0.001;
	global_param[LED_BRIGHTNESS] = 4;

	for (i=0;i<NUM_FLAGS;i++)
	{
		flags[i]=0;
	}
}

//initializes modes that aren't read from flash ram
void init_modes(void)
{
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
		smoothed_potadc[i]		=0;
		old_i_smoothed_potadc[i]=0;
		i_smoothed_potadc[i]	=0x7FFF;
		pot_delta[i]			=0;
	}
	for (i=0;i<NUM_CV_ADCS;i++)
	{
		smoothed_cvadc[i]		=0;
		smoothed_rawcvadc[i]	=0;
		old_i_smoothed_cvadc[i]	=0;
		i_smoothed_cvadc[i]		=0x7FFF;
		i_smoothed_rawcvadc[i]	=0x7FFF;
		cv_delta[i]				=0;
	}
}


void process_adc(void)
{
	uint8_t i;
	int32_t t;

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
	uint8_t old_val;

	recording_enabled=1;

	for (channel=0;channel<2;channel++)
	{
		//Pots
		f_param[channel][LENGTH] 	= smoothed_potadc[LENGTH_POT*2+channel] / 4096.0;
		f_param[channel][START] 	= smoothed_potadc[START_POT*2+channel] / 4096.0;
		f_param[channel][PITCH] 	= (smoothed_potadc[PITCH_POT*2+channel] / 2048.0) - 1.0;

		old_val = i_param[channel][SAMPLE];

		i_param[channel][SAMPLE] 	= detent_num(i_smoothed_potadc[SAMPLE_POT*2+channel]);

		if (old_val != i_param[channel][SAMPLE])
			flags[PlaySample1Changed+channel*2] = 1;

	}

	old_val = i_param[REC][SAMPLE];
	i_param[REC][SAMPLE] 			= detent_num(i_smoothed_potadc[RECSAMPLE_POT]);

	if (old_val != i_param[REC][SAMPLE])
		flags[RecSampleChanged] = 1;


}


//
// Handle all flags to change modes
//
void process_mode_flags(void)
{
	if (!disable_mode_changes)
	{

		if (flags[Play1Trig])
		{
			flags[Play1Trig]=0;
			toggle_playing(0);
		}

		if (flags[Play2Trig])
		{
			flags[Play2Trig]=0;
			toggle_playing(1);
		}

		if (flags[RecTrig]==1)
		{
			flags[RecTrig]=0;
			toggle_recording();
		}

		if (flags[Rev1Trig])
		{
			flags[Rev1Trig]=0;
			if (i_param[0][REV])
			{
				i_param[0][REV]=0;
				ButLED_state[Reverse1ButtonLED] = 0;
			}
			else {
				i_param[0][REV]=1;
				ButLED_state[Reverse1ButtonLED] = 1;
			}
		}
		if (flags[Rev2Trig])
		{
			flags[Rev2Trig]=0;
			if (i_param[1][REV])
			{
				i_param[1][REV]=0;
				ButLED_state[Reverse2ButtonLED] = 0;
			}
			else {
				i_param[1][REV]=1;
				ButLED_state[Reverse2ButtonLED] = 1;
			}
		}

	}
}


uint8_t detent_num(uint16_t adc_val)
{
	if (adc_val<=91)
		return(0);
	else if (adc_val<=310)
		return(1);
	else if (adc_val<=565)
		return(2);
	else if (adc_val<=816)
		return(3);
	else if (adc_val<=1062)
		return(4);
	else if (adc_val<=1304)
		return(5);
	else if (adc_val<=1529)
		return(6);
	else if (adc_val<=1742)
		return(7);
	else if (adc_val<=1950)
		return(8);
	else if (adc_val<=2157) // Center
		return(9);
	else if (adc_val<=2365)
		return(10);
	else if (adc_val<=2580)
		return(11);
	else if (adc_val<=2806)
		return(12);
	else if (adc_val<=3044)
		return(13);
	else if (adc_val<=3289)
		return(14);
	else if (adc_val<=3537)
		return(15);
	else if (adc_val<=3790)
		return(16);
	else if (adc_val<=4003)
		return(17);
	else
		return(18);

}



void adc_param_update_IRQHandler(void)
{

	if (TIM_GetITStatus(TIM9, TIM_IT_Update) != RESET) {


		process_adc();

		if (global_mode[CALIBRATE])
		{
			//update_calibration();
			//update_calibrate_leds();
		}
		else
			update_params();

		process_mode_flags();

		if (global_mode[SYSTEM_SETTINGS])
		{
			//update_system_settings();
			//update_system_settings_leds();
		}


		//check_entering_system_mode();


		TIM_ClearITPendingBit(TIM9, TIM_IT_Update);

	}
}

