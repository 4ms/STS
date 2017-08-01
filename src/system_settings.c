/*
 * System Settings
 */

#include "globals.h"
#include "dig_pins.h"
#include "params.h"
#include "calibration.h"
#include "flash_user.h"
#include "adc.h"
#include "buttons.h"
#include "system_settings.h"
#include "file_util.h"
#include "sts_filesystem.h"
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
		flags[SaveSystemSettings] = 1;
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

		//Rev1 : Toggle 24-bit mode
		if (button_state[Rec] == DOWN \
			&& button_state[Play1]==UP && button_state[Play2]==UP && button_state[Edit]==UP && button_state[RecBank]==UP && button_state[Rev1]==UP && button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP)
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

		//Edit+Play1 : Save and Exit
		if (button_state[Edit] >= SHORT_PRESSED && button_state[Play1] >= SHORT_PRESSED \
			&& button_state[Rec]==UP && button_state[Play2]==UP && button_state[RecBank]==UP && button_state[Rev1]==UP && button_state[Rev2]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP)
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
			&& button_state[Play1]==UP && button_state[Play2]==UP && button_state[RecBank]==UP && button_state[Rec]==UP && button_state[Rev1]==UP && button_state[Bank1]==UP && button_state[Bank2]==UP)
		{
			global_mode[REC_24BITS] = undo_rec_24bits;
		}
	}


	if (BOOTLOADER_BUTTONS)
	{
		bootloader_buttons_down += elapsed_time;
		
		if (bootloader_buttons_down > (44100*5))
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
		}

		//If buttons are found down after they've been released, we will exit
		//Check if they're down long enough to save+exit, or if not then revert+exit
		else
		{
			sysmode_buttons_down += elapsed_time;
			//Hold buttons down for a while ==> save and exit
			if (sysmode_buttons_down > (44100 * 3))
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
		if (sysmode_buttons_down > 10 && sysmode_buttons_down < (44100 * 3))
		{
			//Revert to undo state
			global_mode[REC_24BITS] = undo_rec_24bits;

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
		set_ButtonLED_byPalette(Play1ButtonLED, WHITE);
		set_ButtonLED_byPalette(Play2ButtonLED, WHITE);
		set_ButtonLED_byPalette(RecBankButtonLED, ORANGE);

		if (global_mode[REC_24BITS]) 	set_ButtonLED_byPalette(RecButtonLED, BLUE); //24bit recording
		else							set_ButtonLED_byPalette(RecButtonLED, ORANGE); //16bit recording

		set_ButtonLED_byPalette(Reverse1ButtonLED, ORANGE);
		set_ButtonLED_byPalette(Bank1ButtonLED, ORANGE);
		set_ButtonLED_byPalette(Bank2ButtonLED, ORANGE);
		set_ButtonLED_byPalette(Reverse2ButtonLED, ORANGE);
	}
}


/* user_settings.c */

void set_default_user_settings(void)
{
	global_mode[STEREO_MODE] = 0;
}


FRESULT save_user_settings(void)
{
	FRESULT 	res;
	char		filepath[_MAX_LFN];
	FIL			settings_file;

	// Check sys_dir is ok
	res = check_sys_dir();
	if (res==FR_OK)
	{
		// Create/overwrite the settings file
		str_cat(filepath, SYS_DIR_SLASH, SETTINGS_FILE);
		res = f_open(&settings_file, filepath, FA_CREATE_ALWAYS | FA_WRITE); 
		if (res!=FR_OK) return(res);

		//Write the header
		f_printf(&settings_file, "##\n");
		f_printf(&settings_file, "## 4ms Stereo Triggered Sampler\n");
		f_printf(&settings_file, "## Settings File\n");
		f_printf(&settings_file, "## http://www.4mscompany.com/sts.php\n");
		f_printf(&settings_file, "##\n\n");

		// Write the stereo mode setting
		f_printf(&settings_file, "[STEREO MODE]\n");
		f_printf(&settings_file, "# Choose \"stereo\" or \"mono\" (default)\n");

		if (global_mode[STEREO_MODE])
			f_printf(&settings_file, "stereo\n\n");
		else
			f_printf(&settings_file, "mono\n\n");

		// Write the 24bit record mode setting
		f_printf(&settings_file, "[RECORD SAMPLE BITS]\n");
		f_printf(&settings_file, "# Choose 16 (default) or 24\n");

		if (global_mode[REC_24BITS])
			f_printf(&settings_file, "24\n\n");
		else
			f_printf(&settings_file, "16\n\n");

		res = f_close(&settings_file);
	}

	return (res);
}

enum Settings
{
	NoSetting,
	StereoMode,
	RecordSampleBits,

	NUM_SETTINGS_ENUM
};

FRESULT read_user_settings(void)
{
	FRESULT 	res;
	char		filepath[_MAX_LFN];
	char 		read_buffer[255];
	FIL			settings_file;
	uint8_t		cur_setting_found;


	// Check sys_dir is ok
	res = check_sys_dir();
	if (res==FR_OK)
	{
		// Open the settings file read-only
		str_cat(filepath, SYS_DIR_SLASH, SETTINGS_FILE);
		res = f_open(&settings_file, filepath, FA_READ); 
		if (res!=FR_OK) return(res);

		cur_setting_found = NoSetting;
		while (!f_eof(&settings_file))
		{
			// Read next line
			if (f_gets(read_buffer, 255, &settings_file) == 0) return(FR_INT_ERR);

			// Remove /n from buffer
			read_buffer[str_len(read_buffer)-1]=0;

			// Ignore lines starting with #
			if (read_buffer[0]=='#') continue;

			// Ignore blank lines
			if (read_buffer[0]==0) continue;

			// Look for a settings header
			if (read_buffer[0]=='[')
			{
				if (str_startswith_nocase(read_buffer, "[STEREO MODE]"))
				{
					cur_setting_found = StereoMode; //stereo mode header detected
					continue;
				}

				if (str_startswith_nocase(read_buffer, "[RECORD SAMPLE BITS]"))
				{
					cur_setting_found = RecordSampleBits; //24bit recording mode header detected
					continue;
				}
			}

			 //Look for stereo mode setting
			if (cur_setting_found==StereoMode)
			{
				if (str_startswith_nocase(read_buffer, "stereo"))
					global_mode[STEREO_MODE] = 1;

				else
				if (str_startswith_nocase(read_buffer, "mono"))
					global_mode[STEREO_MODE] = 0;

				cur_setting_found = NoSetting; //back to looking for headers

				//Note: if we don't specify a setting, we will just leave the global mode alone
			}


			if (cur_setting_found==RecordSampleBits)
			{
				if (str_startswith_nocase(read_buffer, "24"))
					global_mode[REC_24BITS] = 1;

				else
				//if (str_startswith_nocase(read_buffer, "16"))
					global_mode[REC_24BITS] = 0;

				cur_setting_found = NoSetting; //back to looking for headers
			}
		}

		res = f_close(&settings_file);
	}
	return (res);
}

