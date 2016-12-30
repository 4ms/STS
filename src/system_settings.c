/*
 * System Settings
 *
 * ????Time A/B knob:
 * -Quantized normally, unquantized when turned with Reverse held down (default)
 * -Unquantized normally, quantized when turned with Reverse held down
 *
 * ????Time A/B CV jack:
 * -Quantized (default)
 * -Always unquantized (...try this?)
 * -Follows mode of knob (quantized until Time knob is wiggled with Reverse held down)
 *
 */

#include "globals.h"
#include "system_settings.h"
#include "adc.h"
#include "params.h"
#include "flash_user.h"
#include "buttons.h"
#include "dig_pins.h"


extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
//extern uint8_t 	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
//extern uint8_t 	settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern float global_param[NUM_GLOBAL_PARAMS];
extern uint32_t global_i_param[NUM_GLOBAL_PARAMS];


//extern uint32_t flash_firmware_version;


uint8_t disable_mode_changes=0;

void set_default_system_settings(void)
{
	f_param[0][TRACKING_COMP]=1.0;
	f_param[1][TRACKING_COMP]=1.0;

	global_param[SLOW_FADE_INCREMENT] = 0.001;
	global_i_param[LED_BRIGHTNESS] = 4;

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
				save_flash_params();
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

//100 * 0.2ms = 20ms
#define TRIG_CNTS 200
//1665 * 0.2ms = 333ms
#define GATE_CNTS 1665

void update_system_settings_leds(void)
{

}

