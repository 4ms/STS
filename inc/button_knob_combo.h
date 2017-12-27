/*
 * button_knob_combo.c - structure for keeping track of button+knob combinations
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



