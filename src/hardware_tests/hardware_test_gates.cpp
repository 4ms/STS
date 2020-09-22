#include "GateInChecker.h"
#include "CodecCallbacks.h"
#include "hardware_test_gates.h"
#include "hardware_test_util.h"
extern "C" {
#include "globals.h"
#include "dig_pins.h"
#include "leds.h"
#include "i2s.h"
#include "rgb_leds.h"
#include "res/LED_palette.h"
}

const uint8_t kNumGateIns = 5;
const unsigned kNumRepeats = 100;
const uint16_t BlockSz = HT16_CHAN_BUFF_LEN;

class STSGateInChecker : public IGateInChecker {
public:
	STSGateInChecker() 
		: IGateInChecker(kNumGateIns)
	{
		gate_out_state = 0;
		set_codec_callback(manual_control_audio_outs_cb);
		set_num_toggles(kNumRepeats);
	}

private:
	static inline uint16_t gate_out_state;
	static void manual_control_audio_outs_cb(int16_t *src, int16_t *dst) {
		for (uint16_t i=0; i<BlockSz/2; i++) {
			*dst++ = gate_out_state;
			*dst++ = 0;
			*dst++ = 0;
			*dst++ = 0;
			(void)(*src);//unused
		}
	}

protected:
	virtual void _set_test_signal(bool state) {
		gate_out_state = state ? 31000 : 0;
		if (state) set_led(0, true);
		else set_led(0, false);
		delay_ms(1); //allow for latency of DAC output
	}

	virtual bool _read_gate(uint8_t gate_num) {
		if (gate_num==0) 
			return (PLAY1JACK!=0);
		else if (gate_num==1)
			return (REV1JACK!=0);
		else if (gate_num==2)
			return (RECTRIGJACK!=0);
		else if (gate_num==3)
			return (REV2JACK!=0);
		else if (gate_num==4)
			return (PLAY2JACK!=0);
		else
			return false;
	}

	virtual void _set_indicator(uint8_t indicator_num, bool newstate) {
		if (indicator_num==0) 
			set_led(0, newstate);
		else if (indicator_num==1)
			set_ButtonLED_byPalette(Reverse1ButtonLED, newstate ? RED : OFF);
		else if (indicator_num==2)
			set_ButtonLED_byPalette(Reverse2ButtonLED, newstate ? RED : OFF);
		else if (indicator_num==3)
			set_led(1, newstate);
		else if (indicator_num==4)
			set_led(3, newstate);
	}

	virtual void _set_error_indicator(uint8_t channel, ErrorType err) {
		switch (err) {
			case ErrorType::None:
				all_leds_off();
				break;

			case ErrorType::MultipleHighs:
				blink_all_lights(100);
				blink_all_lights(100);
				blink_all_lights(100);
				blink_all_lights(100);
				blink_all_lights(100);
				all_leds_off();
				set_led(1, true);
				set_led(2, true);
				flash_mainbut_until_pressed();
				delay_ms(150);
				break;

			case ErrorType::StuckHigh:
				set_led(3, true);
				//flash_ping_until_pressed();
				//delay_ms(150);
				break;

			case ErrorType::StuckLow:
				set_led(0, true);
				// flash_ping_until_pressed();
				// delay_ms(150);
				break;
		}
	}
};

void test_gate_ins() {
	STSGateInChecker checker;

	checker.reset();
	while (checker.check()) {;}

	if (checker.get_error() != STSGateInChecker::ErrorType::None) {
		while (!hardwaretest_continue_button()) {
			blink_all_lights(400);
			blink_all_lights(400);
			blink_all_lights(400);
			blink_all_lights(400);
			flash_mainbut_until_pressed();
			delay_ms(150);
		}
	}

	all_leds_off();
	delay_ms(150);
}


