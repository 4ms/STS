#include "hardware_test_switches_buttons.h"
#include "hardware_test_util.h"

void test_buttons(void)
{
	all_leds_off();

	STSButtonChecker checker;
	checker.reset();

	//values assume 420ns per check
	checker.set_min_steady_state_time(1200);
	checker.set_allowable_noise(800);

	bool is_running = true;
	while (is_running) {
		is_running = checker.check();
	}

}


