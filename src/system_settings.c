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

extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

//extern uint32_t flash_firmware_version;


uint8_t disable_mode_changes=0;

void set_default_system_settings(void)
{
}


void check_entering_system_mode(void)
{

	static int32_t ctr=0;

	if (ENTER_SYSMODE_BUTTONS)
	{
		if (ctr==-1) //waiting for button release
		{
//			LED_PINGBUT_ON;
		}
		else ctr+=40;

		if (ctr>100000)
		{
			if (global_mode[SYSTEM_SETTINGS] == 0)
			{
				global_mode[SYSTEM_SETTINGS] = 1;
				global_mode[CALIBRATE] = 0;
				ctr=-1;
			}
			else
			{
				save_flash_params(10); //flash lights 10 times
				global_mode[SYSTEM_SETTINGS] = 0;
				ctr=-1;
				disable_mode_changes=0;
			}
		}
	}
	else
	{
		if ((ctr>1000) && (ctr<=100000) && (global_mode[SYSTEM_SETTINGS] == 1)) //released buttons too early ==> cancel (exit without save)
		{
			global_mode[SYSTEM_SETTINGS] = 0;
			disable_mode_changes=0;

		}
		ctr=0;
	}

}

FRESULT save_system_settings(void)
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

FRESULT read_system_settings(void)
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

void update_system_settings(void)
{



}

void update_system_settings_button_leds(void)
{


}

void update_system_settings_leds(void)
{

}

