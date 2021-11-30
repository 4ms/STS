#include "hardware_test_util.h"
extern "C" {
#include "LED_palette.h"
#include "rgb_leds.h"
#include "dig_pins.h"
#include "globals.h"
#include "pca9685_driver.h"
}

uint8_t hardwaretest_continue_button(void) {
	return PLAY1BUT;
}

void pause_until_button_pressed(void) {
	delay_ms(10);
	while (!hardwaretest_continue_button()) {;}
}

void pause_until_button_released(void) {
	delay_ms(10);
	while (hardwaretest_continue_button()) {;}
}

void pause_until_button(void) {
	pause_until_button_pressed();
	pause_until_button_released();
}

void flash_mainbut_until_pressed(void) {
	while (1) {
		set_ButtonLED_byPalette(Play1ButtonLED, OFF);
		display_all_ButtonLEDs();

		delay_ms(50);
		if (hardwaretest_continue_button()) break;
		delay_ms(50);
		if (hardwaretest_continue_button()) break;

		set_ButtonLED_byPalette(Play1ButtonLED, GREEN);
		display_all_ButtonLEDs();
		delay_ms(50);
		if (hardwaretest_continue_button()) break;
		delay_ms(50);
		if (hardwaretest_continue_button()) break;
	}
	pause_until_button_released();
	set_ButtonLED_byPalette(Play1ButtonLED, OFF);
}

bool check_for_longhold_button(void) {
	uint32_t press_tmr = 0;
	bool longhold_detected = false;
	PLAYLED1_OFF;
	while (hardwaretest_continue_button()) {
		if (++press_tmr > 20000000) {
			longhold_detected = true;
			PLAYLED1_ON;
		}
	}
	return longhold_detected;
}

bool read_button_state(uint8_t button_num) {
	if (button_num==0) return PLAY1BUT ? 1 : 0;
	if (button_num==1) return BANK1BUT ? 1 : 0;
	if (button_num==2) return BANK2BUT ? 1 : 0;
	if (button_num==3) return PLAY2BUT ? 1 : 0;
	if (button_num==4) return REV1BUT ? 1 : 0;
	if (button_num==4) return RECBUT ? 1 : 0;
	if (button_num==4) return BANKRECBUT ? 1 : 0;
	if (button_num==4) return REV2BUT ? 1 : 0;
	else return 0;
}

void set_led(uint8_t led_num, bool turn_on) {
	if (turn_on) {
		if (led_num==0)
			PLAYLED1_ON;
		else if (led_num==1)
			SIGNALLED_ON;
		else if (led_num==2)
			BUSYLED_ON;
		else if (led_num==3)
			PLAYLED2_ON;
	} 
	else {
		if (led_num==0)
			PLAYLED1_OFF;
		else if (led_num==1)
			SIGNALLED_OFF;
		else if (led_num==2)
			BUSYLED_OFF;
		else if (led_num==3)
			PLAYLED2_OFF;
	}
}

void all_leds_off() {
	PLAYLED1_OFF;
	SIGNALLED_OFF;
	BUSYLED_OFF;
	PLAYLED2_OFF;
}

void all_leds_on() {
	PLAYLED1_ON;
	SIGNALLED_ON;
	BUSYLED_ON;
	PLAYLED2_ON;
}

