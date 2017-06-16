/*
 *	button_knob_combo.c
 *
 *	Created on: Jun 15, 2017
 * 	Author: Dan Green (danngreen1@gmail.com)
 */

#include "button_knob_combo.h"

ButtonKnobCombo button_knob_combo[NUM_BUTTON_KNOB_COMBOS];
// ButtonKnobCombo a_button_knob_combo[NUM_BUTTON_KNOB_COMBO_BUTTONS][NUM_BUTTON_KNOB_COMBO_KNOBS];

// void clear_combo(enum ButtonKnobCombo_Buttons button, enum ButtonKnobCombo_Knobs knob){
// 	a_button_knob_combo[button][knob].init_value = POT_VALUE_UNREAD;

// }

// void clear_buttons_combos(enum ButtonKnobCombo_Buttons button)
// {
// 	enum ButtonKnobCombo_Knobs knob;

// 	//Go through all knobs and clear all values associated with the given button
// 	for (knob=0;knob<NUM_BUTTON_KNOB_COMBO_KNOBS;knob++)
// 	{
// 		a_button_knob_combo[Bank2_Sample2].combo_active = 0;
// 		a_button_knob_combo[Bank2_Sample1].init_value = POT_VALUE_UNREAD;


// 	}

// }