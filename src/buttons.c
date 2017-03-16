#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "buttons.h"
#include "sts_filesystem.h"
#include "edit_mode.h"


enum ButtonStates button_state[NUM_BUTTONS];
extern uint8_t flags[NUM_FLAGS];
extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern enum g_Errors g_error;
extern uint8_t	global_mode[NUM_GLOBAL_MODES];

void clear_errors(void)
{
	g_error = 0;
	CLIPLED1_OFF;
	CLIPLED2_OFF;

}
void init_buttons(void)
{
	uint8_t i;

	for (i=0;i<NUM_BUTTONS;i++)
		button_state[i] = UP;

}


void Button_Debounce_IRQHandler(void)
{
//	static uint16_t State[NUM_BUTTONS] = {0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff}; // Current debounce status
//	static uint16_t State[NUM_BUTTONS] = {[0 ... NUM_BUTTONS-1] = 0xffff}; // Current debounce status
	static uint16_t State[NUM_BUTTONS] = {0xffff}; // Current debounce status
	uint16_t t;
	uint8_t i;
	uint32_t but_read;
	static uint32_t long_press[NUM_BUTTONS] = {0};


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
				case Edit:
					but_read=EDIT_BUTTON;
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
						if (!i_param[0][LOOPING]) 
							flags[Play1Trig]=1;
						clear_errors();
						break;

					case Play2:
						if (!i_param[1][LOOPING]) 
							flags[Play2Trig]=1;
						clear_errors();
						break;

					case Bank1:
						break;
					case Bank2:
						break;
					case Rec:
						break;
					case RecBank:
						break;

					case Rev1:
						break;

					case Rev2:
						break;

					case Edit:
						global_mode[EDIT_MODE] = 1;
						break;

				}

			}

			//Check for debounced up-state
			else if (State[i]==0xe0ff)
			{

				if (button_state[i] != UP)
				{
					switch (i)
					{

						case Bank1:
							flags[PlayBank1Changed] = 1;
							i_param[0][BANK] = next_enabled_bank(i_param[0][BANK]);

							//load assignment samples for new bank
							//if (global_mode[EDIT_MODE]) enter_assignment_mode();
							break;

						case Bank2:
							flags[PlayBank2Changed] = 1;
							i_param[1][BANK] = next_enabled_bank(i_param[1][BANK]);
							break;

						case Rec:
							if (button_state[Rec]<SHORT_PRESSED)
								flags[RecTrig]=1;
							break;

						case RecBank:
							flags[RecBankChanged] = 1;
							if (i_param[2][BANK] >= (MAX_NUM_REC_BANKS-1))	i_param[2][BANK]=0;
							else											i_param[2][BANK]++;
							break;

						case Play1:
							if (i_param[0][LOOPING] && button_state[Play1] == DOWN) 
								flags[Play1Trig] = 1; //if looping, stop playing when lifted (assuming it's a short press)
							break;

						case Play2:
							if (i_param[1][LOOPING] && button_state[Play1] == DOWN) 
								flags[Play2Trig] = 1;
							break;

						case Rev1:
							//if (!global_mode[EDIT_MODE] && !flags[AssignModeRefused])
								flags[Rev1Trig]=1;
							//else
							//	next_unassigned_sample();


							clear_errors();
							break;

						case Rev2:
							flags[Rev2Trig]=1;

							clear_errors();
							break;

						case Edit:
							global_mode[EDIT_MODE] = 0;
							break;

						default:
							break;
					}
				}
				button_state[i] = UP;
			}


			//Check for long presses

			if (State[i]==0xe000) //pressed for 16 cycles
			{
				if (long_press[i] != 0xFFFFFFFF) //already detected, ignore
				{
					if (long_press[i] != 0xFFFFFFFE) //prevent roll-over if holding down for a REALLY long time
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
									//enable_recording=1;
									break;

								case Play1:
									if (button_state[Rev1] == UP)
										flags[ToggleLooping1] = 1;
									break;

								case Play2:
									if (button_state[Rev1] == UP)
										flags[ToggleLooping2] = 1;
									break;

								case Bank1:
									break;

								default:
									break;
							}
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

		//Sample Assignment mode
		// if (button_state[Play1] >= SHORT_PRESSED && button_state[Rev1] >= SHORT_PRESSED)
		// {
		// 	if (global_mode[EDIT_MODE])
		// 		save_exit_assignment_mode();
		// 	else
		// 		enter_assignment_mode();

		// 	long_press[Play1] = 0xFFFFFFFF;
		// 	long_press[Rev1] = 0xFFFFFFFF;
		// 	button_state[Play1] = UP;
		// 	button_state[Rev1] = UP;

		// }
		//ToDo:
		//only allow assign mode on ch2
		//we will use ch1 to move samples from different banks
		//on entering ASSIGN mode: change ch1 bank to ch2 bank
		//during ASSIGN mode:
		//turn off Play1 and Rev1
		//Bank1 changes the source bank (loads t_assign_samples from directory selected by ch1)
		//Bank2 changes the destination bank
		//Rev2 changes the sample within the selected source directoy
		//??
		//Or we could scan the sdcard for samples, everytime you press Rev2 is looks for the next sample
		//MEDIUM PRESS on Rev2 makes it blank (turns red/flashing)
		//Pressing Rev1 leaves the directory and tries another dir in the tree (dim slow flashing orange)
		//Play1 is off, no effect
		//Bank1 is off, no effect
		//

		// if (button_state[Play2] >= SHORT_PRESSED && button_state[Rev2] >= SHORT_PRESSED)
		// {
		// 	cancel_exit_assignment_mode();
		// }


		// Clear TIM update interrupt
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

	}
}

