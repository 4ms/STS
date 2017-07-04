/*
 * flash_user.h
 *
 *  Created on: Mar 4, 2016
 *      Author: design
 */

#ifndef FLASH_USER_H_
#define FLASH_USER_H_
#include <stm32f4xx.h>



void set_firmware_version(void);
void factory_reset(void);
uint32_t load_flash_params(void);
void save_flash_params(uint8_t num_led_blinks);
void copy_system_calibrations_into_staging(void);
void write_all_system_calibrations_to_FLASH(void);
void read_all_system_calibrations_from_FLASH(void);

#endif /* FLASH_USER_H_ */
