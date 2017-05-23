/*
 * flash_user.h
 *
 *  Created on: Mar 4, 2016
 *      Author: design
 */

#ifndef FLASH_USER_H_
#define FLASH_USER_H_
#include <stm32f4xx.h>


typedef struct SystemSettings
{
	uint32_t	firmware_version;
	int32_t		cv_calibration_offset[8];
	int32_t		codec_adc_calibration_dcoffset[2];
	int32_t		codec_dac_calibration_dcoffset[2];
	uint32_t	led_brightness;
	float 		tracking_comp[NUM_PLAY_CHAN];

} SystemSettings;

void set_firmware_version(void);
void factory_reset(uint8_t loop_afterwards);
uint32_t load_flash_params(void);
void save_flash_params(void);
void store_params_into_sram(void);
void write_all_params_to_FLASH(void);
void read_all_params_from_FLASH(void);

#endif /* FLASH_USER_H_ */
