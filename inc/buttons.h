/*
 * buttons.h
 *
 *  Created on: Aug 18, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>
#include "globals.h"

//Bank's + Rec's at boot, or in Sys Mode
#define BOOTLOADER_BUTTONS (\
			!PLAY1BUT && \
		RECBUT &&\
			!EDIT_BUTTON &&\
			!REV1BUT &&\
		BANK1BUT && \
		BANKRECBUT &&\
			!PLAY2BUT && \
		BANK2BUT && \
			!REV2BUT\
		)

//All buttons except Banks and Edit (optional), during runtime
#define SYSMODE_BUTTONS (\
		REV1BUT &&\
		REV2BUT &&\
			!BANK1BUT &&\
			!BANK2BUT &&\
		PLAY1BUT &&\
		PLAY2BUT &&\
		RECBUT &&\
		BANKRECBUT \
		)

#define SYSMODE_BUTTONS_MASK (\
		(1<<Rev1) |\
		(1<<Rev2) |\
		(1<<Play1) |\
		(1<<Play2) |\
		(1<<Rec) |\
		(1<<RecBank) \
		)

#define NO_BUTTONS (\
		!REV1BUT &&\
		!REV2BUT &&\
		!BANK1BUT &&\
		!BANK2BUT &&\
		!PLAY1BUT &&\
		!PLAY2BUT &&\
		!EDIT_BUTTON &&\
		!RECBUT &&\
		!BANKRECBUT \
		)


//Rev's + Rec's at boot
#define RAMTEST_BUTTONS (\
		REV1BUT &&\
		REV2BUT &&\
		BANKRECBUT &&\
		RECBUT &\
		!PLAY1BUT &&\
		!PLAY2BUT \
		)

#define HARDWARETEST_BUTTONS RAMTEST_BUTTONS

//Play's + Rec's at boot
#define ENTER_CALIBRATE_BUTTONS (\
		PLAY1BUT &&\
		PLAY2BUT &&\
		RECBUT &&\
		BANKRECBUT &&\
		!EDIT_BUTTON &&\
		!BANK1BUT &&\
		!BANK2BUT \
		)

//All buttons during system mode
#define FACTORY_RESET_BUTTONS (\
		PLAY1BUT &&\
		PLAY2BUT &&\
		REV1BUT &&\
		REV2BUT &&\
		RECBUT &&\
		BANKRECBUT &&\
		EDIT_BUTTON &&\
		BANK1BUT &&\
		BANK2BUT \
		)

//Rec and RecBank during cal mode
#define SAVE_CALIBRATE_BUTTONS (\
		!PLAY1BUT &&\
		!PLAY2BUT &&\
		!BANK1BUT &&\
		!BANK2BUT &&\
		RECBUT &&\
		BANKRECBUT &&\
		!REV1BUT &&\
		!REV2BUT &&\
		!EDIT_BUTTON \
		)

//Play1 + Bank1 at boot
#define TEST_LED_BUTTONS (\
		PLAY1BUT &&\
		!PLAY2BUT &&\
		BANK1BUT &&\
		!BANK2BUT &&\
		!RECBUT &&\
		!BANKRECBUT &&\
		!EDIT_BUTTON \
		)

enum Buttons {
	Play1,
	Play2,
	Bank1,
	Bank2,
	Rec,
	RecBank,
	Rev1,
	Rev2,
	Edit,
	NUM_BUTTONS
};


enum ButtonStates {
	UNKNOWN,
	UP,
	DOWN,
	SHORT_PRESSED = BASE_SAMPLE_RATE/2, /* 0.5sec */
	MED_PRESS_REPEAT = BASE_SAMPLE_RATE, /* 1.0 sec */
	MED_PRESSED = BASE_SAMPLE_RATE*2, /* 2.0sec */
	LONG_PRESSED = BASE_SAMPLE_RATE*4 /* 4sec */
};


void init_buttons(void);
uint8_t all_buttons_except(enum ButtonStates state, uint32_t button_ignore_mask);
uint8_t all_buttons_atleast(enum ButtonStates state, uint32_t button_mask);


#define Button_Debounce_IRQHandler TIM4_IRQHandler
