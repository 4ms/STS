/*
 * buttons.h
 *
 *  Created on: Aug 18, 2016
 *      Author: design
 */

#ifndef BUTTONS_H_
#define BUTTONS_H_
#include <stm32f4xx.h>


#define BOOTLOADER_BUTTONS (\
		PLAY1BUT && \
		RECBUT &&\
		EDIT_BUTTON &&\
		!REV1BUT &&\
		!BANK1BUT && \
		!BANKRECBUT &&\
		!PLAY2BUT && \
		!BANK2BUT && \
		!REV2BUT\
		)

#define ENTER_SYSMODE_BUTTONS (\
		REV1BUT &&\
		REV2BUT &&\
		BANK1BUT &&\
		BANK2BUT &&\
		PLAY1BUT &&\
		PLAY2BUT &&\
		!EDIT_BUTTON &&\
		!RECBUT &&\
		!BANKRECBUT \
		)

#define RAMTEST_BUTTONS (\
		REV1BUT &&\
		REV2BUT &&\
		BANK1BUT &&\
		BANK2BUT &&\
		BANKRECBUT &&\
		RECBUT)

#define ENTER_CALIBRATE_BUTTONS (\
		PLAY1BUT &&\
		PLAY2BUT &&\
		RECBUT &&\
		BANKRECBUT &&\
		!EDIT_BUTTON &&\
		!BANK1BUT &&\
		!BANK2BUT \
		)

#define SAVE_CALIBRATE_BUTTONS (\
		PLAY1BUT &&\
		PLAY2BUT &&\
		BANK1BUT &&\
		BANK2BUT &&\
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
	MED_PRESSED = 88200, /* 2.0sec */
	LONG_PRESSED = 176400 /* 4sec */
};


void init_buttons(void);

#define Button_Debounce_IRQHandler TIM4_IRQHandler

#endif /* BUTTONS_H_ */
