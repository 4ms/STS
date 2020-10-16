#include "hardware_test_switches_buttons.h"
#include "hardware_test_util.h"
extern "C" {
#include "pca9685_driver.h"
}

void test_buttons(void)
{
	init_dig_inouts();
	LEDDriver_Init(2);
	LEDDRIVER_OUTPUTENABLE_ON;
	init_buttonLEDs();

	using ERRT = STSButtonChecker::ErrorType;
	all_leds_off();

	STSButtonChecker checker;
	checker.reset();

	//values assume 420ns per check
	checker.set_min_steady_state_time(1000);
	checker.set_allowable_noise(500);

	bool is_running = true;
	while (is_running) {
		is_running = checker.check();
	}

	// for (uint8_t i=0; i<kNumSTSButtons; i++) {
	// 	auto err = checker.get_error(i);
	// 	if (err != STSButtonChecker::ErrorType::None) {
	// 		checker._set_error_indicator(i, err);
	// 		flash_mainbut_until_pressed();
	// 	}
	// }
}


