#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "buttons.h"
#include "sts_filesystem.h"
#include "edit_mode.h"
#include "calibration.h"
#include "bank.h"
#include "button_knob_combo.h"
#include "adc.h"

extern ButtonKnobCombo g_button_knob_combo[NUM_BUTTON_KNOB_COMBO_BUTTONS][NUM_BUTTON_KNOB_COMBO_KNOBS];


enum ButtonStates button_state[NUM_BUTTONS];
extern uint8_t flags[NUM_FLAGS];
extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern int16_t old_i_smoothed_potadc[NUM_POT_ADCS];
extern int16_t old_i_smoothed_cvadc[NUM_CV_ADCS];

extern enum g_Errors g_error;
extern uint8_t	global_mode[NUM_GLOBAL_MODES];

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
	uint8_t knob;
	uint8_t combo_was_active;
	ButtonKnobCombo *t_bkc;

	uint8_t chan;
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
			//Load each button's state
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

			//
			// DOWN state (debounced)
			//
			if (State[i]==0xff00) //1111 1111 0000 0000 = not pressed for 8 cycles , then pressed for 8 cycles
			{
				button_state[i] = DOWN;
				if (!flags[skip_process_buttons])
				{
					switch (i)
					{
						case Play1:
						case Play2:
							if (global_mode[EDIT_MODE])
							{
								if (i==Play1) save_exit_assignment_mode();
								if (i==Play2)
								{
									copy_sample(i_param[1][BANK], i_param[1][SAMPLE], i_param[0][BANK], i_param[0][SAMPLE]);
									flags[ForceFileReload2] = 1;
								}

							}
							else {
								if (!i_param[i-Play1][LOOPING]) 
									flags[Play1But + (i-Play1)]=1;
								clear_errors();
							}
						break;

						case Bank1:
						case Bank2:
							chan = i - Bank1;

							//Store the pot values.
							//We use this to determine if the pot has moved while the Bank button is down
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1].latched_value = old_i_smoothed_potadc[SAMPLE_POT*2];
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample2].latched_value = old_i_smoothed_potadc[SAMPLE_POT*2 + 1];

							//Reset the combo_state.
							//We have to detect the knob as moving to make the combo ACTIVE
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1].combo_state = COMBO_INACTIVE;
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample2].combo_state = COMBO_INACTIVE;
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
							enter_edit_mode();
							break;

					}
				}
			}

			//
			// UP state (debounced)
			//
			else if (State[i]==0xffff)
			{
				if (!flags[skip_process_buttons])
				{
					if (button_state[i] != UP)
					{
						switch (i)
						{
							case Bank1:
							case Bank2:
								chan = i - Bank1;	

								combo_was_active = 0;
								for (knob=0;knob<2;knob++)
								{
									t_bkc = &g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1 + knob];

									if (t_bkc->combo_state == COMBO_ACTIVE)
									{
										//Latch the pot+CV value
										//This allows us to keep using this value until either the pot or CV changes
										t_bkc->latched_value = old_i_smoothed_potadc[SAMPLE_POT*2 + knob] + old_i_smoothed_cvadc[SAMPLE_CV*2 + knob];

										//Change our combo state to latched
										t_bkc->combo_state = COMBO_LATCHED;

										//Set the bank parameter to the hover value
										i_param[chan][BANK] = t_bkc->hover_value;

										combo_was_active = 1;
									}
								}

								//Go to next bank if we weren't do a knob/button combo move
								//Edit button down: go to next bank, whether or not it's enabled
								//No edit button: go to next enabled bank
								if (!combo_was_active)
								{
									if (global_mode[EDIT_MODE])
										i_param[chan][BANK] = next_bank(i_param[chan][BANK]);
									else
										i_param[chan][BANK] = next_enabled_bank(i_param[chan][BANK]);
								}

								flags[PlayBank1Changed + chan] = 1;

								//Exit assignment mode (if we were in it)
								if (chan==0)
									global_mode[ASSIGN_MODE] = 0;
								
							break;

							case Rec:
								if (button_state[Rec]<SHORT_PRESSED)
									flags[RecTrig]=1;
							break;

							case RecBank:
								flags[RecBankChanged] = 1;
								if (i_param[2][BANK] >= (MAX_NUM_BANKS-1))	i_param[2][BANK]=0;
								else										i_param[2][BANK]++;
							break;

							case Play1:
							case Play2:
								if (!global_mode[EDIT_MODE])
								{
									if (i_param[i - Play1][LOOPING] && button_state[i - Play1] == DOWN) 
										flags[Play1But + i] = 1; //if looping, stop playing when lifted (assuming it's a short press)
								}
							break;

							case Rev1:
								if (global_mode[EDIT_MODE])
								{
									if (enter_assignment_mode())
										if (next_unassigned_sample())
										{
											flags[ForceFileReload1] = 1;
											flags[Play1But]=1;
										}
								}
								else if (!flags[AssignModeRefused])
									flags[Rev1Trig]=1;
				

								clear_errors();
								break;

							case Rev2:
								if (global_mode[EDIT_MODE])
								{
									restore_undo_state(i_param[0][BANK], i_param[0][SAMPLE]);
								}
								else
									flags[Rev2Trig]=1;

								clear_errors();
								break;

							case Edit:
								exit_edit_mode();
								break;

							default:
								break;
						}
					}
				}
				button_state[i] = UP;
			}


			//
			// Short/Medium/Long presses
			//

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

							if (!flags[skip_process_buttons])
							{
								switch (i)
								{
									case Rev2:
										if (global_mode[EDIT_MODE])
										{
											//
											//Long press with both bank buttons (Edit+Bank1+Bank2+Rev2):
											//Reload all banks from the backup index file
											//Restores the entire sampler to the state it was on boot 
											//
											t = global_mode[STEREO_MODE];
									//		if (button_state[Bank1]>=MED_PRESSED && button_state[Bank2]>=MED_PRESSED)
									//			load_sampleindex_file(BACKUPFILE, ALL_BANKS);
											global_mode[STEREO_MODE] = t;
										}
										break;

									default:
										break;
								}
							}
						}
					}
					else if (long_press[i] > MED_PRESSED) {
						if (button_state[i]!=MED_PRESSED)
						{
							button_state[i] = MED_PRESSED;
							if (!flags[skip_process_buttons])
							{
								switch (i)
								{
									case Rev2:
										if (global_mode[EDIT_MODE])
										{
											//
											//Medium press with Bank button: reload bank from the backup index file
											//Restores just this bank to the state it was on boot 
											//
											t = global_mode[STEREO_MODE];

									//		if (button_state[Bank1]>=SHORT_PRESSED)
									//			load_sampleindex_file(BACKUPFILE, i_param[0][BANK]);
									//
									//		if (button_state[Bank2]>=SHORT_PRESSED)
									//			load_sampleindex_file(BACKUPFILE, i_param[1][BANK]);

											global_mode[STEREO_MODE] = t;
											flags[skip_process_buttons] = 1;
										}
										break;

									default:
										break;
								}
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

		if (!flags[skip_process_buttons])
		{
		//
		// Multi-button presses
		//

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

