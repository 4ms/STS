#include "hardware_test_util.h"
#include "hardware_test_gate_outs.h"
extern "C" {
#include "globals.h"
#include "dig_pins.h"
#include "leds.h"
#include "i2s.h"
#include "rgb_leds.h"
#include "res/LED_palette.h"
}

struct STSEndout1 : public IGateOutput {
	STSEndout1(uint32_t freq, float pulse_width, float initial_phase, uint32_t sample_rate)
		: IGateOutput(freq, pulse_width, initial_phase, sample_rate) {}

	virtual void gate_out(bool state) {
		if (state) 
			ENDOUT1_ON;
		else 
			ENDOUT1_OFF;
	}
};

struct STSEndout2 : public IGateOutput {
	STSEndout2(uint32_t freq, float pulse_width, float initial_phase, uint32_t sample_rate)
		: IGateOutput(freq, pulse_width, initial_phase, sample_rate) {}

	virtual void gate_out(bool state) {
		if (state) 
			ENDOUT2_ON;
		else 
			ENDOUT2_OFF;
	}
};

void test_gate_outs() {
	uint32_t kEmpiricalSampleRate = 2658;
	STSEndout1 endout1_out {80, 0.25, 0, kEmpiricalSampleRate};
	STSEndout2 endout2_out {120, 0.75, 0, kEmpiricalSampleRate};


	set_led(0, true);
	set_led(3, true);
	pause_until_button_released();

	while (!hardwaretest_continue_button()) {
		endout1_out.update();
		endout2_out.update();
	}

	pause_until_button_released();
}

