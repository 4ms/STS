/*
 *	button_knob_combo.h
 *
 *	Created on: Jun 15, 2017
 * 	Author: Dan Green (danngreen1@gmail.com)
 */

#pragma once

#include <stm32f4xx.h>

enum ButtonKnobCombo_Buttons{
	bkc_Bank1,
	bkc_Bank2,
	bkc_RecBank,

	bkc_Edit,

	bkc_Reverse1,
	bkc_Reverse2,

	NUM_BUTTON_KNOB_COMBO_BUTTONS
};

enum ButtonKnobCombo_Knobs{
	bkc_Sample1,
	bkc_Sample2,
	bkc_RecSample,

	bkc_Length2,
	bkc_StartPos1,
	bkc_StartPos2,

	NUM_BUTTON_KNOB_COMBO_KNOBS
};

//When a combo is active, the knob controls an alternative feature.
//Usually, we want the original feature to continue operating as if
//the pot had not been changed. Thus, we latch the pot value when activating
//the combo.  
//When the button is released, the alternative feature stops being controlled,
//(our other code could latch its value, if necessary)
//The original feature should stay at the latched value, which is the 
//COMBO_LATCHED state.
//Our other code can unlatch the pot value and move to COMBO_INACTIVE
//at value-crossing, or at exiting a bracket, or after a timer expires, etc.
//
enum ComboStates{
	COMBO_INACTIVE, //alt feature is inactive, do not latch data
	COMBO_ACTIVE,	//alt feature is active, pot value is latched for normal feature
	COMBO_LATCHED  //alt feature is inactive, pot value is still latched for normal feature
};

typedef struct ButtonKnobCombo{

	uint32_t 			latched_value;
	enum ComboStates 	combo_state;
	uint32_t			hover_value;
	uint8_t				value_crossed;

} ButtonKnobCombo;



