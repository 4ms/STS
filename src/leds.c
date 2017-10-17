/*
 * leds.c
 */

#include "dig_pins.h"
#include "globals.h"
#include "leds.h"
#include "timekeeper.h"
#include "params.h"
#include "calibration.h"


extern SystemCalibrations *system_calibrations;

extern uint32_t end_out_ctr[NUM_PLAY_CHAN];
extern uint32_t play_led_flicker_ctr[NUM_PLAY_CHAN];

uint8_t play_led_state[NUM_PLAY_CHAN]={0,0};
//uint8_t clip_led_state[NUM_PLAY_CHAN]={0,0};

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint8_t flags[NUM_FLAGS];
extern volatile uint32_t sys_tmr;


//runs @ 208uS (4.8kHz), with 32 steps => 6.6ms PWM period = 150Hz
void LED_PWM_IRQHandler(void)
{
	static uint32_t loop_led_PWM_ctr=0;
	uint32_t tm_11 = sys_tmr & 0x07FF; //11-bit counter
	uint32_t tm_12 = sys_tmr & 0x0FFF; //12-bit counter
	uint32_t tm_13 = sys_tmr & 0x1FFF; //13-bit counter
	uint32_t tm_14 = sys_tmr & 0x3FFF; //14-bit counter

	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{

		if (global_mode[CALIBRATE]==0 && global_mode[SYSTEM_MODE]==0)
		{

			if ((play_led_state[1] && global_mode[STEREO_MODE] || play_led_state[0]) && (loop_led_PWM_ctr<system_calibrations->led_brightness) && !play_led_flicker_ctr[0])
				PLAYLED1_ON;
			else
				PLAYLED1_OFF;

			if ((play_led_state[0] && global_mode[STEREO_MODE] || play_led_state[1]) && (loop_led_PWM_ctr<system_calibrations->led_brightness) && !play_led_flicker_ctr[1])
				PLAYLED2_ON;
			else
				PLAYLED2_OFF;

			if (global_mode[MONITOR_RECORDING] == MONITOR_BOTH) { //both channels monitoring: monitor LED always on (subject to led_brightness)
				if (loop_led_PWM_ctr<system_calibrations->led_brightness)					SIGNALLED_ON;
				else																		SIGNALLED_OFF;
			}
			else if (global_mode[MONITOR_RECORDING] == MONITOR_RIGHT) { //R channel monitoring: monitor LED flicker quickly
				if (tm_13<0x0400 && loop_led_PWM_ctr<system_calibrations->led_brightness)	SIGNALLED_ON;
				else																		SIGNALLED_OFF;
			}
			else if (global_mode[MONITOR_RECORDING] == MONITOR_LEFT) { //L channel monitoring: monitor LED flicker dimly
				if (tm_13<0x0400 && loop_led_PWM_ctr<system_calibrations->led_brightness)	SIGNALLED_ON;
				else																		SIGNALLED_OFF;
			}
			else if (global_mode[MONITOR_RECORDING] == MONITOR_OFF) { //no channels monitoring: monitor LED always off
																							SIGNALLED_OFF;
			}

			//Note: Busy LED is controlled directly in the SD Card driver code. 
		}

		loop_led_PWM_ctr++;
		loop_led_PWM_ctr &= 0xF;

		if (play_led_flicker_ctr[0]) play_led_flicker_ctr[0]--;
		if (play_led_flicker_ctr[1]) play_led_flicker_ctr[1]--;

		if (end_out_ctr[0]) {ENDOUT1_ON;end_out_ctr[0]--;}
		else ENDOUT1_OFF;

		if (end_out_ctr[1]) {ENDOUT2_ON;end_out_ctr[1]--;}
		else ENDOUT2_OFF;


		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

	}

}

void flicker_endout(uint8_t chan, float play_time)
{
	play_led_state[chan] = 0;
	end_out_ctr[chan] = (play_time>0.300)? 35 : ((play_time * 90) + 8);
	play_led_flicker_ctr[chan]=(play_time>0.300)? 35 : ((play_time * 36)+1);
	if (chan) ENDOUT2_ON; else ENDOUT1_ON;
}


void chase_all_lights(uint32_t delaytime)
{
	PLAYLED1_ON;
	delay_ms(delaytime);
	PLAYLED1_OFF;
	delay_ms(delaytime);

	SIGNALLED_ON;
	delay_ms(delaytime);
	SIGNALLED_OFF;
	delay_ms(delaytime);

	BUSYLED_ON;
	delay_ms(delaytime);
	BUSYLED_OFF;
	delay_ms(delaytime);

	PLAYLED2_ON;
	delay_ms(delaytime);
	PLAYLED2_OFF;
	delay_ms(delaytime);

	BUSYLED_ON;
	delay_ms(delaytime);
	BUSYLED_OFF;
	delay_ms(delaytime);

	SIGNALLED_ON;
	delay_ms(delaytime);
	SIGNALLED_OFF;
	delay_ms(delaytime);
}


void blink_all_lights(uint32_t delaytime)
{

		PLAYLED1_ON;
		PLAYLED2_ON;
		SIGNALLED_ON;
		BUSYLED_ON;
		delay_ms(delaytime);

		PLAYLED1_OFF;
		PLAYLED2_OFF;
		SIGNALLED_OFF;
		BUSYLED_OFF;
		delay_ms(delaytime);

}


