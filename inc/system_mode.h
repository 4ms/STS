/*
 * system_mode.h
 *
 *  Created on: Apr 26, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

void enter_system_mode(void);
void exit_system_mode(uint8_t do_save);
void update_system_mode(void);
void update_system_mode_leds(void);
void update_system_mode_button_leds(void);
void save_globals_undo_state(void);
void restore_globals_undo_state(void);



