/*
 * params.h
 *
 *  Created on: Mar 27, 2015
 *      Author: design
 */

#ifndef PARAMS_H_
#define PARAMS_H_
#include <stm32f4xx.h>

/* ????????????
 * ChanParam_f: float parameters for a channel (play1, play2, rec): pitch, startpos, length, tracking
 * ChanParam_i: integer parameters: bank, sample, gatetrig_start, gatetrig_rev, gatetrig_endout
 * ChanState:	play state (silent, prebuffering, playing, fade down...);
 * ChanMode:	reverse mode (on, off);
 */
//
// ChannelParams
// Params are float values related to pots and CV jacks
//

enum ChannelParams{
	PITCH,
	START,
	LENGTH,
	TRACKING_COMP,	//TRACKING_COMP: compensation for 1V/oct tracking
	NUM_PARAMS
};

enum ChannelPots{
	PITCH_POT,
	START_POT,
	LENGTH_POT,
	SAMPLE_POT
};


//
// Channel Modes
// Modes are integer values (often) related to switches or settings
//

enum ChannelModes{
	BANK,
	SAMPLE,
	REV,

	GATETRIG_START,
	GATETRIG_REV,
	GATETRIG_ENDOUT,
	NUM_CHAN_MODES
};

#define REC NUM_CHAN

enum GateTrigModes{
	GATE_MODE,
	TRIG_MODE
};



//
//Global Modes
//Global Modes represent global states of functionality
//
enum GlobalModes
{
	CALIBRATE,
	SYSTEM_SETTINGS,

	REC_GATETRIG,
	NUM_GLOBAL_MODES
};

enum GlobalParams
{
	LED_BRIGHTNESS,
	SLOW_FADE_INCREMENT,
	NUM_GLOBAL_PARAMS
};


void update_params(void);
void process_adc(void);

void process_mode_flags(void);

void init_LowPassCoefs(void);
void init_params(void);
void init_modes(void);

#define adc_param_update_IRQHandler TIM1_BRK_TIM9_IRQHandler
void adc_param_update_IRQHandler(void);

#endif /* PARAMS_H_ */
