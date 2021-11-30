#include "AdcRangeChecker.h"
#include "CodecCallbacks.h"
#include "hardware_test_adc.h"
#include "hardware_test_util.h"
extern "C" {
#include "dig_pins.h"
#include "leds.h"
#include "globals.h"
#include "adc.h"
#include "i2s.h"
}
static void show_multiple_nonzeros_error();
extern uint16_t potadc_buffer[NUM_POT_ADCS];
extern uint16_t cvadc_buffer[NUM_CV_ADCS];

static CenterFlatRamp flatRampWaveBiPolar;
static CenterFlatRamp flatRampWaveUniPolar;

using STSCodecCB = CodecCallbacks<int16_t, HT16_CHAN_BUFF_LEN>;

void send_LFOs_to_audio_outs() {
	flatRampWaveBiPolar.init(2.f, 0.2f, five_volts, neg_five_volts, 0.f, SAMPLERATE);
	flatRampWaveUniPolar.init(2.f, 0.2f, five_volts, zero_volts, 0.f, SAMPLERATE);

	STSCodecCB::leftStream = &flatRampWaveBiPolar;
	STSCodecCB::rightStream = &flatRampWaveUniPolar;

	set_codec_callback(STSCodecCB::testWavesOut);
}

const uint8_t adc_map[NUM_POT_ADCS+NUM_CV_ADCS] = {
	PITCH1_POT,
	SAMPLE1_POT,
	SAMPLE2_POT,
	PITCH2_POT,
	START1_POT,
	LENGTH1_POT,
	RECSAMPLE_POT,
	LENGTH2_POT,
	START2_POT,

	PITCH1_CV,
	PITCH2_CV,
	LENGTH1_CV,
	START1_CV,
	SAMPLE1_CV,
	SAMPLE2_CV,
	START2_CV,
	LENGTH2_CV,
};

void setup_adc() {
	Deinit_Pot_ADC();
	Deinit_CV_ADC();

	Init_Pot_ADC((uint16_t *)potadc_buffer, NUM_POT_ADCS);
	Init_CV_ADC((uint16_t *)cvadc_buffer, NUM_CV_ADCS);
}

bool check_max_one_cv_is_nonzero(uint16_t width) {
	uint8_t num_nonzero = 0;

	if (cvadc_buffer[LENGTH1_CV] > width)
		num_nonzero++;
	if (cvadc_buffer[LENGTH2_CV] > width)
		num_nonzero++;
	if (cvadc_buffer[START1_CV] > width)
		num_nonzero++;
	if (cvadc_buffer[START2_CV] > width)
		num_nonzero++;
	if (cvadc_buffer[SAMPLE1_CV] > width)
		num_nonzero++;
	if (cvadc_buffer[SAMPLE2_CV] > width)
		num_nonzero++;
	if (cvadc_buffer[PITCH1_CV] < (2048-width/2))
		num_nonzero++;
	if (cvadc_buffer[PITCH2_CV] < (2048-width/2))
		num_nonzero++;
	if (cvadc_buffer[PITCH1_CV] > (2048+width/2))
		num_nonzero++;
	if (cvadc_buffer[PITCH2_CV] > (2048+width/2))
		num_nonzero++;

	return (num_nonzero <= 1);
}

void test_pots_and_CV() {
	send_LFOs_to_audio_outs();

	struct AdcRangeCheckerBounds bounds = {
		.center_val = 2048,
		.center_width = 200,
		.center_check_counts = 50000,
		.min_val = 35, //about 28mV
		.max_val = 4080, //about 3288mV
	};
	AdcRangeChecker checker {bounds};

	set_led(2, false);

	set_led(0, true);
	set_led(1, true);
	set_led(3, true);

	setup_adc();

	for (uint32_t adc_i=0; adc_i<NUM_POT_ADCS+NUM_CV_ADCS; adc_i++) {
		pause_until_button_released();
		delay_ms(100);

		set_led(0, true);
		set_led(1, true);
		set_led(3, true);

		bool done = false;
		bool zeroes_ok = true;
		uint8_t cur_adc = adc_map[adc_i];

		checker.reset();

		while (!done) {
			uint16_t adcval = (adc_i<NUM_POT_ADCS) ? potadc_buffer[cur_adc]: cvadc_buffer[cur_adc]; //virtual get_val()
			checker.set_adcval(adcval);

			if (adc_i>=NUM_POT_ADCS) {
				zeroes_ok = check_max_one_cv_is_nonzero(300); //use bounds.width
				if (!zeroes_ok) {
					show_multiple_nonzeros_error();
					checker.reset();
					set_led(0, true);
					set_led(1, true);
					set_led(3, true);
				}
			}

			auto status = checker.check();
			//virtual display_status(chan, ADCCheckStatus)
			if (status==ADCCHECK_AT_MIN){
				set_led(0, false);
			}
			else if (status==ADCCHECK_AT_MAX){
				set_led(3, false);
			}
			else if (status==ADCCHECK_AT_CENTER){
				set_led(1, false);
			}
			else if (status==ADCCHECK_ELSEWHERE){
				set_led(1, true);
			}
			else if (status==ADCCHECK_FULLY_COVERED){
				done = true;
			}

			if (hardwaretest_continue_button())
				done = true;
		}

		//virtual handle_channel_done()
		set_led(2, true);
		delay_ms(50);
		set_led(2, false);
	}
}

static void show_multiple_nonzeros_error() {
	blink_all_lights(200);
	blink_all_lights(200);
	blink_all_lights(200);
	blink_all_lights(200);
	blink_all_lights(200);
	flash_mainbut_until_pressed();
	delay_ms(150);
}

