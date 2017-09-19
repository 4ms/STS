/* user_settings.c */
#include "globals.h"
#include "dig_pins.h"
#include "params.h"
#include "user_settings.h"
#include "sts_filesystem.h"
#include "str_util.h"

extern uint8_t 				global_mode[NUM_GLOBAL_MODES];


void set_default_user_settings(void)
{
	global_mode[STEREO_MODE] = 0;
	global_mode[REC_24BITS] = 0;
	global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = AutoStop_OFF;	
	global_mode[LENGTH_FULL_START_STOP] = 0;	

	global_mode[QUANTIZE_CH1] = 0;
	global_mode[QUANTIZE_CH2] = 0;

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
		f_printf(&settings_file, "##\n");
		f_printf(&settings_file, "## [STEREO MODE] can be \"stereo\" or \"mono\" (default)\n");
		f_printf(&settings_file, "## [RECORD SAMPLE BITS] can be 24 or 16 (default)\n");
		f_printf(&settings_file, "## [AUTO STOP ON SAMPLE CHANGE] can be \"No\", \"Looping Only\" or \"Yes\" (default)\n");
		f_printf(&settings_file, "## [PLAY BUTTON STOPS WITH LENGTH AT FULL] can be \"No\" or \"Yes\" (default)\n");
		f_printf(&settings_file, "##\n");
		f_printf(&settings_file, "## Deleting this file will restore default settings\n");
		f_printf(&settings_file, "##\n\n");

		// Write the stereo mode setting
		f_printf(&settings_file, "[STEREO MODE]\n");

		if (global_mode[STEREO_MODE])
			f_printf(&settings_file, "stereo\n\n");
		else
			f_printf(&settings_file, "mono\n\n");

		// Write the 24bit record mode setting
		f_printf(&settings_file, "[RECORD SAMPLE BITS]\n");

		if (global_mode[REC_24BITS])
			f_printf(&settings_file, "24\n\n");
		else
			f_printf(&settings_file, "16\n\n");

		// Write the Auto Stop on Sample Change mode setting
		f_printf(&settings_file, "[AUTO STOP ON SAMPLE CHANGE]\n");

		if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_ALWAYS)
			f_printf(&settings_file, "Yes\n\n");
		else
		if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_OFF)
			f_printf(&settings_file, "No\n\n");
		else
		if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_LOOPING)
			f_printf(&settings_file, "Looping Only\n\n");

		// Write the Auto Stop on Sample Change mode setting
		f_printf(&settings_file, "[PLAY BUTTON STOPS WITH LENGTH AT FULL]\n");

		if (global_mode[LENGTH_FULL_START_STOP])
			f_printf(&settings_file, "Yes\n\n");
		else
			f_printf(&settings_file, "No\n\n");

		// Write the Quantize Channel 1 setting
		f_printf(&settings_file, "[QUANTIZE CHANNEL 1 1V/OCT JACK]\n");

		if (global_mode[QUANTIZE_CH1])
			f_printf(&settings_file, "Yes\n\n");
		else
			f_printf(&settings_file, "No\n\n");

		// Write the Quantize Channel 2 setting
		f_printf(&settings_file, "[QUANTIZE CHANNEL 2 1V/OCT JACK]\n");

		if (global_mode[QUANTIZE_CH2])
			f_printf(&settings_file, "Yes\n\n");
		else
			f_printf(&settings_file, "No\n\n");


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

			// Ignore lines starting with #
			if (read_buffer[0]=='#') continue;

			// Ignore blank lines
			if (read_buffer[0]==0) continue;

			// Remove /n from buffer
			read_buffer[str_len(read_buffer)-1]=0;

			// Look for a settings section header
			if (read_buffer[0]=='[')
			{
				if (str_startswith_nocase(read_buffer, "[STEREO MODE]"))
				{
					cur_setting_found = StereoMode; //stereo mode section detected
					continue;
				}

				if (str_startswith_nocase(read_buffer, "[RECORD SAMPLE BITS]"))
				{
					cur_setting_found = RecordSampleBits; //24bit recording mode section detected
					continue;
				}

				if (str_startswith_nocase(read_buffer, "[AUTO STOP ON SAMPLE CHANGE]"))
				{
					cur_setting_found = AutoStopSampleChange; //Auto Stop Mode section detected
					continue;
				}

				if (str_startswith_nocase(read_buffer, "[PLAY BUTTON STOPS WITH LENGTH AT FULL]"))
				{
					cur_setting_found = PlayStopsLengthFull; //Auto Stop Mode section detected
					continue;
				}
				if (str_startswith_nocase(read_buffer, "[QUANTIZE CHANNEL 1"))
				{
					cur_setting_found = QuantizeChannel1; //Channel 1 Quantize
					continue;
				}
				if (str_startswith_nocase(read_buffer, "[QUANTIZE CHANNEL 2"))
				{
					cur_setting_found = QuantizeChannel2; //Channel 2 Quantize
					continue;
				}
			}

			 //Look for setting values

			if (cur_setting_found==StereoMode)
			{
				if (str_startswith_nocase(read_buffer, "stereo"))
					global_mode[STEREO_MODE] = 1;

				else
				if (str_startswith_nocase(read_buffer, "mono"))
					global_mode[STEREO_MODE] = 0;

				cur_setting_found = NoSetting; //back to looking for headers
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

			if (cur_setting_found==AutoStopSampleChange)
			{
				if (str_startswith_nocase(read_buffer, "Yes"))
					global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = AutoStop_ALWAYS;
				else
				if (str_startswith_nocase(read_buffer, "Looping Only"))
					global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = AutoStop_LOOPING;
				else
					global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] = AutoStop_OFF;

				cur_setting_found = NoSetting; //back to looking for headers
			}
		
			if (cur_setting_found==PlayStopsLengthFull)
			{
				if (str_startswith_nocase(read_buffer, "Yes"))
					global_mode[LENGTH_FULL_START_STOP] = 1;

				else
					global_mode[LENGTH_FULL_START_STOP] = 0;

				cur_setting_found = NoSetting; //back to looking for headers
			}

			if (cur_setting_found==QuantizeChannel1)
			{
				if (str_startswith_nocase(read_buffer, "Yes"))
					global_mode[QUANTIZE_CH1] = 1;

				else
					global_mode[QUANTIZE_CH1] = 0;

				cur_setting_found = NoSetting; //back to looking for headers
			}

			if (cur_setting_found==QuantizeChannel2)
			{
				if (str_startswith_nocase(read_buffer, "Yes"))
					global_mode[QUANTIZE_CH2] = 1;

				else
					global_mode[QUANTIZE_CH2] = 0;

				cur_setting_found = NoSetting; //back to looking for headers
			}
		}

		res = f_close(&settings_file);
	}
	return (res);
}