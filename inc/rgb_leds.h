/*
 * rgb_leds.h
 *
 *  Created on: Oct 26, 2016
 *      Author: Dan Green
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
void set_ButtonLED_byRGB(uint8_t ButtonLED_number, uint16_t red,  uint16_t green,  uint16_t blue);
void set_ButtonLED_byPalette(uint8_t ButtonLED_number, uint16_t paletteIndex);
void set_ButtonLED_byPaletteFade(uint8_t ButtonLED_number, uint16_t paletteIndexA, uint16_t paletteIndexB, float fade);
void display_one_ButtonLED(uint8_t ButtonLED_number);
void display_all_ButtonLEDs(void);

void chase_all_buttonLEDs(uint32_t del);
void fade_all_buttonLEDs(void);
void all_buttonLEDs_off(void);

void update_one_ButtonLED(uint8_t ButtonLED_number);

uint32_t display_bank_blink(uint8_t ButLEDnum, uint8_t bank_to_display, uint32_t t);


