/*
 * dig_inouts.c
 */

#include <audio_sdram.h>
#include <sampler.h>
#include "globals.h"
#include "adc.h"
#include "params.h"
#include "timekeeper.h"
#include "dig_pins.h"
#include "buttons.h"




void init_dig_inouts(void){
	GPIO_InitTypeDef gpio;
	GPIO_StructInit(&gpio);


	RCC_AHB1PeriphClockCmd(ALL_GPIO_RCC, ENABLE);

	//Configure inputs
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Low_Speed;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;


	//Buttons
	gpio.GPIO_Pin = REV1BUT_pin;	GPIO_Init(REV1BUT_GPIO, &gpio);
	gpio.GPIO_Pin = REV2BUT_pin;	GPIO_Init(REV2BUT_GPIO, &gpio);
	gpio.GPIO_Pin = PLAY1BUT_pin;	GPIO_Init(PLAY1BUT_GPIO, &gpio);
	gpio.GPIO_Pin = PLAY2BUT_pin;	GPIO_Init(PLAY2BUT_GPIO, &gpio);
	gpio.GPIO_Pin = RECBUT_pin;	GPIO_Init(RECBUT_GPIO, &gpio);
	gpio.GPIO_Pin = BANK1BUT_pin;	GPIO_Init(BANK1BUT_GPIO, &gpio);
	gpio.GPIO_Pin = BANK2BUT_pin;	GPIO_Init(BANK2BUT_GPIO, &gpio);
	gpio.GPIO_Pin = BANKRECBUT_pin;	GPIO_Init(BANKRECBUT_GPIO, &gpio);
	gpio.GPIO_Pin = EDIT_BUTTON_pin;	GPIO_Init(EDIT_BUTTON_GPIO, &gpio);

	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

	//Trigger input jacks
	gpio.GPIO_Pin = PLAY1JACK_pin;	GPIO_Init(PLAY1JACK_GPIO, &gpio);
	gpio.GPIO_Pin = PLAY2JACK_pin;	GPIO_Init(PLAY2JACK_GPIO, &gpio);
	gpio.GPIO_Pin = RECTRIGJACK_pin; GPIO_Init(RECTRIGJACK_GPIO, &gpio);
	gpio.GPIO_Pin = REV1JACK_pin;	GPIO_Init(REV1JACK_GPIO, &gpio);
	gpio.GPIO_Pin = REV2JACK_pin;	GPIO_Init(REV2JACK_GPIO, &gpio);

	//Switch
	// gpio.GPIO_Pin = STEREOSW_T1_pin;	GPIO_Init(STEREOSW_T1_GPIO, &gpio);
	// gpio.GPIO_Pin = STEREOSW_T2_pin;	GPIO_Init(STEREOSW_T2_GPIO, &gpio);



	//Configure outputs
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

	//Trigger Out jacks
	gpio.GPIO_Pin = ENDOUT1_pin;	GPIO_Init(ENDOUT1_GPIO, &gpio);
	gpio.GPIO_Pin = ENDOUT2_pin;	GPIO_Init(ENDOUT2_GPIO, &gpio);

	//LEDs
	gpio.GPIO_Pin = PLAYLED1_pin;	GPIO_Init(PLAYLED1_GPIO, &gpio);
	gpio.GPIO_Pin = PLAYLED2_pin;	GPIO_Init(PLAYLED2_GPIO, &gpio);
	gpio.GPIO_Pin = CLIPLED1_pin;	GPIO_Init(CLIPLED1_GPIO, &gpio);
	gpio.GPIO_Pin = CLIPLED2_pin;	GPIO_Init(CLIPLED2_GPIO, &gpio);

	PLAYLED1_OFF;
	PLAYLED2_OFF;
	CLIPLED1_OFF;
	CLIPLED2_OFF;

	//DEBUG pins

	gpio.GPIO_Pin = DEBUG0;	GPIO_Init(DEBUG0_GPIO, &gpio);
	gpio.GPIO_Pin = DEBUG1;	GPIO_Init(DEBUG1_GPIO, &gpio);
	gpio.GPIO_Pin = DEBUG2;	GPIO_Init(DEBUG2_GPIO, &gpio);
	gpio.GPIO_Pin = DEBUG3;	GPIO_Init(DEBUG3_GPIO, &gpio);
	DEBUG0_OFF;
	DEBUG1_OFF;
	DEBUG2_OFF;
	DEBUG3_OFF;

	//JUMPERS

	gpio.GPIO_PuPd = GPIO_PuPd_UP;

//	gpio.GPIO_Pin = JUMPER_1_pin;	GPIO_Init(JUMPER_1_GPIO, &gpio);
//	gpio.GPIO_Pin = JUMPER_2_pin;	GPIO_Init(JUMPER_2_GPIO, &gpio);
//	gpio.GPIO_Pin = JUMPER_3_pin;	GPIO_Init(JUMPER_3_GPIO, &gpio);
//	gpio.GPIO_Pin = JUMPER_4_pin;	GPIO_Init(JUMPER_4_GPIO, &gpio);


}

void test_dig_inouts(void)
{
	uint32_t t;

	DEBUG0_ON;
	DEBUG0_OFF;

	DEBUG1_ON;
	DEBUG1_OFF;

	DEBUG2_ON;
	DEBUG2_OFF;

	DEBUG3_ON;
	DEBUG3_OFF;

	PLAYLED1_ON;
	PLAYLED1_OFF;

	PLAYLED2_ON;
	PLAYLED2_OFF;

	CLIPLED1_ON;
	CLIPLED1_OFF;

	CLIPLED2_ON;
	CLIPLED2_OFF;

	ENDOUT1_ON;
	ENDOUT1_OFF;

	ENDOUT2_ON;
	ENDOUT2_OFF;


	t=REV1BUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=REV2BUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=PLAY1BUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=PLAY2BUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=RECBUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=BANK1BUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=BANK2BUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=BANKRECBUT;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=REV1JACK;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=REV2JACK;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=PLAY1JACK;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=PLAY2JACK;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	t=RECTRIGJACK;
	if (t) PLAYLED1_ON;
	PLAYLED1_OFF;

	while (0){
		// t=STEREOSW;
		// if (t==SW_MONO) {PLAYLED1_ON;PLAYLED2_OFF;CLIPLED1_OFF;}
		// else if (t==SW_LR) {PLAYLED1_OFF;PLAYLED2_ON;CLIPLED1_OFF;}
		// else if (t==SW_LINK) {CLIPLED1_ON;PLAYLED1_OFF;PLAYLED2_OFF;}

	}


}


