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

int16_t CV_CALIBRATION_OFFSET[NUM_CV_ADCS];
int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];


//extern uint8_t led_state[NUM_PLAY_CHAN];


void set_default_calibration_values(void)
{

}


void auto_calibrate(void)
{

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

