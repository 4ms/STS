/*
 * rgb_leds.h - Handles the RGB LEDs of the buttons
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#pragma once

#include <stm32f4xx.h>

#define ButtonLED_IRQHandler TIM1_UP_TIM10_IRQHandler



enum Buttons_LEDs {
	RecBankButtonLED,
	RecButtonLED,
	Reverse1ButtonLED,
	Play1ButtonLED,
	Bank1ButtonLED,
	Bank2ButtonLED,
	Play2ButtonLED,
	Reverse2ButtonLED,

	NUM_RGBBUTTONS
};


void init_buttonLEDs(void);
void set_ButtonLED_byRGB(uint8_t LED_id, uint16_t red,  uint16_t green,  uint16_t blue);
void set_ButtonLED_byPalette(uint8_t LED_id, uint16_t paletteIndex);
void set_ButtonLED_byPaletteFade(uint8_t LED_id, uint16_t paletteIndexA, uint16_t paletteIndexB, float fade);
void display_one_ButtonLED(uint8_t LED_id);
void display_all_ButtonLEDs(void);

void chase_all_buttonLEDs(uint32_t del);
void test_all_buttonLEDs(void);
void all_buttonLEDs_off(void);

void update_one_ButtonLED(uint8_t LED_id);
void update_all_ButtonLEDs(void);

void display_bank_blink(uint8_t ButLEDnum, uint8_t bank_to_display, uint32_t t);
