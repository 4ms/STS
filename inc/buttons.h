/*
 * buttons.h
 *
 *  Created on: Aug 18, 2016
 *      Author: design
 */

#ifndef BUTTONS_H_
#define BUTTONS_H_
#include <stm32f4xx.h>

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

//All buttons except Banks, during runtime
#define SYSMODE_BUTTONS (\
		REV1BUT &&\
		REV2BUT &&\
			!BANK1BUT &&\
			!BANK2BUT &&\
		PLAY1BUT &&\
		PLAY2BUT &&\
		EDIT_BUTTON &&\
		RECBUT &&\
		BANKRECBUT \
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


//Rev's + Bank's + Rec's at boot
#define RAMTEST_BUTTONS (\
		REV1BUT &&\
		REV2BUT &&\
		BANK1BUT &&\
		BANK2BUT &&\
		BANKRECBUT &&\
		RECBUT &\
		!PLAY1BUT &&\
		!PLAY2BUT \
		)

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

//Play's + Banks's during cal mode
#define SAVE_CALIBRATE_BUTTONS (\
		PLAY1BUT &&\
		PLAY2BUT &&\
		BANK1BUT &&\
		BANK2BUT &&\
		!RECBUT &&\
		!BANKRECBUT &&\
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
	SHORT_PRESSED = 22050, /* 0.5sec */
	MED_PRESS_REPEAT = 44100, /* 1.0 sec */
	MED_PRESSED = 88200, /* 2.0sec */
	LONG_PRESSED = 176400 /* 4sec */
};


void init_buttons(void);

#define Button_Debounce_IRQHandler TIM4_IRQHandler

#endif /* BUTTONS_H_ */
