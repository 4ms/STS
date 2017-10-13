/*
 * dig_jacks.c
 *
 *  Created on: Dec 19, 2016
 *      Author: design
 */


#include "globals.h"
#include "dig_pins.h"
#include "trigger_jacks.h"
#include "params.h"
#include "adc.h"
#include "sampler.h"

enum TriggerStates 			jack_state[NUM_TRIG_JACKS];
extern uint8_t 				flags[NUM_FLAGS];
extern uint32_t 			play_trig_timestamp[2];

extern uint32_t				voct_latch_value[2];
extern int16_t				bracketed_cvadc[NUM_CV_ADCS];
enum PlayStates 			play_state[NUM_PLAY_CHAN];

volatile uint32_t 			sys_tmr;


void Trigger_Jack_Debounce_IRQHandler(void)
{
	static uint16_t State[NUM_TRIG_JACKS] = {0,0,0,0,0}; // Current debounce status
	uint16_t t;
	uint8_t i;
	uint32_t jack_read;


	if (TIM_GetITStatus(TrigJack_TIM, TIM_IT_Update) != RESET) {
		

		for (i=0;i<NUM_TRIG_JACKS;i++)
		{
			switch (i)
			{
				case TrigJack_Play1:
					jack_read=PLAY1JACK;
					break;
				case TrigJack_Play2:
					jack_read=PLAY2JACK;
					break;
				case TrigJack_Rec:
					jack_read=RECTRIGJACK;
					break;
				case TrigJack_Rev1:
					jack_read=REV1JACK;
					break;
				case TrigJack_Rev2:
					jack_read=REV2JACK;
					break;
			}

			if (jack_read)		t=0xe000;
			else				t=0xe001;

			State[i]=(State[i]<<1) | t;
			if (State[i]==0xfffe) //1111 1111 1111 1110 = low for 15 cycles, then high for 1 cycle
			{
				jack_state[i] = TrigJack_DOWN;

				switch (i)
				{
					case TrigJack_Play1: //we detect a trigger within 0.338ms after voltage appears on the jack
						if (play_state[0] == PLAYING) 
							play_state[0] = PLAY_FADEDOWN;

						voct_latch_value[0]			= bracketed_cvadc[0];
						flags[Play1TrigDelaying]	= 1;
						play_trig_timestamp[0]		= sys_tmr;
						break;
					
					case TrigJack_Play2:
//						DEBUG3_ON;
						if (play_state[1] == PLAYING) 
							play_state[1] = PLAY_FADEDOWN;

						voct_latch_value[1]			= bracketed_cvadc[1];
						flags[Play2TrigDelaying]	= 1;
						play_trig_timestamp[1]		= sys_tmr;
						break;

					case TrigJack_Rec:
						flags[RecTrig]=1;
						break;
					case TrigJack_Rev1:
						flags[Rev1Trig]=1;
						break;
					case TrigJack_Rev2:
						flags[Rev2Trig]=1;
						break;
				}

			}
			else if (State[i]==0xe0ff)
			{
				jack_state[i] = TrigJack_UP;
			}
		}

		process_cv_adc(); 

		// Clear TIM update interrupt
		TIM_ClearITPendingBit(TrigJack_TIM, TIM_IT_Update);
	}
}

