/*
 * buttons.h - Handles all button presses (deboucing, short/long holds, etc)
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

#include "globals.h"
#include <stm32f4xx.h>

//Bank's + Rec's at boot, or in Sys Mode
#define BOOTLOADER_BUTTONS                                                                                             \
	(!PLAY1BUT && RECBUT && !EDIT_BUTTON && !REV1BUT && BANK1BUT && BANKRECBUT && !PLAY2BUT && BANK2BUT && !REV2BUT)

//All buttons except Banks and Edit (optional), during runtime
#define SYSMODE_BUTTONS (REV1BUT && REV2BUT && !BANK1BUT && !BANK2BUT && PLAY1BUT && PLAY2BUT && RECBUT && BANKRECBUT)

#define SYSMODE_BUTTONS_MASK ((1 << Rev1) | (1 << Rev2) | (1 << Play1) | (1 << Play2) | (1 << Rec) | (1 << RecBank))

#define NO_BUTTONS                                                                                                     \
	(!REV1BUT && !REV2BUT && !BANK1BUT && !BANK2BUT && !PLAY1BUT && !PLAY2BUT && !EDIT_BUTTON && !RECBUT && !BANKRECBUT)

//Rev's + Rec's at boot
#define RAMTEST_BUTTONS (REV1BUT && REV2BUT && BANKRECBUT && RECBUT & !PLAY1BUT && !PLAY2BUT)

#define HARDWARETEST_BUTTONS RAMTEST_BUTTONS

//Play's + Rec's at boot
#define ENTER_CALIBRATE_BUTTONS (PLAY1BUT && PLAY2BUT && RECBUT && BANKRECBUT && !EDIT_BUTTON && !BANK1BUT && !BANK2BUT)

//All buttons during system mode
#define FACTORY_RESET_BUTTONS                                                                                          \
	(PLAY1BUT && PLAY2BUT && REV1BUT && REV2BUT && RECBUT && BANKRECBUT && EDIT_BUTTON && BANK1BUT && BANK2BUT)

#define VIDEO_DIM_BUTTONS                                                                                              \
	(PLAY1BUT && PLAY2BUT && !REV1BUT && !REV2BUT && RECBUT && !BANKRECBUT && !EDIT_BUTTON && !BANK1BUT && !BANK2BUT)

//Rec and RecBank during cal mode
#define SAVE_CALIBRATE_BUTTONS                                                                                         \
	(!PLAY1BUT && !PLAY2BUT && !BANK1BUT && !BANK2BUT && RECBUT && BANKRECBUT && !REV1BUT && !REV2BUT && !EDIT_BUTTON)

//Top row during system mode
#define ENTER_LED_ADJUST_BUTTONS                                                                                       \
	(PLAY1BUT && PLAY2BUT && BANK1BUT && BANK2BUT && !RECBUT && !BANKRECBUT && !REV1BUT && !REV2BUT && !EDIT_BUTTON)

//Edit + Save during led adjust mode
#define SAVE_LED_ADJUST_BUTTONS                                                                                        \
	(PLAY1BUT && !PLAY2BUT && !BANK1BUT && !BANK2BUT && !RECBUT && !BANKRECBUT && !REV1BUT && !REV2BUT && EDIT_BUTTON)

//Edit + Revert during led adjust mode
#define CANCEL_LED_ADJUST_BUTTONS                                                                                      \
	(!PLAY1BUT && !PLAY2BUT && !BANK1BUT && !BANK2BUT && !RECBUT && !BANKRECBUT && !REV1BUT && REV2BUT && EDIT_BUTTON)

//Play1 + Bank1 at boot
#define TEST_LED_BUTTONS (PLAY1BUT && !PLAY2BUT && BANK1BUT && !BANK2BUT && !RECBUT && !BANKRECBUT && !EDIT_BUTTON)

enum Buttons { Play1, Play2, Bank1, Bank2, Rec, RecBank, Rev1, Rev2, Edit, NUM_BUTTONS };

enum ButtonStates {
	UNKNOWN,
	UP,
	DOWN,
	SHORT_PRESSED = ONE_SECOND / 2, /* 0.5sec */
	MED_PRESS_REPEAT = ONE_SECOND,	/* 1.0 sec */
	MED_PRESSED = ONE_SECOND * 2,	/* 2.0sec */
	LONG_PRESSED = ONE_SECOND * 4	/* 4sec */
};

void init_buttons(void);
uint8_t all_buttons_except(enum ButtonStates state, uint32_t button_ignore_mask);
uint8_t all_buttons_atleast(enum ButtonStates state, uint32_t button_mask);

#define Button_Debounce_IRQHandler TIM4_IRQHandler
