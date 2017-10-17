/*
 * leds.h
 *
 *  Created on: Apr 21, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

#define LED_PWM_IRQHandler TIM2_IRQHandler

void flicker_endout(uint8_t chan, float play_time);

void blink_all_lights(uint32_t delaytime);
void chase_all_lights(uint32_t delaytime);



