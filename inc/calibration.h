/*
 * calibrate.h
 *
 *  Created on: Mar 4, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

typedef struct SystemCalibrations
{
	uint32_t	major_firmware_version;
	uint32_t	minor_firmware_version;
	int32_t		cv_calibration_offset[8];
	int32_t		codec_adc_calibration_dcoffset[2];
	int32_t		codec_dac_calibration_dcoffset[2];
	uint32_t	led_brightness;
	float 		tracking_comp[NUM_PLAY_CHAN];
	int32_t		pitch_pot_detent_offset[2];

} SystemCalibrations;

void set_default_calibration_values(void);
void auto_calibrate(void);
void update_calibration(uint8_t user_cal_mode);
void update_calibration_button_leds(void);
void update_calibrate_leds(void);


