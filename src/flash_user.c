//ToDo:
//separate mode[][] into two arrays: one for modes stored in flash, and one for modes that the user changes "on the fly"
//mode[][] (On the fly): REV INF TIMEMODE_POT TIMEMODE_JACK
//global_modes[]: DC_INPUT, CALIBRATE, SYSTEM_SETTING
//flash_setting[]: CV_CAL ADC_CAL, DAC_CAL, TRACKING_COMP, LED_DIM, LOOP_CLOCK, etc...
//Store and recall the entire flash_setting array in one read/write operation (so we don't have to spell out the address of each element etc)

#include "globals.h"
#include "flash.h"
#include "flash_user.h"
#include "calibration.h"
#include "system_settings.h"
#include "params.h"
#include "adc.h"
#include "leds.h"
#include "dig_pins.h"
#include <string.h>

SystemSettings s_SRAM_user_params;
SystemSettings s_user_params;
SystemSettings *SRAM_user_params = &s_SRAM_user_params;
SystemSettings *user_params = &s_user_params;


extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];

extern int16_t i_smoothed_cvadc[NUM_POT_ADCS];



#define FLASH_ADDR_userparams 0x08004000

#define FLASH_SYMBOL_bankfilled 0x01
#define FLASH_SYMBOL_startupoffset 0xAA
#define FLASH_SYMBOL_firmwareoffset 0xAA550000


void set_firmware_version(void)
{
	SRAM_user_params->firmware_version = FW_VERSION;
}


void factory_reset(uint8_t loop_afterwards)
{
	uint8_t i,fail=0;

	auto_calibrate();
	set_firmware_version();
	set_default_system_settings();

	store_params_into_sram();
	write_all_params_to_FLASH();


	if (loop_afterwards)
	{
		fail=0;
		for (i=0;i<6;i++)
		{
			if (user_params->cv_calibration_offset[i]>100 || user_params->cv_calibration_offset[i]<-100 )
				fail=1;
		}

		if (i_smoothed_cvadc[0] > 2150 || i_smoothed_cvadc[0]<1950 || i_smoothed_cvadc[1] > 2150 || i_smoothed_cvadc[1]<1950)
			fail=1;

		while(1)
		{
			if (fail)
				blink_all_lights(200); //error: did not auto-calibrate!
			else
				chase_all_lights(10); //All good!
		}
	}
}

uint32_t load_flash_params(void)
{
	uint8_t i;
	uint8_t *src;
	uint8_t *dst;

	read_all_params_from_FLASH();

	if (SRAM_user_params->firmware_version > 0 && SRAM_user_params->firmware_version < 500){

		//memcpy((uint8_t *)(user_params), (const uint8_t *)(SRAM_user_params), sizeof(SystemSettings));

		src = (uint8_t *)SRAM_user_params;
		dst = (uint8_t *)user_params;

		for (i=0;i<sizeof(SystemSettings);i++)
		{
			*src++ = *dst++;
		}

		return (SRAM_user_params->firmware_version);

	} else
	{
    	set_firmware_version();

		return(0); //No calibration data found
	}
}


void save_flash_params(void)
{
	uint32_t i;

	//copy global variables to SRAM staging area
	store_params_into_sram();

	//copy SRAM variables to FLASH
	write_all_params_to_FLASH();

	for (i=0;i<10;i++){
		CLIPLED1_ON;
		CLIPLED2_OFF;
		delay_ms(10);

		CLIPLED1_OFF;
		CLIPLED2_ON;
		delay_ms(10);
	}
}


void store_params_into_sram(void)
{
	//memcpy((uint8_t *)(SRAM_user_params), (const uint8_t *)(user_params), sizeof(SystemSettings));
	uint8_t i;
	uint8_t *src;
	uint8_t *dst;

	src = (uint8_t *)user_params;
	dst = (uint8_t *)SRAM_user_params;

	for (i=0;i<sizeof(SystemSettings);i++)
	{
		*src++ = *dst++;
	}
}


void write_all_params_to_FLASH(void)
{
	uint8_t i;
	uint8_t *addr;

	//flash_begin_open_program();

	//flash_open_erase_sector(FLASH_ADDR_userparams);

	SRAM_user_params->firmware_version += FLASH_SYMBOL_firmwareoffset;

	addr = (uint8_t *)SRAM_user_params;

	for(i=0;i<sizeof(SystemSettings);i++)
	{
	//	flash_open_program_byte(*addr++, FLASH_ADDR_userparams + i);
	}

	//flash_end_open_program();
}


void read_all_params_from_FLASH(void)
{
	uint8_t i;
	//uint32_t addr;
	uint8_t *ptr;

	ptr = (uint8_t *)SRAM_user_params;

	for(i=0;i<sizeof(SystemSettings);i++)
	{
		*ptr++ = flash_read_byte(FLASH_ADDR_userparams + i);
	}

	SRAM_user_params->firmware_version -= FLASH_SYMBOL_firmwareoffset;

/*
	addr = FLASH_ADDR_userparams;

	SRAM_user_params->firmware_version = flash_read_word(addr) - FLASH_SYMBOL_firmwareoffset;
	addr += 4;

	for (i=0;i<8;i++)
	{
		SRAM_user_params->cv_calibration_offset[i] = (int32_t)flash_read_word(addr);
		addr+=4;
	}
	for (i=0;i<2;i++)
	{
		SRAM_user_params->codec_adc_calibration_dcoffset[i] = (int32_t)flash_read_word(addr);
		addr+=4;
	}
	for (i=0;i<2;i++)
	{
		SRAM_user_params->codec_dac_calibration_dcoffset[i] = (int32_t)flash_read_word(addr);
		addr+=4;
	}
	for (i=0;i<2;i++)
	{
		SRAM_user_params->tracking_comp[i] = (float)flash_read_word(addr);
		addr+=4;
	}
*/


	for (i=0;i<8;i++)
	{
		if (SRAM_user_params->cv_calibration_offset[i] > 100 || SRAM_user_params->cv_calibration_offset[i] < -100)
			SRAM_user_params->cv_calibration_offset[i] = 0;
	}
	for (i=0;i<8;i++)
	{
		if (SRAM_user_params->codec_adc_calibration_dcoffset[i] > 4000 || SRAM_user_params->codec_adc_calibration_dcoffset[i] < -100)
			SRAM_user_params->codec_adc_calibration_dcoffset[i] = 0;
	}
	for (i=0;i<8;i++)
	{
		if (SRAM_user_params->codec_dac_calibration_dcoffset[i] > 4000 || SRAM_user_params->codec_dac_calibration_dcoffset[i] < -4000)
			SRAM_user_params->codec_dac_calibration_dcoffset[i] = 0;
	}
	for (i=0;i<2;i++)
	{
		if (SRAM_user_params->tracking_comp[i] > 1.25 || SRAM_user_params->tracking_comp[i] < 0.75)
			SRAM_user_params->tracking_comp[i] = 1.0;
	}

}


