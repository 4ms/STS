/*
 * hardware_tests.c - test hardware with a scripted procedure
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

extern "C" {
#include "dig_pins.h"
//#include "RAM_test.h"
#include "audio_sdram.h"
#include "flash.h"
#include "globals.h"
}
#include "hardware_test_adc.h"
#include "hardware_test_audio.h"
#include "hardware_test_gates.h"
#include "hardware_test_switches_buttons.h"
#include "hardware_test_util.h"
#include "hardware_test_leds.h"
#include "hardware_test_sdcard.h"
#include "hardware_tests.h"


uint16_t _abs(int16_t val)
{
	return (val < 0) ? -val : val;
}

static void test_RAM(void);

static void test_input_jacks(void);
static void animate_success(void);

uint8_t do_hardware_test(void)
{
	pause_until_button_released();

	test_single_leds();
	test_rgb_leds();
	test_codec_init();
	test_audio_out();
	test_audio_in();

	if (!check_for_longhold_button())
		test_RAM();

	test_buttons();

	test_pots_and_CV();

	test_gate_ins();
	test_gate_outs();

	if (!test_sdcard())
		return 0;

	pause_until_button_released();

	while (!hardwaretest_continue_button()) {
		animate_success();
	}

	pause_until_button_released();

	return 1;
}

void animate_success(void)
{
	chase_all_buttonLEDs(20);
}


void test_RAM(void)
{
	set_led(0, true);

	SDRAM_Init();

	uint32_t errs = RAM_test();

	if (errs > 0) {
		const uint32_t bailout_time = 100; // 5 seconds
		uint32_t ctr = bailout_time;
		while (ctr) {
			blink_all_lights(50);
			if (hardwaretest_continue_button())
				ctr--;
			else
				ctr = bailout_time;
		}
	}

	flash_mainbut_until_pressed();
}

