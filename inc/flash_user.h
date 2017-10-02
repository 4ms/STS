/*
 * flash_user.h
 *
 *  Created on: Mar 4, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

void set_firmware_version(void);
void factory_reset(void);
uint32_t load_flash_params(void);
void save_flash_params(uint8_t num_led_blinks);
void copy_system_calibrations_into_staging(void);
void write_all_system_calibrations_to_FLASH(void);
void read_all_system_calibrations_from_FLASH(void);
void apply_firmware_specific_adjustments(void);


