/*
 * user_flash_settings.c - Read/write user settings stored on FLASH
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

#include "user_flash_settings.h"
#include "adc.h"
#include "calibration.h"
#include "dig_pins.h"
#include "flash.h"
#include "globals.h"
#include "leds.h"
#include "math.h"
#include "params.h"
#include "rgb_leds.h"
#include "timekeeper.h"

SystemCalibrations s_staging_user_params;
SystemCalibrations *staging_system_calibrations = &s_staging_user_params;

extern SystemCalibrations *system_calibrations;

extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern float f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];

extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];

#define FLASH_ADDR_userparams 0x08004000

#define FLASH_SYMBOL_firmwareoffset 0xAA550000

void apply_firmware_specific_adjustments(void) {
	//Upgrading from 1.0 or 1.1 to >=	1.2: reset tracking comp to 1.0
	if (FW_MAJOR_VERSION == 1 && FW_MINOR_VERSION >= 2 && system_calibrations->major_firmware_version == 1 &&
		system_calibrations->minor_firmware_version <= 1)
	{
		system_calibrations->tracking_comp[0] = 1.0;
		system_calibrations->tracking_comp[1] = 1.0;
	}
}

void set_firmware_version(void) {
	staging_system_calibrations->major_firmware_version = FW_MAJOR_VERSION;
	system_calibrations->major_firmware_version = FW_MAJOR_VERSION;

	staging_system_calibrations->minor_firmware_version = FW_MINOR_VERSION;
	system_calibrations->minor_firmware_version = FW_MINOR_VERSION;
}

void factory_reset(void) {
	uint8_t i, fail = 0;

	set_default_calibration_values();
	auto_calibrate_cv_jacks();

	set_firmware_version();

	copy_system_calibrations_into_staging();
	write_all_system_calibrations_to_FLASH();

	fail = 0;
	for (i = 0; i < NUM_CV_ADCS; i++) {
		if (system_calibrations->cv_calibration_offset[i] > 100 || system_calibrations->cv_calibration_offset[i] < -100)
			fail = 1;
	}

	if (i_smoothed_cvadc[0] > 2150 || i_smoothed_cvadc[0] < 1950 || i_smoothed_cvadc[1] > 2150 ||
		i_smoothed_cvadc[1] < 1950)
		fail = 1;

	if (fail) {
		global_mode[CALIBRATE] = 0;
		while (1)
			blink_all_lights(200); //error: did not auto-calibrate!
	}
}

uint32_t load_flash_params(void) {
	uint8_t i;
	uint8_t *src;
	uint8_t *dst;

	read_all_system_calibrations_from_FLASH(); //into staging area

	if (staging_system_calibrations->major_firmware_version <= 30) //valid firmware version
	{

		//memcopy: system_calibrations = staging_system_calibrations;
		dst = (uint8_t *)system_calibrations;
		src = (uint8_t *)staging_system_calibrations;
		for (i = 0; i < sizeof(SystemCalibrations); i++)
			*dst++ = *src++;

		return (1); //Valid firmware version found
	} else {
		set_firmware_version();

		return (0); //No calibration data found
	}
}

void save_flash_params(uint8_t num_led_blinks) {
	uint32_t i;

	copy_system_calibrations_into_staging();

	write_all_system_calibrations_to_FLASH();

	for (i = 0; i < num_led_blinks; i++) {
		SIGNALLED_ON;
		BUSYLED_ON;
		PLAYLED1_ON;
		PLAYLED2_ON;
		delay_ms(10);

		SIGNALLED_OFF;
		BUSYLED_OFF;
		PLAYLED1_OFF;
		PLAYLED2_OFF;

		delay_ms(10);
	}
}

void copy_system_calibrations_into_staging(void) {
	uint8_t i;
	uint8_t *src;
	uint8_t *dst;

	//copy: staging_system_calibrations = system_calibrations;
	dst = (uint8_t *)staging_system_calibrations;
	src = (uint8_t *)system_calibrations;
	for (i = 0; i < sizeof(SystemCalibrations); i++)
		*dst++ = *src++;
}

void write_all_system_calibrations_to_FLASH(void) {
	uint8_t i;
	uint8_t *addr;

	flash_begin_open_program();

	flash_open_erase_sector(FLASH_ADDR_userparams);

	staging_system_calibrations->major_firmware_version += FLASH_SYMBOL_firmwareoffset;

	addr = (uint8_t *)staging_system_calibrations;

	for (i = 0; i < sizeof(SystemCalibrations); i++) {
		flash_open_program_byte(*addr++, FLASH_ADDR_userparams + i);
	}

	flash_end_open_program();
}

void read_all_system_calibrations_from_FLASH(void) {
	uint8_t i;
	uint8_t *ptr;
	uint8_t invalid_fw_version = 0;

	ptr = (uint8_t *)staging_system_calibrations;

	for (i = 0; i < sizeof(SystemCalibrations); i++) {
		*ptr++ = flash_read_byte(FLASH_ADDR_userparams + i);
	}

	staging_system_calibrations->major_firmware_version -= FLASH_SYMBOL_firmwareoffset;

	if (staging_system_calibrations->major_firmware_version > 30)
		invalid_fw_version = 1;

	//Validate settings:

	for (i = 0; i < 8; i++) {
		if (invalid_fw_version || staging_system_calibrations->cv_calibration_offset[i] > 100 ||
			staging_system_calibrations->cv_calibration_offset[i] < -100)
			staging_system_calibrations->cv_calibration_offset[i] = 0;
	}
	for (i = 0; i < 2; i++) {
		if (invalid_fw_version || staging_system_calibrations->codec_adc_calibration_dcoffset[i] > 4000 ||
			staging_system_calibrations->codec_adc_calibration_dcoffset[i] < -4000)
			staging_system_calibrations->codec_adc_calibration_dcoffset[i] = 0;

		if (invalid_fw_version || staging_system_calibrations->codec_dac_calibration_dcoffset[i] > 4000 ||
			staging_system_calibrations->codec_dac_calibration_dcoffset[i] < -4000)
			staging_system_calibrations->codec_dac_calibration_dcoffset[i] = 0;

		if (invalid_fw_version || staging_system_calibrations->tracking_comp[i] > 2.0f ||
			staging_system_calibrations->tracking_comp[i] < 0.5f ||
			(*(uint32_t *)(&staging_system_calibrations->tracking_comp[i]) == 0xFFFFFFFF))
			staging_system_calibrations->tracking_comp[i] = 1.0;
	}

	if (invalid_fw_version || staging_system_calibrations->led_brightness > 15 ||
		staging_system_calibrations->led_brightness < 1)
		staging_system_calibrations->led_brightness = 4;

	for (i = 0; i < NUM_RGBBUTTONS; i++) {
		if (isnan(staging_system_calibrations->rgbled_adjustments[i][0]) ||
			staging_system_calibrations->rgbled_adjustments[i][0] > 5.5f ||
			staging_system_calibrations->rgbled_adjustments[i][0] < 0.1f)
			staging_system_calibrations->rgbled_adjustments[i][0] = 1.0f;

		if (isnan(staging_system_calibrations->rgbled_adjustments[i][1]) ||
			staging_system_calibrations->rgbled_adjustments[i][1] > 5.5f ||
			staging_system_calibrations->rgbled_adjustments[i][1] < 0.1f)
			staging_system_calibrations->rgbled_adjustments[i][1] = 1.0f;

		if (isnan(staging_system_calibrations->rgbled_adjustments[i][2]) ||
			staging_system_calibrations->rgbled_adjustments[i][2] > 5.5f ||
			staging_system_calibrations->rgbled_adjustments[i][2] < 0.1f)
			staging_system_calibrations->rgbled_adjustments[i][2] = 1.0f;
	}
}
