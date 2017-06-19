/*
 *	button_knob_combo.h
 *
 *	Created on: Jun 15, 2017
 * 	Author: Dan Green (danngreen1@gmail.com)
 */

#ifndef INC_BUTTON_KNOB_COMBO_H
#define INC_BUTTON_KNOB_COMBO_H
#include <stm32f4xx.h>


enum ButtonKnobCombo_Buttons{
	bkc_Bank1,
	bkc_Bank2,

	NUM_BUTTON_KNOB_COMBO_BUTTONS
};

enum ButtonKnobCombo_Knobs{
	bkc_Sample1,
	bkc_Sample2,

	NUM_BUTTON_KNOB_COMBO_KNOBS
};

enum ComboStates{
	COMBO_INACTIVE,
	COMBO_ACTIVE,
	COMBO_LATCHED, //combo is inactive but old pot data is still latched


	COMBO_UNKNOWN
};

typedef struct ButtonKnobCombo{

	uint32_t 			latched_value;
	enum ComboStates 	combo_state;
	uint32_t			hover_value;

} ButtonKnobCombo;


#endif /* INC_BUTTON_KNOB_COMBO_H */
