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

#define DIGINPUT_TIM TIM4
#define DIGINPUT_TIM_RCC RCC_APB1Periph_TIM4
#define DIGINPUT_TIM_IRQn TIM4_IRQn
#define DIGINPUT_IRQHandler TIM4_IRQHandler

void init_inputread_timer(void);


#endif /* BUTTONS_H_ */
