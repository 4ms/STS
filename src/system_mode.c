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

void update_system_mode(void)
{
	static int32_t sysmode_buttons_down=INITIAL_BUTTONS_DOWN;
	static int32_t bootloader_buttons_down=0;
	static uint8_t button_armed[NUM_BUTTONS];

	static uint32_t last_sys_tmr=0;

	static uint8_t  undo_rec_24bits;
	static uint8_t  undo_auto_stop;
	uint32_t elapsed_time;


	if (sys_tmr < last_sys_tmr) //then sys_tmr wrapped from 0xFFFFFFFF to 0x00000000
		elapsed_time = (0xFFFFFFFF - last_sys_tmr) + sys_tmr;
	else
		elapsed_time = (sys_tmr - last_sys_tmr);
	last_sys_tmr = sys_tmr;


	if (sysmode_buttons_down == 0)
	{
		//
		// Do system mode stuff here: check buttons/pots to set various parameters
		//

		//Rec : Toggle 24-bit mode
		if (button_state[Rec] == DOWN \
			&& all_buttons_except(UP, (1<<Rec)))
			// && button_state[Play1]==UP && button_state[Play2]==UP && button_state[Edit]==UP && button_state[RecBank]==UP && button_state[Rev1]==UP && button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP)
		{
			button_armed[Rec] = 1;
		}
		else if (button_state[Rec] == UP && button_armed[Rec] == 1)
		{
			if (global_mode[REC_24BITS]) global_mode[REC_24BITS] = 0;
			else						 global_mode[REC_24BITS] = 1; 

			//Disable the button from doing anything until it's released
			button_state[Rec] = UP;
			button_armed[Rec] = 0;
		}

		//Play2 : Toggle Auto-Stop-on-Sample-Change
		if (button_state[Play2] == DOWN \
			&& all_buttons_except(UP, (1<<Play2)))
			// && button_state[Play1]==UP && button_state[Rec]==UP && button_state[Edit]==UP && button_state[RecBank]==UP && button_state[Rev1]==UP && button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP)
		{
			button_armed[Play2] = 1;
		}
		else if (button_state[Play2] == UP && button_armed[Play2] == 1)
		{
			if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE])global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = 0;
			else						 				global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = 1; 

			//Disable the button from doing anything until it's released
			button_state[Play2] = UP;
			button_armed[Play2] = 0;
		}


		//Edit+Play1 : Save and Exit
		if (button_state[Edit] >= SHORT_PRESSED && button_state[Play1] >= SHORT_PRESSED \
			&& all_buttons_except(UP, (1<<Edit) | (1<<Play1)))
//			&& button_state[Rec]==UP && button_state[Play2]==UP && button_state[RecBank]==UP && button_state[Rev1]==UP && button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP)
		{
			//Save and Exit
			exit_system_mode(1);

			//Reset state for the next time we enter system mode
			sysmode_buttons_down=INITIAL_BUTTONS_DOWN;

			 //indicate we're ready to pass over control of buttons once all buttons are released
			flags[SkipProcessButtons] = 2;
		}

		//Edit+Rev2 : Revert
		if (button_state[Edit] >= SHORT_PRESSED && button_state[Rev2] >= SHORT_PRESSED \
			&& all_buttons_except(UP, (1<<Edit) | (1<<Rev2)))
//			&& button_state[Play1]==UP && button_state[Play2]==UP && button_state[RecBank]==UP && button_state[Rec]==UP && button_state[Rev1]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP)
		{
			global_mode[REC_24BITS] 				= undo_rec_24bits;
			global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]	= undo_auto_stop;
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
			undo_rec_24bits = global_mode[REC_24BITS];
			undo_auto_stop 	= global_mode[AUTO_STOP_ON_SAMPLE_CHANGE];
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
			//Revert to undo state
			global_mode[REC_24BITS] 				= undo_rec_24bits;
			global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]	= undo_auto_stop;

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

		if (global_mode[REC_24BITS]) 					set_ButtonLED_byPalette(RecButtonLED, BLUE); //24bit recording
		else											set_ButtonLED_byPalette(RecButtonLED, ORANGE); //16bit recording

		if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]) 	set_ButtonLED_byPalette(Play2ButtonLED, RED); //Auto Stop on sample change
		else											set_ButtonLED_byPalette(Play2ButtonLED, GREEN); //No auto stop on sample change


		set_ButtonLED_byPalette(Play1ButtonLED, WHITE);
		set_ButtonLED_byPalette(RecBankButtonLED, ORANGE);
		set_ButtonLED_byPalette(Reverse1ButtonLED, ORANGE);
		set_ButtonLED_byPalette(Bank1ButtonLED, ORANGE);
		set_ButtonLED_byPalette(Bank2ButtonLED, ORANGE);
		set_ButtonLED_byPalette(Reverse2ButtonLED, ORANGE);
	}
}
