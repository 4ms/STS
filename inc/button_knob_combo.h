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
	bkn_Bank2,

	NUM_BUTTON_KNOB_COMBO_BUTTONS
};

enum ButtonKnobCombo_Knobs{
	bkc_Sample1,
	bkn_Sample2,

	NUM_BUTTON_KNOB_COMBO_KNOBS
};

enum ButtonKnobCombos{
	Bank1_Sample1,
	Bank1_Sample2,
	Bank2_Sample1,
	Bank2_Sample2,

	NUM_BUTTON_KNOB_COMBOS

};


typedef struct ButtonKnobCombo{

	uint16_t 	init_value;
	uint8_t 	combo_active;

} ButtonKnobCombo;

#define POT_VALUE_UNREAD 0xFFFF;

#endif /* INC_BUTTON_KNOB_COMBO_H */

