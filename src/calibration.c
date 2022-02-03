/*
 * calibration.c - Routines for setting system calibrations (tracking comp, adc/dac offsets, pot detents, etc..) 
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

#include "calibration.h"
#include "adc.h"
#include "buttons.h"
#include "dig_pins.h"
#include "globals.h"
#include "led_color_adjust.h"
#include "params.h"
#include "res/LED_palette.h"
#include "rgb_leds.h"
#include "user_flash_settings.h"

SystemCalibrations s_system_calibrations;
SystemCalibrations *system_calibrations = &s_system_calibrations;

extern volatile uint32_t sys_tmr;

extern int16_t bracketed_potadc[NUM_POT_ADCS];

extern int16_t i_smoothed_potadc[NUM_POT_ADCS];
extern int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint8_t flags[NUM_FLAGS];

uint8_t playbut2_color, playbut1_color;

void set_default_calibration_values(void) {
	uint8_t i;

	system_calibrations->codec_dac_calibration_dcoffset[0] = -1800;
	system_calibrations->codec_dac_calibration_dcoffset[1] = -1800;

	system_calibrations->codec_adc_calibration_dcoffset[0] = 0;
	system_calibrations->codec_adc_calibration_dcoffset[1] = 0;

	system_calibrations->tracking_comp[0] = 1.0;
	system_calibrations->tracking_comp[1] = 1.0;

	system_calibrations->led_brightness = 4;

	system_calibrations->pitch_pot_detent_offset[0] = 30;
	system_calibrations->pitch_pot_detent_offset[1] = 30;

	for (i = 0; i < NUM_CV_ADCS; i++)
		system_calibrations->cv_calibration_offset[i] = -18;

	set_default_led_color_adjust();
}

void auto_calibrate_cv_jacks(void) {
	//delay_ms(7000);
	delay();
	delay();
	delay();
	delay();

	system_calibrations->cv_calibration_offset[0] = 2048 - (i_smoothed_rawcvadc[0] & 0x0FFF);
	system_calibrations->cv_calibration_offset[1] = 2048 - (i_smoothed_rawcvadc[1] & 0x0FFF);
	system_calibrations->cv_calibration_offset[2] = -1 * (i_smoothed_rawcvadc[2] & 0x0FFF);
	system_calibrations->cv_calibration_offset[3] = -1 * (i_smoothed_rawcvadc[3] & 0x0FFF);
	system_calibrations->cv_calibration_offset[4] = -1 * (i_smoothed_rawcvadc[4] & 0x0FFF);
	system_calibrations->cv_calibration_offset[5] = -1 * (i_smoothed_rawcvadc[5] & 0x0FFF);
	system_calibrations->cv_calibration_offset[6] = -1 * (i_smoothed_rawcvadc[6] & 0x0FFF);
	system_calibrations->cv_calibration_offset[7] = -1 * (i_smoothed_rawcvadc[7] & 0x0FFF);
}

void update_calibration(uint8_t user_cal_mode) {
	static uint32_t save_calibration_buttons_down = 0;
	int32_t t;

	system_calibrations->cv_calibration_offset[0] = 2048 - (i_smoothed_rawcvadc[0] & 0x0FFF);
	system_calibrations->cv_calibration_offset[1] = 2048 - (i_smoothed_rawcvadc[1] & 0x0FFF);
	system_calibrations->cv_calibration_offset[2] = -1 * (i_smoothed_rawcvadc[2] & 0x0FFF);
	system_calibrations->cv_calibration_offset[3] = -1 * (i_smoothed_rawcvadc[3] & 0x0FFF);
	system_calibrations->cv_calibration_offset[4] = -1 * (i_smoothed_rawcvadc[4] & 0x0FFF);
	system_calibrations->cv_calibration_offset[5] = -1 * (i_smoothed_rawcvadc[5] & 0x0FFF);
	system_calibrations->cv_calibration_offset[6] = -1 * (i_smoothed_rawcvadc[6] & 0x0FFF);
	system_calibrations->cv_calibration_offset[7] = -1 * (i_smoothed_rawcvadc[7] & 0x0FFF);

	//Check if Pitch pot is not centered (either the knob is not set to the center,
	//or the detent calibration is incorrectly set
	//or the hardware is faulty)
	//Use the PLAY light to show if we detect a non-centered calibrated value
	t = bracketed_potadc[PITCH_POT * 2] + system_calibrations->pitch_pot_detent_offset[0];
	if (t > 2152 || t < 1957)
		playbut1_color = RED; //red: out of pitch=1.0 range
	else if (t > 2102 || t < 2007)
		playbut1_color = YELLOW; //yellow: warning, close to edge
	else if (t > 2068 || t < 2028)
		playbut1_color = GREEN; //green: ok: more than 20 from center
	else
		playbut1_color = WHITE; //white: within 20 of center

	t = bracketed_potadc[PITCH_POT * 2 + 1] + system_calibrations->pitch_pot_detent_offset[1];
	if (t > 2152 || t < 1957)
		playbut2_color = RED; //red: out of pitch=1.0 range
	else if (t > 2102 || t < 2007)
		playbut2_color = YELLOW; //yellow: warning, close to edge
	else if (t > 2068 || t < 2028)
		playbut2_color = GREEN; //green: ok: more than 20 from center
	else
		playbut2_color = WHITE; //white: within 20 of center

	//Chan 1 Pitch pot center position
	if (PLAY1BUT)
		system_calibrations->pitch_pot_detent_offset[0] = (2048 - i_smoothed_potadc[PITCH_POT * 2]);

	//Chan 2 Pitch pot center position
	if (PLAY2BUT)
		system_calibrations->pitch_pot_detent_offset[1] = (2048 - i_smoothed_potadc[PITCH_POT * 2 + 1]);

	if (!user_cal_mode) {
		//Chan 1 Audio Out DC offset
		if (REV1BUT)
			system_calibrations->codec_dac_calibration_dcoffset[0] = (i_smoothed_potadc[LENGTH_POT * 2] - 2048 - 1750);

		//Chan 2 Audio Out DC offset
		if (REV2BUT)
			system_calibrations->codec_dac_calibration_dcoffset[1] =
				(i_smoothed_potadc[LENGTH_POT * 2 + 1] - 2048 - 1750);
	}

	//ToDo: Create a tracking calibration mode.
	//In tracking calibration mode, user should input C0 (0.00V) into 1V/oct jack
	//When a voltage -0.5 < V < 0.5 is registered, a button lights up. Then the user taps that button
	//Then input C3 (2.00V) into 1V/oct jack
	//When a voltage 1.5 > V > 2.5 is registered, a button lights up. Then the user taps that button
	//
	//The procedure can then be repeated for the other channel
	//The difference between adc values can be used to determine the tracking_comp:
	//>>> Find x, where y = voltoct[adc_high * x] / voltoct[adc_low * x], y = 2.000 (...or 1.999< y <2.001)

	if (SAVE_CALIBRATE_BUTTONS) {
		save_calibration_buttons_down++;
		if (save_calibration_buttons_down == 3000) {
			save_flash_params(10);
			global_mode[CALIBRATE] = 0;
			flags[SkipProcessButtons] = 2; //indicate we're ready to clear the flag, once all buttons are released
		}
	} else
		save_calibration_buttons_down = 0;
}

void update_calibrate_leds(void) {
	PLAYLED2_OFF;
	PLAYLED1_OFF;
	SIGNALLED_OFF;
	BUSYLED_OFF;
}

void update_calibration_button_leds(void) {

	set_ButtonLED_byPalette(Play1ButtonLED, playbut1_color);
	set_ButtonLED_byPalette(Play2ButtonLED, playbut2_color);

	set_ButtonLED_byPalette(RecBankButtonLED, sys_tmr & 0x4000 ? LAVENDER : WHITE);
	set_ButtonLED_byPalette(RecButtonLED, sys_tmr & 0x4000 ? LAVENDER : WHITE);
	set_ButtonLED_byPalette(Reverse1ButtonLED, DIM_RED);
	set_ButtonLED_byPalette(Bank1ButtonLED, DIM_RED);
	set_ButtonLED_byPalette(Bank2ButtonLED, DIM_RED);
	set_ButtonLED_byPalette(Reverse2ButtonLED, DIM_RED);
}
