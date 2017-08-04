/*
 * system_mode.h
 *
 *  Created on: Apr 26, 2016
 *      Author: design
 */

#ifndef SYSTEM_MODE_H_
#define SYSTEM_MODE_H_

#include <stm32f4xx.h>

void enter_system_mode(void);
void exit_system_mode(uint8_t do_save);
void update_system_mode(void);
void update_system_mode_leds(void);
void update_system_mode_button_leds(void);


#endif /* SYSTEM_MODE_H_ */
