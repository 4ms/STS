/*
 * params.h
 *
 *  Created on: Mar 27, 2015
 *      Author: design
 */

#ifndef PARAMS_H_
#define PARAMS_H_
#include <stm32f4xx.h>


//
// Params are values that the user controls, often in real-time
//
// i_ChannelParams are integer values
// f_ChannelParams are float values
//

enum i_ChannelParams{
	BANK,
	SAMPLE,
	REV,
	LOOPING,
	NUM_I_PARAMS
};

enum f_ChannelParams{
	PITCH,
	START,
	LENGTH,
	NUM_F_PARAMS
};

//ChannelPots is just a shortcut to help us with params, and must match the order in adc.h
enum ChannelPots{
	PITCH_POT,
	START_POT,
	LENGTH_POT,
	SAMPLE_POT
};
enum ChannelCVs{
	PITCH_CV,
	START_CV,
	LENGTH_CV,
	SAMPLE_CV
};

//
// Channel Settings are integer values related to states of operation
//

enum ChannelSettings{
	GATETRIG_START,
	GATETRIG_REV,
	GATETRIG_ENDOUT,
	NUM_CHAN_SETTINGS
};


enum GateTrigTypes{
	GATE_MODE,
	TRIG_MODE
};

enum Stereo_Modes{
	MONO,
	STEREO_LEFT,
	STEREO_RIGHT,
	STEREO_AVERAGE
};


//
//Global
//Global Modes represent global states of functionality
//
enum GlobalModes
{
	CALIBRATE,
	SYSTEM_SETTINGS,
	MONITOR_RECORDING,
	ENABLE_RECORDING,
	EDIT_MODE,
	STEREO_MODE,
	ASSIGN_MODE,

	NUM_GLOBAL_MODES
};

void reset_cv_lowpassfilter(uint8_t cv_num);


static inline float LowPassSmoothingFilter(float current_value, float new_value, float coef)
{
	return (current_value * coef) + (new_value * (1.0f-coef));
}
float LowPassSmoothingFilter(float current_value, float new_value, float coef);


void update_params(void);
void process_adc(void);

void process_mode_flags(void);

void init_LowPassCoefs(void);
void init_params(void);
void init_modes(void);

uint8_t detent_num(uint16_t adc_val);

#define adc_param_update_IRQHandler TIM1_BRK_TIM9_IRQHandler
void adc_param_update_IRQHandler(void);

#endif /* PARAMS_H_ */
