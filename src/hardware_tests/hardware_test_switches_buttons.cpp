#include "hardware_test_switches_buttons.h"
#include "hardware_test_util.h"

void test_buttons(void)
{
	using ERRT = STSButtonChecker::ErrorType;
	all_leds_off();

	STSButtonChecker checker;
	checker.reset();
	checker.set_min_steady_state_time(400);
	while (checker.check()) {
	}

	for (uint8_t i=0; i<kNumSTSButtons; i++) {
		auto err = checker.get_error(i);
		if (err != STSButtonChecker::ErrorType::None) {
			checker._set_error_indicator(i, err);
			flash_mainbut_until_pressed();
		}
	}
}


