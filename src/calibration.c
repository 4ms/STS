/*
 * Calibration
 *
 */

#include "dig_pins.h"
#include "buttons.h"
#include "globals.h"
#include "calibration.h"
#include "adc.h"
#include "params.h"
#include "rgb_leds.h"
#include "flash_user.h"

SystemCalibrations s_user_params;
SystemCalibrations *system_calibrations = &s_user_params;

extern volatile uint32_t sys_tmr;

extern SystemCalibrations *system_calibrations;

extern int16_t i_smoothed_potadc[NUM_POT_ADCS];
extern int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

extern volatile int16_t rx_buffer[codec_BUFF_LEN];

extern uint8_t	global_mode[NUM_GLOBAL_MODES];
extern uint8_t 	flags[NUM_FLAGS];


void set_default_calibration_values(void)
{
	uint8_t i;

	system_calibrations->codec_dac_calibration_dcoffset[0]=-1746;
	system_calibrations->codec_dac_calibration_dcoffset[1]=-1746;
	system_calibrations->codec_dac_calibration_dcoffset[2]=-1746;
	system_calibrations->codec_dac_calibration_dcoffset[3]=-1746;

	system_calibrations->codec_adc_calibration_dcoffset[0]=0;
	system_calibrations->codec_adc_calibration_dcoffset[1]=0;
	system_calibrations->codec_adc_calibration_dcoffset[2]=0;
	system_calibrations->codec_adc_calibration_dcoffset[3]=0;

	system_calibrations->tracking_comp[0]=1.0;
	system_calibrations->tracking_comp[1]=1.0;

	system_calibrations->led_brightness = 4;


	for (i=0;i<NUM_CV_ADCS;i++)
		system_calibrations->cv_calibration_offset[i] = 8;
}


void auto_calibrate(void)
{

	set_default_calibration_values();

	//delay_ms(7000);
	delay();
	delay();
	delay();
	delay();

	system_calibrations->cv_calibration_offset[0] = 2048-(i_smoothed_rawcvadc[0] & 0x0FFF);
	system_calibrations->cv_calibration_offset[1] = 2048-(i_smoothed_rawcvadc[1] & 0x0FFF);
	system_calibrations->cv_calibration_offset[2] = -1*(i_smoothed_rawcvadc[2] & 0x0FFF);
	system_calibrations->cv_calibration_offset[3] = -1*(i_smoothed_rawcvadc[3] & 0x0FFF);
	system_calibrations->cv_calibration_offset[4] = -1*(i_smoothed_rawcvadc[4] & 0x0FFF);
	system_calibrations->cv_calibration_offset[5] = -1*(i_smoothed_rawcvadc[5] & 0x0FFF);
	system_calibrations->cv_calibration_offset[6] = -1*(i_smoothed_rawcvadc[6] & 0x0FFF);
	system_calibrations->cv_calibration_offset[7] = -1*(i_smoothed_rawcvadc[7] & 0x0FFF);

}


void update_calibration(void)
{
	static uint32_t save_calibration_buttons_down=0;

	system_calibrations->cv_calibration_offset[0] = 2048-(i_smoothed_rawcvadc[0] & 0x0FFF);
	system_calibrations->cv_calibration_offset[1] = 2048-(i_smoothed_rawcvadc[1] & 0x0FFF);
	system_calibrations->cv_calibration_offset[2] = -1*(i_smoothed_rawcvadc[2] & 0x0FFF);
	system_calibrations->cv_calibration_offset[3] = -1*(i_smoothed_rawcvadc[3] & 0x0FFF);
	system_calibrations->cv_calibration_offset[4] = -1*(i_smoothed_rawcvadc[4] & 0x0FFF);
	system_calibrations->cv_calibration_offset[5] = -1*(i_smoothed_rawcvadc[5] & 0x0FFF);
	system_calibrations->cv_calibration_offset[6] = -1*(i_smoothed_rawcvadc[6] & 0x0FFF);
	system_calibrations->cv_calibration_offset[7] = -1*(i_smoothed_rawcvadc[7] & 0x0FFF);

	if (REV1BUT)
			system_calibrations->codec_dac_calibration_dcoffset[0]=(i_smoothed_potadc[LENGTH_POT*2]-2048-1750); //Chan 1 Out
	if (REV2BUT)
			system_calibrations->codec_dac_calibration_dcoffset[1]=(i_smoothed_potadc[LENGTH_POT*2+1]-2048-1750); //Chan 2 Out

	if (EDIT_BUTTON)
	{
		system_calibrations->codec_adc_calibration_dcoffset[0]= -1*rx_buffer[0];
		system_calibrations->codec_adc_calibration_dcoffset[1]= -1*rx_buffer[0];
	}

	if (SAVE_CALIBRATE_BUTTONS)
	{
		save_calibration_buttons_down++;
		if (save_calibration_buttons_down==3000)
		{
			save_flash_params(10);
			global_mode[CALIBRATE] = 0;
			flags[skip_process_buttons] = 2; //indicate we're ready to clear the flag, once all buttons are released

		}
	} else
		save_calibration_buttons_down=0;

}

#ifndef UINT32_MAX
#define UINT32_MAX 4294967295U
#endif


void update_calibrate_leds(void)
{
	static uint32_t led_flasher=0;

	led_flasher+=UINT32_MAX/1665;
	if (led_flasher < UINT32_MAX/2)	{SIGNALLED_ON;BUSYLED_OFF;PLAYLED1_ON;PLAYLED2_OFF;}
	else 							{SIGNALLED_OFF;BUSYLED_ON;PLAYLED1_OFF;PLAYLED2_ON;}

}

void update_calibration_button_leds(void)
{
	uint8_t ButLEDnum;

	for (ButLEDnum=0;ButLEDnum<NUM_RGBBUTTONS;ButLEDnum++)
	{
		set_ButtonLED_byPalette(ButLEDnum, 0);
	}
}

