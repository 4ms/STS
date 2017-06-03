#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "buttons.h"
#include "sts_filesystem.h"
#include "edit_mode.h"
#include "calibration.h"


enum ButtonStates button_state[NUM_BUTTONS];
extern uint8_t flags[NUM_FLAGS];
extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern enum g_Errors g_error;
extern uint8_t	global_mode[NUM_GLOBAL_MODES];
extern SystemCalibrations *system_calibrations;

extern volatile uint32_t sys_tmr;

void clear_errors(void)
{
	g_error = 0;
	SIGNALLED_OFF;
	BUSYLED_OFF;

}
void init_buttons(void)
{
	uint8_t i;

	for (i=0;i<NUM_BUTTONS;i++)
		button_state[i] = UP;

}


void Button_Debounce_IRQHandler(void)
{
	static uint16_t State[NUM_BUTTONS] = {0xffff}; // Current debounce status

	uint16_t t;
	uint8_t i;
	uint32_t but_read;
	static uint32_t long_press[NUM_BUTTONS] = {0};
	static uint32_t last_sys_tmr=0;
	uint32_t elapsed_time;


	if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {

		if (sys_tmr < last_sys_tmr) //then sys_tmr wrapped from 0xFFFFFFFF to 0x00000000
			elapsed_time = (0xFFFFFFFF - last_sys_tmr) + sys_tmr;
		else
			elapsed_time = (sys_tmr - last_sys_tmr);
		last_sys_tmr = sys_tmr;

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
				if (!flags[skip_process_buttons])
				{
					switch (i)
					{
						case Play1:
							if (global_mode[EDIT_MODE])
								save_exit_assignment_mode();
							else {
								if (!i_param[0][LOOPING]) 
									flags[Play1But]=1;
								clear_errors();
							}
							break;

						case Play2:
							if (global_mode[EDIT_MODE])
								assign_sample_from_other_bank(i_param[1][BANK], i_param[1][SAMPLE]);
							else 
							{
								if (!i_param[1][LOOPING]) 
									flags[Play2But]=1;
								clear_errors();
							}
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
			}

			//Check for debounced up-state
			//else if ((State[i] & 0xefff)==0xe0ff)
			else if (State[i]==0xffff)
			{
				if (button_state[i] != UP)
				{
					if (!flags[skip_process_buttons])
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
								if (!global_mode[EDIT_MODE])
								{
									if (i_param[0][LOOPING] && button_state[Play1] == DOWN) 
										flags[Play1But] = 1; //if looping, stop playing when lifted (assuming it's a short press)
								}
								break;

							case Play2:
								if (!global_mode[EDIT_MODE])
								{
									if (i_param[1][LOOPING] && button_state[Play2] == DOWN) 
										flags[Play2But] = 1;
								}
								break;

							case Rev1:
								if (global_mode[EDIT_MODE])
								{
									enter_assignment_mode();
									next_unassigned_sample();
								}
								else if (!flags[AssignModeRefused])
									flags[Rev1Trig]=1;
				

								clear_errors();
								break;

							case Rev2:
								if (global_mode[EDIT_MODE]){
									t = global_mode[STEREO_MODE];
									load_sampleindex_file();
									global_mode[STEREO_MODE] = t;
								}else
									flags[Rev2Trig]=1;

								clear_errors();
								break;

							case Edit:
								global_mode[EDIT_MODE] = 0;
								flags[Play1But] = 1;
								break;

							default:
								break;
						}
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
						long_press[i]+=elapsed_time;


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
							if (!flags[skip_process_buttons])
							{
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

			}
			else
			{
				long_press[i]=0;
				//button_state[i] = UNKNOWN; //If we are just getting noise that makes State[i]==0x0001, we need to set button_state[i] to something other than LONG/MED/SHORT_PRESSED
			}
		}

		//Check for multi-press holds
		if (!flags[skip_process_buttons])
		{

			if (button_state[Bank1] >= SHORT_PRESSED && button_state[Bank2] >= SHORT_PRESSED)
			{
				if (global_mode[STEREO_MODE] == 1)
				{
					global_mode[STEREO_MODE] = 0;
					flags[StereoModeTurningOff] = 1;
				} else {
					global_mode[STEREO_MODE] = 1;
					flags[StereoModeTurningOn] = 1;
				}

				long_press[Bank1] = 0xFFFFFFFF;
				long_press[Bank2] = 0xFFFFFFFF;
				button_state[Bank1] = UP;
				button_state[Bank2] = UP;
			}

		}

		// Clear TIM update interrupt
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

	}

	if (flags[skip_process_buttons]==2) //ready to clear flag, but wait until all buttons are up
	{
		for (i=0;i<NUM_BUTTONS;i++)
		{
			if (button_state[i] != UP) break;
		}
		if (i==NUM_BUTTONS) flags[skip_process_buttons] = 0;
	}

}

