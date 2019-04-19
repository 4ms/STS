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

extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];

void test_noise(void)
{
	uint32_t i,j;

	GPIO_InitTypeDef gpio;

	GPIO_StructInit(&gpio);

	/* Configure PC.08, PC.09, PC.10, PC.11 pins: D0, D1, D2, D3 pins */
	gpio.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOC, &gpio);

	/* Configure PD.02 CMD line */
	gpio.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOD, &gpio);

	/* Configure PC.12 pin: CLK pin */
	gpio.GPIO_Pin = GPIO_Pin_12;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &gpio);

	j=0;
	while (1)
	{
		i=potadc_buffer[0]*100;while(i--){;}
		GPIOC->BSRRL = GPIO_Pin_8;
		GPIOC->BSRRL = GPIO_Pin_9;
		GPIOC->BSRRL = GPIO_Pin_10;
		GPIOC->BSRRL = GPIO_Pin_11;
		GPIOC->BSRRL = GPIO_Pin_12;
		GPIOD->BSRRL = GPIO_Pin_2;

		i=potadc_buffer[0]*100;while(i--){;}
		GPIOC->BSRRH = GPIO_Pin_8;
		GPIOC->BSRRH = GPIO_Pin_9;
		GPIOC->BSRRH = GPIO_Pin_10;
		GPIOC->BSRRH = GPIO_Pin_11;
		GPIOC->BSRRH = GPIO_Pin_12;
		GPIOD->BSRRH = GPIO_Pin_2;

		j++;if (j==1000){
			j=0;
			i=2000000;while(i--){;}
		}
	}

}

uint8_t get_PCB_version(void)
{
	uint8_t pulled_up, pulled_down;

	GPIO_InitTypeDef gpio;
	GPIO_StructInit(&gpio);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_High_Speed;
	gpio.GPIO_Pin = PCBVERSION_A_pin;

	//Test pin when pulled up
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(PCBVERSION_A_GPIO, &gpio);

	delay_ms(20);
	pulled_up = PCBVERSION_A ? 1 : 0;

	//Test pin when pulled down
	gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(PCBVERSION_A_GPIO, &gpio);

	delay_ms(20);
	pulled_down = PCBVERSION_A ? 1 : 0;

	if (pulled_up && pulled_down) return(1); //always high: PCB version 1.1
	if (pulled_up && !pulled_down) return(0); //floating: PCB version 1.0

	if (!pulled_up && !pulled_down) return(2); //always low: reserved for future PCB version
	if (!pulled_up && pulled_down) return(0xFF); //high when pulled low, low when pulled high: invalid

	return(0xFF); //unknown version

}

void deinit_dig_inouts(void)
{
	RCC_AHB1PeriphClockCmd(ALL_GPIO_RCC, DISABLE);
	RCC_DeInit();

}
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

	gpio.GPIO_Pin = EDIT_BUTTONREF_pin;	GPIO_Init(EDIT_BUTTONREF_GPIO, &gpio);
	EDIT_BUTTONREF_OFF;

	//LEDs
	gpio.GPIO_Pin = PLAYLED1_pin;	GPIO_Init(PLAYLED1_GPIO, &gpio);
	gpio.GPIO_Pin = PLAYLED2_pin;	GPIO_Init(PLAYLED2_GPIO, &gpio);
	gpio.GPIO_Pin = SIGNALLED_pin;	GPIO_Init(SIGNALLED_GPIO, &gpio);
	gpio.GPIO_Pin = BUSYLED_pin;	GPIO_Init(BUSYLED_GPIO, &gpio);
	
	PLAYLED1_OFF;
	PLAYLED2_OFF;
	SIGNALLED_OFF;
	BUSYLED_OFF;

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

	SIGNALLED_ON;
	SIGNALLED_OFF;

	BUSYLED_ON;
	BUSYLED_OFF;

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
		// if (t==SW_MONO) {PLAYLED1_ON;PLAYLED2_OFF;SIGNALLED_OFF;}
		// else if (t==SW_LR) {PLAYLED1_OFF;PLAYLED2_ON;SIGNALLED_OFF;}
		// else if (t==SW_LINK) {SIGNALLED_ON;PLAYLED1_OFF;PLAYLED2_OFF;}

	}


}


