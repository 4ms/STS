#include "globals.h"
#include "flash.h"
#include "flash_user.h"
#include "calibration.h"
#include "params.h"
#include "adc.h"
#include "leds.h"
#include "dig_pins.h"
#include <string.h>

SystemCalibrations s_staging_user_params;
SystemCalibrations *staging_system_calibrations = &s_staging_user_params;

extern SystemCalibrations *system_calibrations;


extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];

extern int16_t i_smoothed_cvadc[NUM_POT_ADCS];



#define FLASH_ADDR_userparams 0x08004000

#define FLASH_SYMBOL_bankfilled 0x01
#define FLASH_SYMBOL_startupoffset 0xAA
#define FLASH_SYMBOL_firmwareoffset 0xAA550000


void set_firmware_version(void)
{
	staging_system_calibrations->firmware_version = FW_VERSION;
	system_calibrations->firmware_version = FW_VERSION;
}


void factory_reset(uint8_t loop_afterwards)
{
	uint8_t i,fail=0;

	auto_calibrate();
	set_firmware_version();
//	set_default_system_settings();

	copy_system_calibrations_into_staging();
	write_all_system_calibrations_to_FLASH();


	if (loop_afterwards)
	{
		fail=0;
		for (i=0;i<6;i++)
		{
			if (system_calibrations->cv_calibration_offset[i]>100 || system_calibrations->cv_calibration_offset[i]<-100 )
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

	read_all_system_calibrations_from_FLASH(); //into staging area

	if (staging_system_calibrations->firmware_version > 0 && staging_system_calibrations->firmware_version < 500) //valid firmware version
	{

		//memcopy: system_calibrations = staging_system_calibrations;
		dst = (uint8_t *)system_calibrations;
		src = (uint8_t *)staging_system_calibrations;
		for (i=0;i<sizeof(SystemCalibrations);i++)
			*dst++ = *src++;
		
		return (staging_system_calibrations->firmware_version);

	} else
	{
    	set_firmware_version();

		return(0); //No calibration data found
	}
}


void save_flash_params(uint8_t num_led_blinks)
{
	uint32_t i;

	copy_system_calibrations_into_staging();

	write_all_system_calibrations_to_FLASH();

	for (i=0;i<num_led_blinks;i++){
		SIGNALLED_ON;
		BUSYLED_ON;
		PLAYLED1_ON;
		PLAYLED2_ON;
		delay_ms(10);

		SIGNALLED_OFF;
		BUSYLED_OFF;
		PLAYLED1_OFF;
		PLAYLED2_OFF;

		delay_ms(10);
	}
}


void copy_system_calibrations_into_staging(void)
{
	uint8_t i;
	uint8_t *src;
	uint8_t *dst;

	//copy: staging_system_calibrations = system_calibrations;
	dst = (uint8_t *)staging_system_calibrations;
	src = (uint8_t *)system_calibrations;
	for (i=0;i<sizeof(SystemCalibrations);i++)
		*dst++ = *src++;
}


void write_all_system_calibrations_to_FLASH(void)
{
	uint8_t i;
	uint8_t *addr;

	flash_begin_open_program();

	flash_open_erase_sector(FLASH_ADDR_userparams);

	staging_system_calibrations->firmware_version += FLASH_SYMBOL_firmwareoffset;

	addr = (uint8_t *)staging_system_calibrations;

	for(i=0;i<sizeof(SystemCalibrations);i++)
	{
		flash_open_program_byte(*addr++, FLASH_ADDR_userparams + i);
	}

	flash_end_open_program();
}


void read_all_system_calibrations_from_FLASH(void)
{
	uint8_t i;
	uint8_t *ptr;
	uint8_t invalid_fw_version;

	ptr = (uint8_t *)staging_system_calibrations;

	for(i=0;i<sizeof(SystemCalibrations);i++)
	{
		*ptr++ = flash_read_byte(FLASH_ADDR_userparams + i);
	}

	staging_system_calibrations->firmware_version -= FLASH_SYMBOL_firmwareoffset;

	if (staging_system_calibrations->firmware_version < 1 || staging_system_calibrations->firmware_version > 500)
		invalid_fw_version = 1;

	for (i=0;i<8;i++)
	{
		if (invalid_fw_version || staging_system_calibrations->cv_calibration_offset[i] > 100 || staging_system_calibrations->cv_calibration_offset[i] < -100)
			staging_system_calibrations->cv_calibration_offset[i] = 0;
	}
	for (i=0;i<2;i++)
	{
		if (invalid_fw_version || staging_system_calibrations->codec_adc_calibration_dcoffset[i] > 4000 || staging_system_calibrations->codec_adc_calibration_dcoffset[i] < -4000)
			staging_system_calibrations->codec_adc_calibration_dcoffset[i] = 0;

		if (invalid_fw_version || staging_system_calibrations->codec_dac_calibration_dcoffset[i] > 4000 || staging_system_calibrations->codec_dac_calibration_dcoffset[i] < -4000)
			staging_system_calibrations->codec_dac_calibration_dcoffset[i] = 0;
	
		if (invalid_fw_version || staging_system_calibrations->tracking_comp[i] > 1.25 || staging_system_calibrations->tracking_comp[i] < 0.75 || (*(uint32_t *)(&staging_system_calibrations->tracking_comp[i])==0xFFFFFFFF))
			staging_system_calibrations->tracking_comp[i] = 1.0;
	}

	if (invalid_fw_version || staging_system_calibrations->led_brightness > 15 || staging_system_calibrations->led_brightness < 1)
		staging_system_calibrations->led_brightness = 4;

}


