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
#include "flash_user.h"

extern SystemSettings *user_params;

extern int16_t i_smoothed_potadc[NUM_POT_ADCS];
extern int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

//extern uint8_t led_state[NUM_PLAY_CHAN];


void set_default_calibration_values(void)
{
	uint8_t i;

	user_params->codec_dac_calibration_dcoffset[0]=-1746;
	user_params->codec_dac_calibration_dcoffset[1]=-1746;
	user_params->codec_dac_calibration_dcoffset[2]=-1746;
	user_params->codec_dac_calibration_dcoffset[3]=-1746;

	user_params->codec_adc_calibration_dcoffset[0]=0;
	user_params->codec_adc_calibration_dcoffset[1]=0;
	user_params->codec_adc_calibration_dcoffset[2]=0;
	user_params->codec_adc_calibration_dcoffset[3]=0;

	for (i=0;i<NUM_CV_ADCS;i++)
		user_params->cv_calibration_offset[i] = 8;
}


void auto_calibrate(void)
{
	set_default_calibration_values();

	//delay_ms(7000);
	delay();
	delay();
	delay();
	delay();

	user_params->cv_calibration_offset[0] = 2048-(i_smoothed_rawcvadc[0] & 0x0FFF);
	user_params->cv_calibration_offset[1] = 2048-(i_smoothed_rawcvadc[1] & 0x0FFF);
	user_params->cv_calibration_offset[2] = -1*(i_smoothed_rawcvadc[2] & 0x0FFF);
	user_params->cv_calibration_offset[3] = -1*(i_smoothed_rawcvadc[3] & 0x0FFF);
	user_params->cv_calibration_offset[4] = -1*(i_smoothed_rawcvadc[4] & 0x0FFF);
	user_params->cv_calibration_offset[5] = -1*(i_smoothed_rawcvadc[5] & 0x0FFF);

}


void update_calibration(void)
{

}



void update_calibrate_leds(void)
{
	static uint32_t led_flasher=0;

	led_flasher+=UINT32_MAX/1665;
//	led_state[(led_flasher < UINT32_MAX/2)?1:0] = 0;

}

void update_calibration_button_leds(void)
{
	static uint32_t led_flasher=0;

	//Flash button LEDs to indicate we're in Calibrate mode
	led_flasher+=1280000;
	if (led_flasher<UINT32_MAX/20)
	{
	//	LED_INF1_OFF;
	}
	else
	{
	//	LED_INF1_ON;
	}

}

