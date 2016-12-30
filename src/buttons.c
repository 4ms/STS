#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "buttons.h"

enum ButtonStates button_state[NUM_BUTTONS];
extern uint8_t flags[NUM_FLAGS];
extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];



void Button_Debounce_IRQHandler(void)
{
	static uint16_t State[NUM_BUTTONS] = {0,0,0,0,0,0,0,0}; // Current debounce status
	uint16_t t;
	uint8_t i;
	uint32_t but_read;


	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {

		for (i=0;i<NUM_BUTTONS;i++)
		{
			switch (i)
			{
				case Play1:
					but_read=PLAY1BUT;
					break;
				case Play2:
					but_read=PLAY2BUT;
					break;
				case Bank1:
					but_read=BANK1BUT;
					break;
				case Bank2:
					but_read=BANK2BUT;
					break;
				case Rec:
					but_read=RECBUT;
					break;
				case RecBank:
					but_read=BANKRECBUT;
					break;
				case Rev1:
					but_read=REV1BUT;
					break;
				case Rev2:
					but_read=REV2BUT;
					break;
			}

			if (but_read) t=0xe000;
			else t=0xe001;

			State[i]=(State[i]<<1) | t;
			if (State[i]==0xff00) //1111 1111 0000 0000 = not pressed for 8 cycles , then pressed for 8 cycles
			{
				button_state[i] = DOWN;

				switch (i)
				{
					case Play1:
						flags[Play1Trig]=1;
						break;

					case Play2:
						flags[Play2Trig]=1;
						break;

					case Bank1:
						flags[PlayBank1Changed] = 1;
						if (i_param[0][BANK] >= (NUM_BANKS-1))	i_param[0][BANK]=0;
						else									i_param[0][BANK]++;
						break;

					case Bank2:
						flags[PlayBank2Changed] = 1;
						if (i_param[1][BANK] >= (NUM_BANKS-1))	i_param[1][BANK]=0;
						else									i_param[1][BANK]++;
						break;

					case Rec:
						flags[RecTrig]=1;
						break;
					case RecBank:
						break;
					case Rev1:
						flags[Rev1Trig]=1;
						break;
					case Rev2:
						flags[Rev2Trig]=1;
						break;
				}

			}
			else if (State[i]==0xe0ff)
			{
				button_state[i] = UP;
			}
		}

		// Clear TIM update interrupt
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

	}
}

