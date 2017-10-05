/*
 * System Mode
 */

#include "globals.h"
#include "dig_pins.h"
#include "params.h"
#include "calibration.h"
#include "flash_user.h"
#include "adc.h"
#include "buttons.h"
#include "system_mode.h"
#include "rgb_leds.h"
#include "res/LED_palette.h"
#include "sampler.h"
#include "buttons.h"

extern uint8_t 				global_mode[NUM_GLOBAL_MODES];
extern volatile uint32_t 	sys_tmr;
extern uint8_t 				flags[NUM_FLAGS];
extern enum PlayStates 		play_state[NUM_PLAY_CHAN];
enum ButtonStates 			button_state[NUM_BUTTONS];
uint8_t 					button_armed[NUM_BUTTONS];

void enter_system_mode(void)
{
	global_mode[SYSTEM_MODE] = 1;
	play_state[0] = SILENT;
	play_state[1] = SILENT;
}

void exit_system_mode(uint8_t do_save)
{

	if (do_save)
	{
		save_flash_params(5);
		flags[SaveUserSettings] = 1;
	}
	global_mode[SYSTEM_MODE] = 0;
}

#define INITIAL_BUTTONS_DOWN -1

//returns 1 if the button is detected as being pressed for the first time
uint8_t check_button_pressed(uint8_t button)
{
	if (button_state[button] == DOWN && all_buttons_except(UP, (1<<button)))
	{
		button_armed[button] = 1;
	}
	else if (button_state[button] == UP && button_armed[button] == 1)
	{
		//Disable the button from doing anything until it's released
		button_state[button] = UP;
		button_armed[button] = 0;

		return(1); //first time detected as pressed
	}

	return(0);
}

uint8_t 	undo_global_mode[NUM_GLOBAL_MODES];

void save_globals_undo_state(void)
{
	uint8_t i;
	for(i=0;i<NUM_GLOBAL_MODES;i++)
		undo_global_mode[i] = global_mode[i];
}

void restore_globals_undo_state(void)
{
	uint8_t i;
	for(i=0;i<NUM_GLOBAL_MODES;i++)
		global_mode[i] = undo_global_mode[i];
}

void update_system_mode(void)
{
	static int32_t sysmode_buttons_down=INITIAL_BUTTONS_DOWN;
	static int32_t bootloader_buttons_down=0;

	static uint32_t last_sys_tmr=0;

	uint32_t elapsed_time;


	if (sys_tmr < last_sys_tmr) //then sys_tmr wrapped from 0xFFFFFFFF to 0x00000000
		elapsed_time = (0xFFFFFFFF - last_sys_tmr) + sys_tmr;
	else
		elapsed_time = (sys_tmr - last_sys_tmr);
	last_sys_tmr = sys_tmr;


	if (sysmode_buttons_down == 0)
	{
		//
		// Check buttons to set various parameters
		//

		//Rec : Toggle 24-bit mode
		if (check_button_pressed(Rec)){	
			if (global_mode[REC_24BITS]) 									global_mode[REC_24BITS] = 0;
			else															global_mode[REC_24BITS] = 1; 
		}

		//Play2 : Toggle Auto-Stop-on-Sample-Change
		if (check_button_pressed(Play2)){	
			if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_OFF) 		global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = AutoStop_ALWAYS;
			else
			if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_ALWAYS) 	global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = AutoStop_LOOPING;
			else						 									global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = AutoStop_OFF; 
		}

		//Play1 : Toggle LENGTH_FULL_START_STOP
		if (check_button_pressed(Play1)){	
			if (global_mode[LENGTH_FULL_START_STOP])						global_mode[LENGTH_FULL_START_STOP] = 0;
			else						 									global_mode[LENGTH_FULL_START_STOP] = 1; 
		}

		//Bank1 : Toggle QUANTIZE_CH1
		if (check_button_pressed(Bank1)){	
			if (global_mode[QUANTIZE_CH1])									global_mode[QUANTIZE_CH1] = 0;
			else						 									global_mode[QUANTIZE_CH1] = 1; 
		}

		//Bank2 : Toggle QUANTIZE_CH2
		if (check_button_pressed(Bank2)){	
			if (global_mode[QUANTIZE_CH2])									global_mode[QUANTIZE_CH2] = 0;
			else						 									global_mode[QUANTIZE_CH2] = 1; 
		}

		//Rev1 : Change envelopes on/off
		//Toggles 00 -> 01 -> 10 -> 11 -> 00 ->...
		if (check_button_pressed(Rev1)){	
			if (global_mode[PERC_ENVELOPE])	{								global_mode[PERC_ENVELOPE] = 0;
																			global_mode[FADEUPDOWN_ENVELOPE] = 1 - global_mode[FADEUPDOWN_ENVELOPE];}
			else						 									global_mode[PERC_ENVELOPE] = 1; 
		}


		//Edit+Play1 : Save and Exit
		if (button_state[Edit] >= SHORT_PRESSED && button_state[Play1] >= SHORT_PRESSED && all_buttons_except(UP, (1<<Edit) | (1<<Play1)))
		{
			//Save and Exit
			exit_system_mode(1);

			//Reset state for the next time we enter system mode
			sysmode_buttons_down=INITIAL_BUTTONS_DOWN;

			 //indicate we're ready to pass over control of buttons once all buttons are released
			flags[SkipProcessButtons] = 2;
		}

		//Edit+Rev2 : Revert
		if (button_state[Edit] >= SHORT_PRESSED && button_state[Rev2] >= SHORT_PRESSED	&& all_buttons_except(UP, (1<<Edit) | (1<<Rev2)))
		{
			restore_globals_undo_state();
		}
	}


	if (BOOTLOADER_BUTTONS)
	{
		bootloader_buttons_down += elapsed_time;
		
		if (bootloader_buttons_down > (BASE_SAMPLE_RATE*3))
		{
			flags[ShutdownAndBootload] = 1;

			set_ButtonLED_byPalette(Play1ButtonLED, GREEN);
			set_ButtonLED_byPalette(Play2ButtonLED, OFF);
			set_ButtonLED_byPalette(RecBankButtonLED, OFF);
			set_ButtonLED_byPalette(RecButtonLED, OFF);
			set_ButtonLED_byPalette(Reverse1ButtonLED, YELLOW);
			set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
			set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
			set_ButtonLED_byPalette(Reverse2ButtonLED, OFF);
			display_all_ButtonLEDs();

			global_mode[SYSTEM_MODE] = 0;
		}
	} else
		bootloader_buttons_down=0;


	if (SYSMODE_BUTTONS)
	{
		//Set the undo state on our initial entry
		if (sysmode_buttons_down==INITIAL_BUTTONS_DOWN)
		{
			save_globals_undo_state();
		}

		//If buttons are found down after they've been released, we will exit
		//Check if they're down long enough to save+exit, or if not then revert+exit
		else
		{
			sysmode_buttons_down += elapsed_time;
			//Hold buttons down for a while ==> save and exit
			if (sysmode_buttons_down >= (BASE_SAMPLE_RATE * 3))
			{
				//Save and Exit
				exit_system_mode(1);

				//Reset state for the next time we enter system mode
				sysmode_buttons_down=INITIAL_BUTTONS_DOWN;

				 //indicate we're ready to pass over control of buttons once all buttons are released
				flags[SkipProcessButtons] = 2;
			}
		}
	} else
	{
		//Release buttons too early ===> revert+exit
		if (sysmode_buttons_down > 10 && sysmode_buttons_down < (BASE_SAMPLE_RATE * 3))
		{
			restore_globals_undo_state();

			//Exit without saving
			exit_system_mode(0);

			//Reset state for the next time we enter system mode
			sysmode_buttons_down=INITIAL_BUTTONS_DOWN;

			 //indicate we're ready to pass over control of buttons once all buttons are released
			flags[SkipProcessButtons] = 2;
		}
		//buttons were detected up, exit the INITIAL_BUTTONS_DOWN state
		else if (NO_BUTTONS)
			sysmode_buttons_down = 0;
	}

}

void update_system_mode_leds(void)
{
	uint32_t tm;

	tm = (sys_tmr & ((1<<14) | (1<<13))) >> 13;
	if (tm==0b00)
	{
		PLAYLED1_ON;
		SIGNALLED_OFF;
		BUSYLED_OFF;
		PLAYLED2_OFF;
	}	else
	if (tm==0b01)
	{
		PLAYLED1_OFF;
		SIGNALLED_ON;
		BUSYLED_OFF;
		PLAYLED2_OFF;
	}	else
	if (tm==0b10)
	{
		PLAYLED1_OFF;
		SIGNALLED_OFF;
		BUSYLED_ON;
		PLAYLED2_OFF;
	}	else
	{
		PLAYLED1_OFF;
		SIGNALLED_OFF;
		BUSYLED_OFF;
		PLAYLED2_ON;
	}
}

void update_system_mode_button_leds(void)
{
	if (flags[ShutdownAndBootload] != 1)
	{

		if (global_mode[REC_24BITS]) 									set_ButtonLED_byPalette(RecButtonLED, BLUE); //24bit recording
		else															set_ButtonLED_byPalette(RecButtonLED, ORANGE); //16bit recording

		if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_OFF) 		set_ButtonLED_byPalette(Play2ButtonLED, GREEN); //No auto stop on sample change
		else
		if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_ALWAYS) 	set_ButtonLED_byPalette(Play2ButtonLED, RED); //Auto Stop on sample change
		else						 									set_ButtonLED_byPalette(Play2ButtonLED, BLUE); //Auto Stop only when Looping

		if (global_mode[LENGTH_FULL_START_STOP]) 						set_ButtonLED_byPalette(Play1ButtonLED, RED); //Tapping play button with Length>98% start/stops
		else															set_ButtonLED_byPalette(Play1ButtonLED, BLUE); //tapping play button always re-starts

		if (global_mode[QUANTIZE_CH1]) 									set_ButtonLED_byPalette(Bank1ButtonLED, BLUE); //Ch1 quantized to semitones
		else															set_ButtonLED_byPalette(Bank1ButtonLED, ORANGE); //Ch1 not quantized

		if (global_mode[QUANTIZE_CH2]) 									set_ButtonLED_byPalette(Bank2ButtonLED, BLUE); //Ch2 quantized to semitones
		else															set_ButtonLED_byPalette(Bank2ButtonLED, ORANGE); //Ch2 not quantized

		set_ButtonLED_byPalette(RecBankButtonLED, ORANGE);

		if (button_state[Edit] >= DOWN && all_buttons_except(UP, (1<<Edit)))
		{
			set_ButtonLED_byPalette(Reverse1ButtonLED, (FW_MAJOR_VERSION)%14);
			set_ButtonLED_byPalette(Reverse2ButtonLED, (FW_MINOR_VERSION)%14);
		}
		else
		{
			set_ButtonLED_byPalette(Reverse2ButtonLED, ORANGE);

			if (global_mode[PERC_ENVELOPE]) {
				if (global_mode[FADEUPDOWN_ENVELOPE]) 					set_ButtonLED_byPalette(Reverse1ButtonLED, ORANGE);	//Orange: All envelopes, default mode
				else													set_ButtonLED_byPalette(Reverse1ButtonLED, YELLOW);	//Yellow: Percussive envelopes only, no start/end sample envelopes
			} else {
				if (global_mode[FADEUPDOWN_ENVELOPE]) 					set_ButtonLED_byPalette(Reverse1ButtonLED, RED);	//Red: Start/end sample envelopes only, no percussive envelopes
				else													set_ButtonLED_byPalette(Reverse1ButtonLED, DIM_RED); //Dim Red: no envelopes

			}
		}
	}
}
