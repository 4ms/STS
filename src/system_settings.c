/*
 * System Settings
 */

#include "globals.h"
#include "calibration.h"
#include "flash_user.h"
#include "adc.h"
#include "params.h"
#include "buttons.h"
#include "dig_pins.h"

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


void update_system_settings(void)
{



}

void update_system_settings_button_leds(void)
{


}

void update_system_settings_leds(void)
{

}

