/*
 * dig_jacks.c
 *
 *  Created on: Dec 19, 2016
 *      Author: design
 */


#include "globals.h"
#include "dig_pins.h"
#include "trigger_jacks.h"

enum TriggerStates 			jack_state[NUM_TRIG_JACKS];
extern uint8_t 				flags[NUM_FLAGS];
extern uint32_t 			play_trig_timestamp[2];

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
			if (State[i]==0xff00) //1111 1111 0000 0000 = not pressed for 8 cycles , then pressed for 8 cycles
			{
				jack_state[i] = TrigJack_DOWN;

				switch (i)
				{
					case TrigJack_Play1:
						flags[Play1Trig]=1;
						play_trig_timestamp[0]=sys_tmr;
						DEBUG1_ON;
						break;
						
					case TrigJack_Play2:
						flags[Play2Trig]=1;
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

		// Clear TIM update interrupt
		TIM_ClearITPendingBit(TrigJack_TIM, TIM_IT_Update);

	}
}

