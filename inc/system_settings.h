/*
 * system_settings.h
 *
 *  Created on: Apr 26, 2016
 *      Author: design
 */

#ifndef SYSTEM_SETTINGS_H_
#define SYSTEM_SETTINGS_H_

#include <stm32f4xx.h>
#include "ff.h"

void enter_system_mode(void);
void exit_system_mode(uint8_t do_save);
void update_system_mode(void);
void update_system_mode_leds(void);
void update_system_mode_button_leds(void);

void set_default_user_settings(void);
FRESULT save_user_settings(void);
FRESULT read_user_settings(void);


#endif /* SYSTEM_SETTINGS_H_ */
