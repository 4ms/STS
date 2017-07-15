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

extern int16_t bracketed_potadc[NUM_POT_ADCS];

extern int16_t i_smoothed_potadc[NUM_POT_ADCS];
extern int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

extern volatile int16_t rx_buffer[codec_BUFF_LEN];

extern uint8_t	global_mode[NUM_GLOBAL_MODES];
extern uint8_t 	flags[NUM_FLAGS];

uint8_t playbut2_color, playbut1_color;


void set_default_calibration_values(void)
{
	uint8_t i;

	system_calibrations->codec_dac_calibration_dcoffset[0]= -1800;
	system_calibrations->codec_dac_calibration_dcoffset[1]= -1800;

	system_calibrations->codec_adc_calibration_dcoffset[0]=0;
	system_calibrations->codec_adc_calibration_dcoffset[1]=0;

	system_calibrations->tracking_comp[0]=1.025;
	system_calibrations->tracking_comp[1]=1.034;

	system_calibrations->led_brightness = 4;

	system_calibrations->pitch_pot_detent_offset[0]=30;
	system_calibrations->pitch_pot_detent_offset[1]=30;

	for (i=0;i<NUM_CV_ADCS;i++)
		system_calibrations->cv_calibration_offset[i] = -18;
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
	int32_t t;

	system_calibrations->cv_calibration_offset[0] = 2048-(i_smoothed_rawcvadc[0] & 0x0FFF);
	system_calibrations->cv_calibration_offset[1] = 2048-(i_smoothed_rawcvadc[1] & 0x0FFF);
	system_calibrations->cv_calibration_offset[2] = -1*(i_smoothed_rawcvadc[2] & 0x0FFF);
	system_calibrations->cv_calibration_offset[3] = -1*(i_smoothed_rawcvadc[3] & 0x0FFF);
	system_calibrations->cv_calibration_offset[4] = -1*(i_smoothed_rawcvadc[4] & 0x0FFF);
	system_calibrations->cv_calibration_offset[5] = -1*(i_smoothed_rawcvadc[5] & 0x0FFF);
	system_calibrations->cv_calibration_offset[6] = -1*(i_smoothed_rawcvadc[6] & 0x0FFF);
	system_calibrations->cv_calibration_offset[7] = -1*(i_smoothed_rawcvadc[7] & 0x0FFF);

	//Check if Pitch pot is not centered (either the knob is not set to the center,
	//or the detent calibration is incorrectly set
	//or the hardware is faulty
	//Use the PLAY light to show if we detect a non-centered calibrated value
	t = bracketed_potadc[PITCH_POT*2] + system_calibrations->pitch_pot_detent_offset[0];
	if (t > 2079 || t < 2010)		playbut1_color = 2; //red: out of pitch=1.0 range
	else if (t > 2069 || t < 2020)	playbut1_color = 4; //yellow: warning, close to edge
	else if (t > 2058 || t < 2038)	playbut1_color = 5; //green: ok: more than 10 from center
	else 							playbut1_color = 1; //white: within 10 of center 

	t = bracketed_potadc[PITCH_POT*2+1] + system_calibrations->pitch_pot_detent_offset[1];
	if (t > 2079 || t < 2010)		playbut2_color = 2; //red: out of pitch=1.0 range
	else if (t > 2069 || t < 2020)	playbut2_color = 4; //yellow: warning, close to edge
	else if (t > 2058 || t < 2038)	playbut2_color = 5; //green: ok: more than 10 from center
	else 							playbut2_color = 1; //white: within 10 of center 

	//Chan 1 Audio Out DC offset
	if (REV1BUT)
			system_calibrations->codec_dac_calibration_dcoffset[0]=(i_smoothed_potadc[LENGTH_POT*2]-2048-1750);

	//Chan 2 Audio Out DC offset
	if (REV2BUT)
			system_calibrations->codec_dac_calibration_dcoffset[1]=(i_smoothed_potadc[LENGTH_POT*2+1]-2048-1750);

 	//Chan 1 Pitch pot center position
 	if (PLAY1BUT)
			system_calibrations->pitch_pot_detent_offset[0]=(2048 - i_smoothed_potadc[PITCH_POT*2]);

	//Chan 2 Pitch pot center position
	if (PLAY2BUT)
			system_calibrations->pitch_pot_detent_offset[1]=(2048 - i_smoothed_potadc[PITCH_POT*2+1]);

	//Audio input dc offset (hardware uses AC coupling, so this not necessary)
	// if (EDIT_BUTTON)
	// {
	// 	system_calibrations->codec_adc_calibration_dcoffset[0]= -1*rx_buffer[0];
	// 	system_calibrations->codec_adc_calibration_dcoffset[1]= -1*rx_buffer[2];
	// }

	//FixMe: make this controlable in system mode
	//In tracking calibration mode, user should input C0 (0.00V) into 1V/oct jack
	//When a voltage -0.5 < V < 0.5 is registered, a button lights up. Then the user taps that button
	//Then input C3 (2.00V) into 1V/oct jack
	//When a voltage 1.5 > V > 2.5 is registered, a button lights up. Then the user taps that button
	//
	//The procedure can then be repeated for the other channel
	//The difference between adc values can be used to determine the tracking_comp:
	//>>> Find x, where y = voltoct[adc_high * x] / voltoct[adc_low * x], y = 2.000 (...or 1.999< y <2.001)

	system_calibrations->tracking_comp[0]=1.025;
	system_calibrations->tracking_comp[1]=1.034;


	if (SAVE_CALIBRATE_BUTTONS)
	{
		save_calibration_buttons_down++;
		if (save_calibration_buttons_down==3000)
		{
			save_flash_params(10);
			global_mode[CALIBRATE] = 0;
			flags[SkipProcessButtons] = 2; //indicate we're ready to clear the flag, once all buttons are released

		}
	} else
		save_calibration_buttons_down=0;

}

#ifndef UINT32_MAX
#define UINT32_MAX 4294967295U
#endif


void update_calibrate_leds(void)
{
	// static uint32_t led_flasher=0;

	// led_flasher+=UINT32_MAX/1665;
	//if (led_flasher < UINT32_MAX/2)	{SIGNALLED_ON;BUSYLED_OFF;PLAYLED1_ON;PLAYLED2_OFF;}
	//else 							{SIGNALLED_OFF;BUSYLED_ON;PLAYLED1_OFF;PLAYLED2_ON;}
	PLAYLED2_OFF;
	PLAYLED1_OFF;
	SIGNALLED_OFF;
	BUSYLED_OFF;
}

void update_calibration_button_leds(void)
{

	set_ButtonLED_byPalette(Play1ButtonLED, playbut1_color);
	set_ButtonLED_byPalette(Play2ButtonLED, playbut2_color);

	set_ButtonLED_byPalette(RecBankButtonLED, 9);
	set_ButtonLED_byPalette(RecButtonLED, 9);
	set_ButtonLED_byPalette(Reverse1ButtonLED, 9);
	set_ButtonLED_byPalette(Bank1ButtonLED, 9);
	set_ButtonLED_byPalette(Bank2ButtonLED, 9);
	set_ButtonLED_byPalette(Reverse2ButtonLED, 9);

}

