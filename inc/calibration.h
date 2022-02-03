/*
 * calibration.h - Routines for setting system calibrations (tracking comp, adc/dac offsets, pot detents, etc..) 
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

#include "globals.h"
#include "rgb_leds.h"
#include <stm32f4xx.h>

//Important! Only add new fields to the end of the SystemCalibrations struct. The order of the fields is crucial for reading from FLASH
typedef struct SystemCalibrations {
	uint32_t major_firmware_version;
	uint32_t minor_firmware_version;
	int32_t cv_calibration_offset[8];
	int32_t codec_adc_calibration_dcoffset[2];
	int32_t codec_dac_calibration_dcoffset[2];
	uint32_t led_brightness;
	float tracking_comp[NUM_PLAY_CHAN];
	int32_t pitch_pot_detent_offset[2];

	float rgbled_adjustments[NUM_RGBBUTTONS][3];

} SystemCalibrations;

void set_default_calibration_values(void);
void auto_calibrate_cv_jacks(void);
void update_calibration(uint8_t user_cal_mode);
void update_calibration_button_leds(void);
void update_calibrate_leds(void);
