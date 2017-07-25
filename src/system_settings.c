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

extern uint8_t 				global_mode[NUM_GLOBAL_MODES];
extern volatile uint32_t 	sys_tmr;
extern uint8_t 				flags[NUM_FLAGS];
extern enum PlayStates 		play_state[NUM_PLAY_CHAN];



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
//		save_user_settings(); //not needed for now, since we don't have anything in the user settings file that we change in system mode
	}
	global_mode[SYSTEM_MODE] = 0;
}

#define INITIAL_BUTTONS_DOWN -1

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


	if (sysmode_buttons_down != INITIAL_BUTTONS_DOWN)
	{
		//
		// Do system mode stuff here: check buttons/pots to set various parameters
		//
	}


	if (BOOTLOADER_BUTTONS)
	{
		bootloader_buttons_down += elapsed_time;
		
		if (bootloader_buttons_down > (44100*5))
		{
			flags[ShutdownAndBootload] = 1;
			global_mode[SYSTEM_MODE] = 0;
		}
	} else
		bootloader_buttons_down=0;


	if (SYSMODE_BUTTONS)
	{
		if (sysmode_buttons_down!=INITIAL_BUTTONS_DOWN)
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
		//Release buttons too early ===> cancel (exit without saving)
		if (sysmode_buttons_down > 10 && sysmode_buttons_down < (44100 * 3))
		{
			//Exit without saving
			exit_system_mode(1);

			//Reset state for the next time we enter system mode
			sysmode_buttons_down=INITIAL_BUTTONS_DOWN;

			 //indicate we're ready to pass over control of buttons once all buttons are released
			flags[SkipProcessButtons] = 2;
		}
		//buttons were detected up, exit the INITIAL_BUTTONS_DOWN state
		else
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
	set_ButtonLED_byPalette(Play1ButtonLED, WHITE);
	set_ButtonLED_byPalette(Play2ButtonLED, WHITE);

	set_ButtonLED_byPalette(RecBankButtonLED, ORANGE);
	set_ButtonLED_byPalette(RecButtonLED, ORANGE);
	set_ButtonLED_byPalette(Reverse1ButtonLED, ORANGE);
	set_ButtonLED_byPalette(Bank1ButtonLED, ORANGE);
	set_ButtonLED_byPalette(Bank2ButtonLED, ORANGE);
	set_ButtonLED_byPalette(Reverse2ButtonLED, ORANGE);
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

		if (global_mode[STEREO_MODE])
			f_printf(&settings_file, "stereo\n\n");
		else
			f_printf(&settings_file, "mono\n\n");

		res = f_close(&settings_file);
	}

	return (res);
}

FRESULT read_user_settings(void)
{
	FRESULT 	res;
	char		filepath[_MAX_LFN];
	char 		read_buffer[255];
	FIL			settings_file;
	uint8_t		state;


	// Check sys_dir is ok
	res = check_sys_dir();
	if (res==FR_OK)
	{
		// Open the settings file read-only
		str_cat(filepath, SYS_DIR_SLASH, SETTINGS_FILE);
		res = f_open(&settings_file, filepath, FA_READ); 
		if (res!=FR_OK) return(res);

		state = 0;
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
					state = 1; //stereo mode header detected
					continue;
				}

				// if (str_cmp(read_buffer, "[ANOTHER SETTING]"))
				// {
				// 	state = 2; //_____ header detected
				// 	continue;
				// }
			}

			 //Look for stereo mode setting
			if (state==1)
			{
				if (str_startswith_nocase(read_buffer, "stereo"))
					global_mode[STEREO_MODE] = 1;

				else
				if (str_startswith_nocase(read_buffer, "mono"))
					global_mode[STEREO_MODE] = 0;

				state=0; //back to looking for headers

				//Note: if we don't specify a setting, we will just leave the global mode alone
			}


			// if (state==2)
			// {
			// 	if (str_startswith_nocase(read_buffer, "XXXX"))
			// 		global_mode[ANOTHER_SETTING] = 1;

			// 	else
			// 	if (str_startswith_nocase(read_buffer, "YYYY"))
			// 		global_mode[ANOTHER_SETTING] = 0;

			// 	state=0; //back to looking for headers
			// }
		}

		res = f_close(&settings_file);
	}
	return (res);
}

