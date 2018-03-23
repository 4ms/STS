/*
 * timekeeper.h
 *
 *  Created on: Mar 29, 2015
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

void init_timekeeper(void);

void init_ButtonDebounce_IRQ(void);
void init_adc_param_update_IRQ(void);
void init_LED_PWM_IRQ(void);
void init_inputread_IRQ(void);
void init_TrigJackDebounce_IRQ(void);
void init_ButtonLED_IRQ(void);

void init_SDIO_read_IRQ(void);


