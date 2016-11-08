/*
 * rgb_leds.h
 *
 *  Created on: Oct 26, 2016
 *      Author: Dan Green
 */

#ifndef SRC_RGB_LEDS_H_
#define SRC_RGB_LEDS_H_

#include <stm32f4xx.h>

#define ButtonLED_IRQHandler TIM1_UP_TIM10_IRQHandler

enum Buttons_LEDs {
	RecBankButtonLED,
	RecButtonLED,
	Reverse1ButtonLED,
	Bank1ButtonLED,
	Play1ButtonLED,
	Play2ButtonLED,
	Bank2ButtonLED,
	Reverse2ButtonLED,
	NUM_RGBBUTTONS
};

//Play1ButtonLED+chan
//Bank1ButtonLED+chan*3
//Reverse1ButtonLED+chan*5


void test_all_buttonLEDs(void);
void update_one_ButtonLED(uint8_t ButtonLED_number);
void update_all_ButtonLEDs(void);


#endif /* SRC_RGB_LEDS_H_ */
