/*
 * leds.h
 *
 *  Created on: Apr 21, 2016
 *      Author: design
 */

#ifndef LEDS_H_
#define LEDS_H_
#include <stm32f4xx.h>

#define LED_PWM_IRQHandler TIM2_IRQHandler


void blink_all_lights(uint32_t delaytime);
void chase_all_lights(uint32_t delaytime);


#endif /* LEDS_H_ */
