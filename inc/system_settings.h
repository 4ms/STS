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

void check_entering_system_mode(void);
void update_system_settings(void);
void set_default_system_settings(void);

void update_system_settings_button_leds(void);
void update_system_settings_leds(void);
void update_system_settings(void);

FRESULT save_system_settings(void);
FRESULT read_system_settings(void);


#endif /* SYSTEM_SETTINGS_H_ */
