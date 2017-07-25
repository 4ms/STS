#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "buttons.h"
#include "sts_filesystem.h"
#include "sts_fs_index.h"
#include "edit_mode.h"
#include "calibration.h"
#include "bank.h"
#include "button_knob_combo.h"
#include "adc.h"
#include "system_settings.h"
#include "sampler.h"
#include "res/LED_palette.h"
#include "wav_recording.h"
#include "flash_user.h"

extern ButtonKnobCombo 		g_button_knob_combo[NUM_BUTTON_KNOB_COMBO_BUTTONS][NUM_BUTTON_KNOB_COMBO_KNOBS];


enum ButtonStates 			button_state[NUM_BUTTONS];
extern uint8_t 				flags[NUM_FLAGS];
extern uint8_t 				i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern int16_t 				bracketed_potadc[NUM_POT_ADCS];
extern int16_t 				bracketed_cvadc[NUM_CV_ADCS];

extern enum g_Errors 		g_error;
extern uint8_t				global_mode[NUM_GLOBAL_MODES];

extern volatile uint32_t 	sys_tmr;

extern uint8_t 				cur_assign_bank;
extern uint8_t 				bank_status[MAX_NUM_BANKS];
extern enum 				PlayStates play_state[NUM_PLAY_CHAN];

extern SystemCalibrations 	*system_calibrations;
extern enum RecStates	rec_state;

void clear_errors(void)
{
	g_error = 0;
	// SIGNALLED_OFF;
	// BUSYLED_OFF;

}
void init_buttons(void)
{
	uint8_t i;

	for (i=0;i<NUM_BUTTONS;i++)
		button_state[i] = UP;

	flags[SkipProcessButtons] = 2;

}


void Button_Debounce_IRQHandler(void)
{
	static uint16_t State[NUM_BUTTONS] = {0xffff}; // Current debounce status

	uint16_t t;
	uint8_t i;
	uint8_t knob;
	uint8_t combo_was_active;
	ButtonKnobCombo *t_bkc;
	uint8_t bank_blink, bank_color;

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
				if (!flags[SkipProcessButtons])
				{
					switch (i)
					{
						case Play1:
						case Play2:
							if (!global_mode[EDIT_MODE])
							{
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
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1].latched_value = bracketed_potadc[SAMPLE_POT*2];
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample2].latched_value = bracketed_potadc[SAMPLE_POT*2 + 1];

							//Reset the combo_state.
							//We have to detect the knob as moving to make the combo ACTIVE
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1].combo_state = COMBO_INACTIVE;
							g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample2].combo_state = COMBO_INACTIVE;
						break;


						case RecBank:
							if (!global_mode[EDIT_MODE])
							{
								//Store the pot values.
								//We use this to determine if the pot has moved while the Bank button is down
								g_button_knob_combo[bkc_RecBank][bkc_RecSample].latched_value = bracketed_potadc[RECSAMPLE_POT];

								//Reset the combo_state.
								//We have to detect the knob as moving to make the combo ACTIVE
								g_button_knob_combo[bkc_RecBank][bkc_RecSample].combo_state = COMBO_INACTIVE;
							}
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
				if (!flags[SkipProcessButtons])
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
									//store a short-hand version for the combo, just to make code more readable
									t_bkc = &g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1 + knob];

									if (t_bkc->combo_state == COMBO_ACTIVE)
									{
										//Latch the pot+CV value
										//This allows us to keep using this value until either the pot or CV changes
										t_bkc->latched_value = bracketed_potadc[SAMPLE_POT*2 + knob] + bracketed_cvadc[SAMPLE_CV*2 + knob];

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
									exit_assignment_mode();
								
							break;

							case Rec:
								if (global_mode[EDIT_MODE])
								{
									if (play_state[0] != SILENT) {system_calibrations->tracking_comp[0] -= 0.001;}
									if (play_state[1] != SILENT) {system_calibrations->tracking_comp[1] -= 0.001;}
								}
								else
								{
									if (button_state[Rec]<SHORT_PRESSED)
										flags[RecTrig]=1;
								}
							break;

							case RecBank:
								if (global_mode[EDIT_MODE])
								{
									if (play_state[0] != SILENT) {system_calibrations->tracking_comp[0] += 0.001;}
									if (play_state[1] != SILENT) {system_calibrations->tracking_comp[1] += 0.001;}
								}
								else 
								{
									//See if there was a combo button+knob happening

									//this a short-hand version for the combo, just to make code more readable:
									t_bkc = &g_button_knob_combo[bkc_RecBank][bkc_RecSample];
									if (t_bkc->combo_state == COMBO_ACTIVE)
									{
										//Change our combo state to inactive
										//Note: unlike Bank1 and Bank2, we do not latch Rec Sample value after RecBank is released
										t_bkc->combo_state = COMBO_INACTIVE;

										//Set the rec bank parameter to the hover value
										i_param[REC][BANK] = t_bkc->hover_value;

										combo_was_active = 1;
									}

									else
									//No combo, so just increment the Rec Bank color, but not blink
									{
										bank_blink = get_bank_blink_digit(i_param[REC][BANK]);
										bank_color = get_bank_color_digit(i_param[REC][BANK]);
										bank_color++;
										if (bank_color>=10) bank_color=0;

										i_param[REC][BANK] = bank_color + (bank_blink*10);

										//range check, but should not be neccessary
										if (i_param[REC][BANK] >= MAX_NUM_BANKS)	i_param[REC][BANK]=0;
									}

									flags[RecBankChanged] = 1;
								}
							break;

							case Play1:
							case Play2:
								if (!global_mode[EDIT_MODE])
								{
									if (i_param[i - Play1][LOOPING] && button_state[i - Play1] == DOWN) 
										flags[Play1But + i] = 1; //if looping, stop playing when lifted (assuming it's a short press)
								}
							break;

							// Edit + Next File
							case Rev1:
								if (global_mode[EDIT_MODE])
								{
									//Tell the main loop to find the next sample to assign
									if (button_state[Rev1]<SHORT_PRESSED)
										flags[FindNextSampleToAssign] = 1;
								}
								else
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
								//This is handled outside the SkipProcessButtons if block
								break;

							default:
								break;
						}
					}
				}
				if (button_state[i]!=UP && i==Edit)
					exit_edit_mode();
				button_state[i] = UP;

			}


			//
			// Short/Medium/Long presses
			//

			if (State[i]==0xe000) //pressed for 16 cycles
			{
				if (long_press[i] != 0xFFFFFFFF) //already detected, ignore
				{
					if (long_press[i] < (0xFFFFFFFF - elapsed_time)) //prevent roll-over if holding down for a REALLY long time
						long_press[i]+=elapsed_time;
					else
						long_press[i] = 0xFFFFFFFE;

					if (long_press[i] > LONG_PRESSED) {

						if (button_state[i]!=LONG_PRESSED)
						{
							button_state[i] = LONG_PRESSED;

							if (!flags[SkipProcessButtons])
							{
								switch (i)
								{
					
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
							if (!flags[SkipProcessButtons])
							{
								switch (i)
								{

									case Rev1:
										if (button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP && button_state[Rec]==UP && button_state[RecBank]==UP)
										{
											if(global_mode[EDIT_MODE])
											{
												//Go to previous assignment bank
												flags[FindNextSampleToAssign] = 2;

												//Disable the Rev1 button from doing anything until it's released
												button_state[Rev1] = UP;

												//Make the prev-bank action repeat at faster rate
												long_press[Rev1] = MED_PRESS_REPEAT;
											}
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
							if (!flags[SkipProcessButtons])
							{
								switch (i)
								{
									case Rec:
										flags[ToggleMonitor] = 1;
										//enable_recording=1;
										break;

									case Play1:
										if (button_state[Rev1]==UP && button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP && button_state[Rec]==UP && button_state[RecBank]==UP)
										{
											if (global_mode[EDIT_MODE])
											{
												check_enabled_banks();
												flags[RewriteIndex] 		= WHITE;
												flags[SkipProcessButtons] 	= 2;
												save_flash_params(0);
											}
											else
												flags[ToggleLooping1] 		= 1;
										}
										break;

									case Play2:
										if (button_state[Rev1]==UP && button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP && button_state[Rec]==UP && button_state[RecBank]==UP)
										{
											if (global_mode[EDIT_MODE])
											{
												copy_sample(i_param[1][BANK], i_param[1][SAMPLE], i_param[0][BANK], i_param[0][SAMPLE]);
												flags[ForceFileReload2] 	= 1;
												flags[SkipProcessButtons]	= 2;
											}
											else
												flags[ToggleLooping2] 		= 1;
										}
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

		if (!flags[SkipProcessButtons])
		{
		//
		// Multi-button presses
		//

			if (global_mode[EDIT_MODE])
			{
				if (button_state[Rev1] == UP && button_state[Play1] == UP && button_state[Play2] == UP && button_state[Rec] == UP && button_state[RecBank] == UP)
				{
					//
					//Medium press with Edit + Rev2 + one Bank button: reload bank from the saved index file
					//(not the backup-- this is because we want to undo changes since we booted, 
					//not since the last time we booted)
					//Restores just this bank to the state it was on boot 
					//
					if (button_state[Bank1]>=MED_PRESSED && button_state[Rev2]>=MED_PRESSED && button_state[Bank2]==UP)
					{
						flags[LoadBackupIndex] = i_param[0][BANK]+1;
						flags[RevertBank1]=200;
		
						flags[SkipProcessButtons] = 2;
						flags[UndoSampleExists] = 0;
						play_state[0] = SILENT;
						play_state[1] = SILENT;
					}
			
					if (button_state[Bank2]>=MED_PRESSED && button_state[Rev2]>=MED_PRESSED && button_state[Bank1]==UP)
					{
						flags[LoadBackupIndex] = i_param[1][BANK]+1;
						flags[RevertBank2]=200;

						flags[SkipProcessButtons] = 2;
						flags[UndoSampleExists] = 0;
						play_state[0] = SILENT;
						play_state[1] = SILENT;
					}

					//
					//Long press with Edit + Rev2 + both bank buttons
					//Reload all banks from the backup index file
					//Restores the entire sampler to the state it was on boot 
					//

					if (button_state[Bank1]>=LONG_PRESSED && button_state[Bank2]>=LONG_PRESSED && button_state[Rev2]>=LONG_PRESSED)
					{
						flags[LoadBackupIndex] = ALL_BANKS + 1;
						flags[RevertAll]=255;

						flags[SkipProcessButtons] = 2;
						flags[UndoSampleExists] = 0;
						play_state[0] = SILENT;
						play_state[1] = SILENT;
					}
				}

				if (!global_mode[SYSTEM_MODE])
				{
					// All buttons down except Bank1, Bank2, for LONG PRESS, and we're not recording, we enter System Mode
					if (button_state[Rev1]>=LONG_PRESSED && button_state[Rev2]>=LONG_PRESSED && button_state[Play1]>=LONG_PRESSED && button_state[Play2]>=LONG_PRESSED && button_state[Rec]>=LONG_PRESSED && button_state[RecBank]>=LONG_PRESSED)
					{
						if (rec_state==REC_OFF)
						{
							enter_system_mode();
							flags[SkipProcessButtons] = 1;
						}
					}
				}
				// else
				// {
				// 	// Holding down for MED PRESS: exit and save
				// 	if (button_state[Rev1]>=MED_PRESSED && button_state[Rev2]>=MED_PRESSED && button_state[Play1]>=MED_PRESSED && button_state[Play2]>=MED_PRESSED && button_state[Rec]>=MED_PRESSED && button_state[RecBank]>=MED_PRESSED)
				// 	{
				// 		exit_system_mode(1); //save
				// 		flags[SkipProcessButtons] = 2;
				// 	}

				// 	//If buttons were tapped but not held down long, then exit
				// 	//First, detect they were pressed...
				// 	if (button_state[Rev1]>=DOWN && button_state[Rev2]>=DOWN && button_state[Play1]>=DOWN && button_state[Play2]>=DOWN && button_state[Rec]>=DOWN && button_state[RecBank]>=DOWN)
				// 	{
				// 		flags[SystemModeButtonsDown] = 1;
				// 	}
				// 	else
				// 	{
				// 		//...Next, if we already detected them as pressed down, but they're not now, exit without saving
				// 		if (flags[SystemModeButtonsDown])
				// 		{
				// 			exit_system_mode(0); //don't save
				// 		}
				// 		flags[SystemModeButtonsDown] = 0;
				// 	}
				// }

			}
			else //not EDIT_MODE
			{
				//Both bank buttons --> Stereo Mode toggle
				if (button_state[Bank1] >= SHORT_PRESSED && button_state[Bank2] >= SHORT_PRESSED && button_state[RecBank] == UP)
				{
					if (global_mode[STEREO_MODE] == 1)
					{
						global_mode[STEREO_MODE] = 0;
						flags[StereoModeTurningOff] = 1;
						flags[SaveSystemSettings] = 1;
					} 
					else
					{
						global_mode[STEREO_MODE] = 1;
						flags[StereoModeTurningOn] = 1;
						flags[SaveSystemSettings] = 1;
					}

					long_press[Bank1] = 0xFFFFFFFF;
					long_press[Bank2] = 0xFFFFFFFF;
					button_state[Bank1] = UP;
					button_state[Bank2] = UP;
				}

				//RecBank and one bank button --> change the play bank to the Rec bank's value
				if (button_state[RecBank] >= SHORT_PRESSED)
				{
					if (button_state[Bank1] >= SHORT_PRESSED && button_state[Bank2] == UP)
					{
						i_param[0][BANK] 		= i_param[REC][BANK];
						flags[PlayBank1Changed] = 1;

						long_press[Bank1] 		= 0xFFFFFFFF;
						long_press[RecBank] 	= 0xFFFFFFFF;
						button_state[Bank1] 	= UP;
						button_state[RecBank] 	= UP;

						//Exit assignment mode (if we were in it)
						exit_assignment_mode();
					}
					if (button_state[Bank2] >= SHORT_PRESSED && button_state[Bank1] == UP)
					{
						i_param[1][BANK] 		= i_param[REC][BANK];
						flags[PlayBank2Changed] = 1;
						
						long_press[Bank2] 		= 0xFFFFFFFF;
						long_press[RecBank] 	= 0xFFFFFFFF;
						button_state[Bank2] 	= UP;
						button_state[RecBank] 	= UP;
					}
				}

			}


		}


		if (flags[SkipProcessButtons]==2) //ready to clear flag, but wait until all buttons are up
		{
			for (i=0;i<NUM_BUTTONS;i++)
			{
				if (i==Edit) continue;
				if (button_state[i] != UP) break;
			}
			if (i==NUM_BUTTONS) flags[SkipProcessButtons] = 0;
		}


		// Clear TIM update interrupt
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

	}


}

