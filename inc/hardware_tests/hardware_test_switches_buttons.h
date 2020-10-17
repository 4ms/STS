#pragma once
#include "ButtonChecker.h"
extern "C" {
#include "dig_pins.h"
#include "globals.h"
#include "leds.h"
#include "pca9685_driver.h"
#include "res/LED_palette.h"
#include "rgb_leds.h"
}

const uint8_t kNumSTSButtons = 9;

class STSButtonChecker : public IButtonChecker {
public:
	STSButtonChecker()
		: IButtonChecker(kNumSTSButtons)
	{
		LEDDriver_Init(2);
		LEDDRIVER_OUTPUTENABLE_ON;
		init_buttonLEDs();
	}

	~STSButtonChecker() {}

	virtual bool _read_button(uint8_t button_num)
	{
		if (button_num == 0)
			return PLAY1BUT ? true : false;
		if (button_num == 1)
			return BANK1BUT ? true : false;
		if (button_num == 2)
			return BANK2BUT ? true : false;
		if (button_num == 3)
			return PLAY2BUT ? true : false;
		if (button_num == 4)
			return REV1BUT ? true : false;
		if (button_num == 5)
			return RECBUT ? true : false;
		if (button_num == 6)
			return BANKRECBUT ? true : false;
		if (button_num == 7)
			return REV2BUT ? true : false;
		if (button_num == 8)
			return EDIT_BUTTON ? true : false;
		else
			return false;
	}

	virtual void _set_error_indicator(uint8_t channel, ErrorType err)
	{
		PLAYLED1_OFF;
		PLAYLED2_OFF;
		if (err == ErrorType::NoisyPress)
			PLAYLED1_ON;
		if (err == ErrorType::NoisyRelease)
			PLAYLED2_ON;
		if (err != ErrorType::None) {
			uint8_t flashes = 10;
			while (flashes--) {
				delay_ms(50);
				BUSYLED_ON;
				_set_indicator(channel, false);
				delay_ms(50);
				BUSYLED_OFF;
				_set_indicator(channel, true);
			}
		}
		PLAYLED1_OFF;
		PLAYLED2_OFF;
	}

	virtual void _set_indicator(uint8_t indicate_num, bool turn_on)
	{
		if (turn_on) {
			if (indicate_num == 0)
				set_ButtonLED_byPalette(Play1ButtonLED, WHITE);
			else if (indicate_num == 1)
				set_ButtonLED_byPalette(Bank1ButtonLED, WHITE);
			else if (indicate_num == 2)
				set_ButtonLED_byPalette(Bank2ButtonLED, WHITE);
			else if (indicate_num == 3)
				set_ButtonLED_byPalette(Play2ButtonLED, WHITE);
			else if (indicate_num == 4)
				set_ButtonLED_byPalette(Reverse1ButtonLED, WHITE);
			else if (indicate_num == 5)
				set_ButtonLED_byPalette(RecButtonLED, WHITE);
			else if (indicate_num == 6)
				set_ButtonLED_byPalette(RecBankButtonLED, WHITE);
			else if (indicate_num == 7)
				set_ButtonLED_byPalette(Reverse2ButtonLED, WHITE);
			else if (indicate_num == 8) {
				set_ButtonLED_byPalette(Bank1ButtonLED, WHITE);
				set_ButtonLED_byPalette(Bank2ButtonLED, WHITE);
			}
		} else {
			if (indicate_num == 0)
				set_ButtonLED_byPalette(Play1ButtonLED, OFF);
			else if (indicate_num == 1)
				set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
			else if (indicate_num == 2)
				set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
			else if (indicate_num == 3)
				set_ButtonLED_byPalette(Play2ButtonLED, OFF);
			else if (indicate_num == 4)
				set_ButtonLED_byPalette(Reverse1ButtonLED, OFF);
			else if (indicate_num == 5)
				set_ButtonLED_byPalette(RecButtonLED, OFF);
			else if (indicate_num == 6)
				set_ButtonLED_byPalette(RecBankButtonLED, OFF);
			else if (indicate_num == 7)
				set_ButtonLED_byPalette(Reverse2ButtonLED, OFF);
			else if (indicate_num == 8) {
				set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
				set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
			}
		}
		display_all_ButtonLEDs();
	}

	virtual void _check_max_one_pin_changed() {}
};

void test_buttons(void);

