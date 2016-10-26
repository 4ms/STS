/*
 * leds.c
 */

#include <dig_pins.h>
#include "globals.h"
#include "leds.h"
#include "timekeeper.h"
#include "params.h"


//extern uint8_t mode[NUM_CHAN][NUM_CHAN_MODES];
//extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern float global_param[NUM_GLOBAL_PARAMS];

uint8_t play_led_state[NUM_CHAN]={0,0};
uint8_t clip_led_state[NUM_CHAN]={0,0};






void update_channel_leds(void)
{
	uint8_t channel;

	for (channel=0;channel<NUM_CHAN;channel++)
	{

	}
}


void init_LED_PWM_IRQ(void)
{
	TIM_TimeBaseInitTypeDef tim;
	NVIC_InitTypeDef nvic;

	RCC_APB1PeriphClockCmd(LED_TIM_RCC, ENABLE);

	nvic.NVIC_IRQChannel = LED_TIM_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 0;
	nvic.NVIC_IRQChannelSubPriority = 2;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
//	tim.TIM_Period = 17500; //168MHz / 2 / 17500 = 4.8kHz (208.3us) ... / 32 =
	tim.TIM_Period = 4375; //168MHz / 2 / 4375 = 19.2kHz
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(LED_TIM, &tim);
	TIM_ITConfig(LED_TIM, TIM_IT_Update, ENABLE);
	TIM_Cmd(LED_TIM, ENABLE);
}



//runs @ 208uS (4.8kHz), with 32 steps => 6.6ms PWM period = 150Hz
void LED_PWM_IRQHandler(void)
{
	static uint32_t loop_led_PWM_ctr=0;

	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
		 //begin1: 300ns - 450ns


		if (play_led_state[0] && (loop_led_PWM_ctr<global_param[LED_BRIGHTNESS]))
			PLAYLED1_ON;
		else
			PLAYLED1_OFF;

		if (play_led_state[1] && (loop_led_PWM_ctr<global_param[LED_BRIGHTNESS]))
			PLAYLED2_ON;
		else
			PLAYLED2_OFF;

		if (clip_led_state[0] && (loop_led_PWM_ctr<global_param[LED_BRIGHTNESS]))
			CLIPLED1_ON;
		else
			CLIPLED1_OFF;

		if (clip_led_state[1] && (loop_led_PWM_ctr<global_param[LED_BRIGHTNESS]))
			CLIPLED2_ON;
		else
			CLIPLED2_OFF;

		if (loop_led_PWM_ctr++>32)
			loop_led_PWM_ctr=0;

		//end1: 300ns - 450ns

		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

	}

}



void chase_all_lights(uint32_t delaytime)
{

		PLAYLED1_ON;
		delay_ms(delaytime);
		PLAYLED1_OFF;
		delay_ms(delaytime);

		CLIPLED1_ON;
		delay_ms(delaytime);
		CLIPLED1_OFF;
		delay_ms(delaytime);

		CLIPLED2_ON;
		delay_ms(delaytime);
		CLIPLED2_OFF;
		delay_ms(delaytime);

		PLAYLED2_ON;
		delay_ms(delaytime);
		PLAYLED2_OFF;
		delay_ms(delaytime);

		PLAYLED2_ON;
		delay_ms(delaytime);
		PLAYLED2_OFF;
		delay_ms(delaytime);

		CLIPLED2_ON;
		delay_ms(delaytime);
		CLIPLED2_OFF;
		delay_ms(delaytime);

		CLIPLED1_ON;
		delay_ms(delaytime);
		CLIPLED1_OFF;
		delay_ms(delaytime);

		PLAYLED1_ON;
		delay_ms(delaytime);
		PLAYLED1_OFF;
		delay_ms(delaytime);


		//infinite loop to force user to reset

}


void blink_all_lights(uint32_t delaytime)
{

		PLAYLED1_ON;
		PLAYLED2_ON;
		CLIPLED1_ON;
		CLIPLED2_ON;
		delay_ms(delaytime);

		PLAYLED1_OFF;
		PLAYLED2_OFF;
		CLIPLED1_OFF;
		CLIPLED2_OFF;
		delay_ms(delaytime);

}


