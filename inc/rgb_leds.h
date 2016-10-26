/*
 * rgb_leds.h
 *
 *  Created on: Oct 26, 2016
 *      Author: Dan Green
 */

#ifndef SRC_RGB_LEDS_H_
#define SRC_RGB_LEDS_H_

#include <stm32f4xx.h>

#define NUM_RGBBUTTONS 8

void test_all_buttonLEDs(void);
void update_one_ButtonLED(uint8_t ButtonLED_number);
void update_all_ButtonLEDs(void);


#endif /* SRC_RGB_LEDS_H_ */
