#include "LEDTester.h"
#include "hardware_test_leds.h"
#include "hardware_test_util.h"
extern "C" {
#include "res/LED_palette.h"
#include "dig_pins.h"
#include "pca9685_driver.h"
#include "leds.h"
#include "rgb_leds.h"
#include "globals.h"
}

class STSLEDTester : public ILEDTester {
	virtual void set_led(int led_id, bool turn_on) override
	{
		::set_led(led_id, turn_on);
	}

	virtual void pause_between_steps() override
	{
		pause_until_button();
	}

public:
	STSLEDTester(uint8_t num_leds)
		: ILEDTester(num_leds)
	{}
};

// The continue button is pressed to illuminate each LED, one at a time
// At the end, all leds turn on
void test_single_leds(void)
{

	const uint8_t numLEDs = 4;
	STSLEDTester check{numLEDs};
	check.run_test();
	pause_until_button();

	all_leds_on();
	pause_until_button();
	all_leds_off();
}

//RGB

class STSRGBLEDTester : public ILEDTester {
	virtual void set_led(int led_id, bool turn_on) override
	{
		int led_base = led_id / 3;
		int color = led_id - (led_base * 3);
		set_ButtonLED_byPalette(led_base, turn_on == false ? OFF : color == 0 ? RED : color == 1 ? GREEN : BLUE);
		display_all_ButtonLEDs();
	}

	virtual void pause_between_steps() override
	{
		pause_until_button();
	}

public:
	STSRGBLEDTester(uint8_t num_leds)
		: ILEDTester(num_leds)
	{}
};

void test_rgb_leds(void)
{
	set_led(0, false);
	set_led(1, true);
	set_led(2, true);
	set_led(3, true);

	LEDDriver_Init(2);
	LEDDRIVER_OUTPUTENABLE_ON;

	set_led(1, false);
	set_led(2, false);
	set_led(3, false);

	int colorseq[] = {WHITE, RED, BLUE, GREEN, CYAN, MAGENTA, YELLOW };
	for (auto c : colorseq) {
		for (int i = 0; i < NUM_RGBBUTTONS; i++)
			set_ButtonLED_byPalette(i, c);
		display_all_ButtonLEDs();
		pause_until_button();
	}
	all_buttonLEDs_off();

	flash_mainbut_until_pressed();

	if (EDIT_BUTTON) {
		fade_all_buttonLEDs();
		all_buttonLEDs_off();

		STSRGBLEDTester rgb_tester(NUM_RGBBUTTONS * 3);
		rgb_tester.run_test();

		all_leds_on();
		pause_until_button_pressed();
		pause_until_button_released();

		all_buttonLEDs_off();
	}

	all_leds_off();
}

