#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "buttons.h"

enum ButtonStates button_state[NUM_BUTTONS];
extern uint8_t flags[NUM_FLAGS];
extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern enum g_Errors g_error;

void clear_errors(void)
{
	g_error = 0;
	CLIPLED1_OFF;
	CLIPLED2_OFF;

}

void Button_Debounce_IRQHandler(void)
{
	static uint16_t State[NUM_BUTTONS] = {0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff}; // Current debounce status
	uint16_t t;
	uint8_t i;
	uint32_t but_read;
	static uint32_t long_press[NUM_BUTTONS] = {0,0,0,0,0,0,0,0};


	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {

		for (i=0;i<NUM_BUTTONS;i++)
		{
			//Load the button's state
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

			//check for de-bounced down state
			if (State[i]==0xff00) //1111 1111 0000 0000 = not pressed for 8 cycles , then pressed for 8 cycles
			{
				button_state[i] = DOWN;

				switch (i)
				{
					case Play1:
						flags[Play1Trig]=1;
						clear_errors();
						break;

					case Play2:
						flags[Play2Trig]=1;
						clear_errors();
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
						break;
					case RecBank:
						flags[RecBankChanged] = 1;
						if (i_param[2][BANK] >= (NUM_BANKS-1))	i_param[2][BANK]=0;
						else									i_param[2][BANK]++;
						break;
					case Rev1:
						flags[Rev1Trig]=1;
						clear_errors();
						break;
					case Rev2:
						flags[Rev2Trig]=1;
						clear_errors();
						break;
				}

			}

			//Check for debounced up-state
			else if (State[i]==0xe0ff)
			{

				switch (i)
				{
					case Rec:
						if (button_state[Rec]<SHORT_PRESSED)
							flags[RecTrig]=1;
						break;

					default:
						break;
				}

				button_state[i] = UP;


			}

			//Check for long presses

			if (State[i]==0xe000) //pressed for 16 cycles
			{
				if (long_press[i] != 0xFFFFFFFF) //prevent roll-over if holding down for a REALLY long time
					long_press[i]++;


				if (long_press[i] > LONG_PRESSED) {

					if (button_state[i]!=LONG_PRESSED)
					{
						button_state[i] = LONG_PRESSED;
						switch (i)
						{
							default:
								break;
						}
					}
				}
				else if (long_press[i] > MED_PRESSED) {
					if (button_state[i]!=MED_PRESSED)
					{
						button_state[i] = MED_PRESSED;
						switch (i)
						{
							default:
								break;
						}
					}
				}
				else if (long_press[i] > SHORT_PRESSED) {
					if (button_state[i]!=SHORT_PRESSED)
					{
						button_state[i] = SHORT_PRESSED;
						switch (i)
						{
							case Rec:
								flags[ToggleMonitor] = 1;
								break;

							case Play1:
								flags[ToggleLooping1] = 1;
								break;

							case Play2:
								flags[ToggleLooping2] = 1;
								break;

							default:
								break;
						}
					}
				}

			}
			else
			{
				long_press[i]=0;
				//button_state[i] = UNKNOWN; //If we are just getting noise that makes State[i]==0x0001, we need to set button_state[i] to something other than LONG/MED/SHORT_PRESSED
			}


		}

		// Clear TIM update interrupt
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

	}
}

