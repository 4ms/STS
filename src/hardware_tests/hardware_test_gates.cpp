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

	using STSCodecCB = CodecCallbacks<int16_t, HT16_CHAN_BUFF_LEN>;

public:
	STSGateInChecker()
		: IGateInChecker(kNumGateIns)
	{
		STSCodecCB::leftStream = &manual_codec_cb;
		STSCodecCB::rightStream = &manual_codec_cb;

		set_codec_callback(STSCodecCB::testWavesOut);
		set_num_toggles(kNumRepeats);

		cont_button_state = 0;
	}


private:
	static inline ManualValue manual_codec_cb;
	int cont_button_state;
	int blink_ctr;

protected:
	virtual void set_test_signal(bool state) {
		manual_codec_cb.set_val(state ? 31000.0f : 0.0f);
		delay_ms(2); //allow for latency of DAC output
	}

	virtual bool read_gate(uint8_t gate_num) {
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

	virtual void set_indicator(uint8_t indicator_num, bool newstate) {
		if (indicator_num==0)
			set_led(0, newstate);
		else if (indicator_num==1)
			set_ButtonLED_byPalette(Reverse1ButtonLED, newstate ? WHITE : OFF);
		else if (indicator_num==2)
			set_ButtonLED_byPalette(RecButtonLED, newstate ? WHITE : OFF);
		else if (indicator_num==3)
			set_ButtonLED_byPalette(Reverse2ButtonLED, newstate ? WHITE : OFF);
		else if (indicator_num==4)
			set_led(3, newstate);

		display_all_ButtonLEDs();
	}

	virtual void signal_jack_done(uint8_t chan) {
		cont_button_state = 0;
		set_ButtonLED_byPalette(Play1ButtonLED, YELLOW);
		display_all_ButtonLEDs();
	}

	virtual bool is_ready_to_read_jack(uint8_t chan) {
		if (hardwaretest_continue_button()) {
			cont_button_state = 1;
			return false;
		}
		else {
			if (cont_button_state == 1) {
				set_ButtonLED_byPalette(Play1ButtonLED, OFF);
				display_all_ButtonLEDs();
				return true;
			}
			else {
				set_ButtonLED_byPalette(Play1ButtonLED, ((blink_ctr++ & 0xFFFF) < 0x8000) ? YELLOW : OFF);
				display_all_ButtonLEDs();

				return false;
			}
		}
	}

	virtual void set_error_indicator(uint8_t channel, ErrorType err) {
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
				set_ButtonLED_byPalette(Bank2ButtonLED, RED);
				display_all_ButtonLEDs();
				flash_mainbut_until_pressed();
				delay_ms(150);
				set_ButtonLED_byPalette(Bank2ButtonLED, OFF);
				display_all_ButtonLEDs();
				clear_error();
				break;

			case ErrorType::StuckLow:
				set_ButtonLED_byPalette(Bank1ButtonLED, RED);
				display_all_ButtonLEDs();
				flash_mainbut_until_pressed();
				delay_ms(150);
				set_ButtonLED_byPalette(Bank1ButtonLED, OFF);
				display_all_ButtonLEDs();
				clear_error();
				break;
		}
	}
};

void test_gate_ins() {
	STSGateInChecker checker;

	checker.set_num_toggles(50);
	checker.reset();

	while (checker.check()) {;}

	set_ButtonLED_byPalette(Play1ButtonLED, OFF);
	display_all_ButtonLEDs();

	all_leds_off();
	delay_ms(150);
}


