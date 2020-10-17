#include "hardware_test_audio.h"
#include "hardware_test_util.h"
#include "CodecCallbacks.h"
extern "C" {
#include "dig_pins.h"
#include "leds.h"
#include "globals.h"
#include "codec.h"
#include "i2s.h"
}

static void setup_outs_as_LFOs(void);

//FlagStatus codeca_timeout=0, codeca_ackfail=0, codeca_buserr=0;
//FlagStatus codecb_timeout=0, codecb_ackfail=0, codecb_buserr=0;

////Todo: enable these to help diagnose
//void I2C1_ER_IRQHandler(void)
//{
//	codeca_timeout = I2C_GetFlagStatus(CODECA_I2C, I2C_FLAG_TIMEOUT);
//	codeca_ackfail = I2C_GetFlagStatus(CODECA_I2C, I2C_FLAG_AF);
//	codeca_buserr = I2C_GetFlagStatus(CODECA_I2C, I2C_FLAG_BERR);
//}

//void I2C2_ER_IRQHandler(void)
//{
//	codecb_timeout = I2C_GetFlagStatus(CODECB_I2C, I2C_FLAG_TIMEOUT);
//	codecb_ackfail = I2C_GetFlagStatus(CODECB_I2C, I2C_FLAG_AF);
//	codecb_buserr = I2C_GetFlagStatus(CODECB_I2C, I2C_FLAG_BERR);
//}


void test_codec_init(void) {
	all_leds_off();
	pause_until_button_released();

	Codec_GPIO_Init();

	PLAYLED1_ON;
	Codec_AudioInterface_Init(SAMPLERATE);
	PLAYLED1_OFF;

	PLAYLED1_ON;
	PLAYLED2_ON;
	init_audio_dma();
	PLAYLED1_OFF;
	PLAYLED2_OFF;

	uint32_t err;
	BUSYLED_ON;
	err = Codec_Register_Setup(0);
	if (err) {
		const uint32_t bailout_time=50; //5 seconds
		uint32_t ctr = bailout_time;
		while (ctr) {
			PLAYLED2_ON;
			delay_ms(50);
			PLAYLED2_OFF;
			delay_ms(50);
			if (hardwaretest_continue_button())
				ctr--;
			else 
				ctr = bailout_time;
		}
	}
	BUSYLED_OFF;
}

static SkewedTriOsc testWaves[2];

//Test Audio Outs
//"Outer" button lights turn on (Play1/2)
//Each output jack outputs a left-leaning triangle, rising in freq as you go left->right
//Out A should have two tones mixed together, then patching a dummy cable into Out B should mute the higher tone
//
using STSCodecCB = CodecCallbacks<int16_t, HT16_CHAN_BUFF_LEN>;

void test_audio_out(void) {
	PLAYLED1_ON;
	PLAYLED2_ON;

	float max = ((1<<15)-1);
	float min = -max - 1.0f;

	testWaves[0].init(100, 0.2, max, min, 0, SAMPLERATE);
	testWaves[1].init(150, 0.2, max, min, 0, SAMPLERATE);

	STSCodecCB::leftStream = &testWaves[0];
	STSCodecCB::rightStream = &testWaves[1];
	set_codec_callback(STSCodecCB::testWavesOut);

	Start_I2SDMA();

	pause_until_button();

	PLAYLED1_OFF;
	PLAYLED2_OFF;
}

//Test Audio Ins
//"Inner" button lights turn on (Hold A/B)
//Each output jack outputs the same tones as in the last test, but at 1/2 the volume
//Patch a cable from any output jack to an input jack, and the two tones will mix
//Note: patching Out B to In A will produce a feedback loop unless a dummy cable is patched into In B
//Recommended to just avoid patching Out B->In A.
//Note: You can hear three tones by patching Send A or B-> In B while listening to Out A

void test_audio_in(void) {
	SIGNALLED_ON;
	BUSYLED_ON;

	float max = ((1<<15)-1) / 2.0f;
	float min = -max - 1.0f;

	testWaves[0].init(200, 0.2, max, min, 0, SAMPLERATE);
	testWaves[1].init(300, 0.2, max, min, 0, SAMPLERATE);

	set_codec_callback(STSCodecCB::passthruPlusTestWave);

	pause_until_button_pressed();
	delay_ms(80);

	SIGNALLED_OFF;
	BUSYLED_OFF;
}

