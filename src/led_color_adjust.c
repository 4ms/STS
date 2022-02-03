/*
 * led_color_adjust.c - handles color adjustments for leds
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

#include "led_color_adjust.h"
#include "adc.h"
#include "buttons.h"
#include "calibration.h"
#include "dig_pins.h"
#include "globals.h"
#include "params.h"
#include "res/LED_palette.h"
#include "rgb_leds.h"
#include "user_flash_settings.h"

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint8_t flags[NUM_FLAGS];

extern SystemCalibrations *system_calibrations;
extern enum ButtonStates button_state[NUM_BUTTONS];
extern uint16_t i_smoothed_potadc[NUM_POT_ADCS];

extern enum Buttons_LEDs Button_LED_map[NUM_RGBBUTTONS];

static const uint8_t NO_LED_SELECTED = 0xFF;

uint32_t test_color_id_small, test_color_id_big;

uint32_t save_led_adjust_buttons_down = 0;
uint32_t cancel_led_adjust_buttons_down = 0;

void set_default_led_color_adjust(void) {
	uint8_t i;

	for (i = 0; i < NUM_RGBBUTTONS; i++) {
		system_calibrations->rgbled_adjustments[i][0] = 1.0;
		system_calibrations->rgbled_adjustments[i][1] = 1.0;
		system_calibrations->rgbled_adjustments[i][2] = 1.0;
	}
}

void init_led_color_adjust(void) {
	uint8_t i;

	save_led_adjust_buttons_down = 0;
	cancel_led_adjust_buttons_down = 0;

	//Turn all buttons to solid red
	for (i = 0; i < NUM_RGBBUTTONS; i++)
		set_ButtonLED_byPalette(i, RED);
	display_all_ButtonLEDs();

	delay_ms(500);

	//Turn all buttons to solid green
	for (i = 0; i < NUM_RGBBUTTONS; i++)
		set_ButtonLED_byPalette(i, GREEN);
	display_all_ButtonLEDs();

	delay_ms(500);

	//Turn all buttons to solid blue
	for (i = 0; i < NUM_RGBBUTTONS; i++)
		set_ButtonLED_byPalette(i, BLUE);
	display_all_ButtonLEDs();

	delay_ms(500);

	//Turn all buttons to solid white
	test_color_id_small = WHITE;
	test_color_id_big = WHITE;

	for (i = 0; i < NUM_RGBBUTTONS; i++) {
		if (i == Play1ButtonLED || i == Play2ButtonLED)
			set_ButtonLED_byPalette(i, test_color_id_big);
		else
			set_ButtonLED_byPalette(i, test_color_id_small);
	}

	display_all_ButtonLEDs();

	global_mode[LED_COLOR_ADJUST] = 1;
}

float diff(uint16_t a, uint16_t b) {
	return (a > b) ? (a - b) : (b - a);
}

float fdiff(float a, float b) {
	return (a > b) ? (a - b) : (b - a);
}

#define TOP_INPUT_MAX 4095.0
#define TOP_INPUT_PLATEAU 2070.0
#define TOP_OUTPUT_MAX 6.0
#define TOP_OUTPUT_MIN 1.0

#define BOTTOM_INPUT_MIN 0.0
#define BOTTOM_INPUT_PLATEAU 2030.0
#define BOTTOM_OUTPUT_MIN 1.0 / 6.0
#define BOTTOM_OUTPUT_MAX 1.0

#define TOP_DIVIDER ((TOP_MAX - TOP_PLATEAU) / (TOP_OUTPUT_MAX - TOP_OUTPUT_MIN))
#define TOP_ADDER TOP_OUTPUT_MIN

#define BOTTOM_DIVIDER ((BOTTOM_INPUT_PLATEAU - BOTTOM_INPUT_MIN) / ((1.0 / BOTTOM_OUTPUT_MIN) - BOTTOM_OUTPUT_MAX))

float adjust_amount(uint16_t potval) {
	//(4095-2070)/5 = 405
	if (potval > 2070)
		return (1.0 + ((float)(potval - 2070) / 405.5));
	if (potval >= 2030)
		return 1.0;
	return (1.0 / (1.0 + (float)(2030 - potval) / 406.0));
}

void process_led_color_adjust_mode(void) {
	uint8_t i;
	static uint8_t selected_led_id = 0xFF;
	static uint8_t change_color_armed = 0;
	static uint16_t r_pot = 2048, g_pot = 2048, b_pot = 2048;
	static uint8_t armed = 0;

	float old_r = 0.0, old_g = 0.0, old_b = 0.0;

	PLAYLED1_OFF;
	SIGNALLED_OFF;
	BUSYLED_OFF;
	PLAYLED2_OFF;

	//Read all buttons in order and pick the first one detected as pressed down
	for (i = 0; i < NUM_RGBBUTTONS; i++) {
		if (button_state[Button_LED_map[i]] >= DOWN) {
			if (armed)
				selected_led_id = i;

			break;
		}
	}
	if (i == NUM_RGBBUTTONS)
		armed = 1; //will happen when all buttons are up

	if (button_state[Edit] >= DOWN) {
		selected_led_id = NO_LED_SELECTED;
		change_color_armed = 1;
	} else if (button_state[Edit] == UP && change_color_armed) {
		change_color_armed = 0;
		test_color_id_small++;
		if (test_color_id_small > PINK)
			test_color_id_small = WHITE; //loop across bank colors

		for (i = 0; i < NUM_RGBBUTTONS; i++) {
			// if ((i == Play1ButtonLED || i==Play2ButtonLED))	set_ButtonLED_byPalette(i, (selected_led_id==Play1ButtonLED || selected_led_id==Play2ButtonLED) ? test_color_id_big : OFF);
			// else											set_ButtonLED_byPalette(i, (selected_led_id==Play1ButtonLED || selected_led_id==Play2ButtonLED) ? OFF : test_color_id_small);
			if ((i == Play1ButtonLED || i == Play2ButtonLED))
				set_ButtonLED_byPalette(i, test_color_id_big);
			else
				set_ButtonLED_byPalette(i, test_color_id_small);
		}

		display_all_ButtonLEDs();
	}

	if (selected_led_id != NO_LED_SELECTED) {

		// Check if pot moved
		if (diff(r_pot, i_smoothed_potadc[LENGTH1_POT]) > 10) {
			r_pot = i_smoothed_potadc[LENGTH1_POT];
			system_calibrations->rgbled_adjustments[selected_led_id][0] = adjust_amount(r_pot);
		}
		if (diff(g_pot, i_smoothed_potadc[RECSAMPLE_POT]) > 10) {
			g_pot = i_smoothed_potadc[RECSAMPLE_POT];
			system_calibrations->rgbled_adjustments[selected_led_id][1] = adjust_amount(g_pot);
		}
		if (diff(b_pot, i_smoothed_potadc[LENGTH2_POT]) > 10) {
			b_pot = i_smoothed_potadc[LENGTH2_POT];
			system_calibrations->rgbled_adjustments[selected_led_id][2] = adjust_amount(b_pot);
		}

		if (fdiff(old_r, system_calibrations->rgbled_adjustments[selected_led_id][0]) > 0.01 ||
			fdiff(old_g, system_calibrations->rgbled_adjustments[selected_led_id][1]) > 0.01 ||
			fdiff(old_b, system_calibrations->rgbled_adjustments[selected_led_id][2]) > 0.01)
		{
			old_r = system_calibrations->rgbled_adjustments[selected_led_id][0];
			old_g = system_calibrations->rgbled_adjustments[selected_led_id][1];
			old_b = system_calibrations->rgbled_adjustments[selected_led_id][2];

			display_one_ButtonLED(selected_led_id);
		}
	}

	if (SAVE_LED_ADJUST_BUTTONS) {
		save_led_adjust_buttons_down++;
		if (save_led_adjust_buttons_down == 3000) {
			save_flash_params(10);
			global_mode[LED_COLOR_ADJUST] = 0;
			flags[SkipProcessButtons] = 2; //indicate we're ready to clear the flag, once all buttons are released
		}
	} else
		save_led_adjust_buttons_down = 0;

	if (CANCEL_LED_ADJUST_BUTTONS) {
		cancel_led_adjust_buttons_down++;
		if (cancel_led_adjust_buttons_down == 3000) {
			global_mode[LED_COLOR_ADJUST] = 0;
			flags[SkipProcessButtons] = 2; //indicate we're ready to clear the flag, once all buttons are released
		}
	} else
		cancel_led_adjust_buttons_down = 0;
}
