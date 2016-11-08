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
		REV1BUT &&\
		REV2BUT &&\
		BANK1BUT &&\
		BANK2BUT &&\
		0)

#define ENTER_SYSMODE_BUTTONS (\
		REV1BUT &&\
		REV2BUT &&\
		BANK1BUT &&\
		BANK2BUT)

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
		BANKRECBUT)


enum Buttons {
	Play1,
	Play2,
	Bank1,
	Bank2,
	Rec,
	RecBank,
	Rev1,
	Rev2,
	NUM_BUTTONS
};

enum ButtonStates {
	UP,
	DOWN
};



#define Button_Debounce_IRQHandler TIM4_IRQHandler

#endif /* BUTTONS_H_ */
