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
// ChannelParams
// Params are float values related to pots and CV jacks
//

enum ChannelParams{
	PITCH,			//TIME: fractional value for time multiplication, integer value for time division
	START,			//LEVEL: 0..1 representing amount of main input mixed into delay loop
	LENGTH,			//REGEN: 0..1 representing amount of regeneration
	TRACKING_COMP,	//TRACKING_COMP: -2.0 .. 2.0 representing compensation for 1V/oct tracking
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


enum GateTrigModes{
	GATE_MODE,
	TRIG_MODE
};



//
//Global Modes
//Global Modes represent global states of functionality
//
enum GlobalModes{
	CALIBRATE,
	SYSTEM_SETTINGS,

	REC_GATETRIG,
	NUM_GLOBAL_MODES
};

enum GlobalParams{
	LED_BRIGHTNESS,
	SLOW_FADE_INCREMENT,
	NUM_GLOBAL_PARAMS
};


void update_params(void);
void process_adc(void);

void process_mode_flags(uint8_t channel);

void init_LowPassCoefs(void);
void init_params(void);
void init_modes(void);


#endif /* PARAMS_H_ */
