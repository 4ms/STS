/*
 * leds.c
 */

#include "dig_pins.h"
#include "globals.h"
#include "leds.h"
#include "timekeeper.h"
#include "params.h"
#include "flash_user.h"


extern SystemSettings *user_params;

extern uint32_t end_out_ctr[NUM_PLAY_CHAN];
extern uint32_t play_led_flicker_ctr[NUM_PLAY_CHAN];

uint8_t play_led_state[NUM_PLAY_CHAN]={0,0};
uint8_t clip_led_state[NUM_PLAY_CHAN]={0,0};

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint8_t flags[NUM_FLAGS];
extern volatile uint32_t sys_tmr;





void update_channel_leds(void)
{
	uint8_t channel;

	for (channel=0;channel<NUM_PLAY_CHAN;channel++)
	{

	}
}



//runs @ 208uS (4.8kHz), with 32 steps => 6.6ms PWM period = 150Hz
void LED_PWM_IRQHandler(void)
{
	static uint32_t loop_led_PWM_ctr=0;
	uint32_t tm_13 = sys_tmr & 0x1FFF; //13-bit counter
	uint32_t tm_14 = sys_tmr & 0x3FFF; //14-bit counter

	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
		 //begin1: 300ns - 450ns

		if (flags[StereoModeTurningOn])
		{
			if (tm_14 < 0x2000) {
				PLAYLED1_ON;
				PLAYLED2_ON;
			} else {
				PLAYLED1_OFF;
				PLAYLED2_OFF;
			}
		}
		else if (flags[StereoModeTurningOff])
		{
			if (tm_13 < 0x1000)	PLAYLED1_ON;
			else PLAYLED1_OFF;
		
			if (tm_14 < 0x2000)	PLAYLED2_ON;
			else PLAYLED2_OFF;
	
		}
		else
		{
			if ((play_led_state[1] && global_mode[STEREO_LINK] || play_led_state[0]) && (loop_led_PWM_ctr<user_params->led_brightness) && !play_led_flicker_ctr[0])
				PLAYLED1_ON;
			else
				PLAYLED1_OFF;

			if ((play_led_state[0] && global_mode[STEREO_LINK] || play_led_state[1]) && (loop_led_PWM_ctr<user_params->led_brightness) && !play_led_flicker_ctr[1])
				PLAYLED2_ON;
			else
				PLAYLED2_OFF;
	//
	//		if (clip_led_state[0] && (loop_led_PWM_ctr<user_params->led_brightness))
	//			CLIPLED1_ON;
	//		else
	//			CLIPLED1_OFF;
	//
	//		if (clip_led_state[1] && (loop_led_PWM_ctr<user_params->led_brightness))
	//			CLIPLED2_ON;
	//		else
	//			CLIPLED2_OFF;
		}

		loop_led_PWM_ctr++;
		loop_led_PWM_ctr &= 0xF;

		//end1: 300ns - 450ns

		if (play_led_flicker_ctr[0]) play_led_flicker_ctr[0]--;
		if (play_led_flicker_ctr[1]) play_led_flicker_ctr[1]--;

		if (end_out_ctr[0]) {ENDOUT1_ON;end_out_ctr[0]--;}
		else ENDOUT1_OFF;

		if (end_out_ctr[1]) {ENDOUT2_ON;end_out_ctr[1]--;}
		else ENDOUT2_OFF;

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


