/*
 * params.h - Reads UI elements, conditions signals and sets parameter values
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

#pragma once

#include <stm32f4xx.h>

//
// Params are values that the user controls, often in real-time
//
// i_ChannelParams are integer values
// f_ChannelParams are float values
//

enum i_ChannelParams { BANK, SAMPLE, REV, LOOPING, NUM_I_PARAMS };

enum f_ChannelParams { PITCH, START, LENGTH, VOLUME, NUM_F_PARAMS };

//ChannelPots is just a shortcut to help us with params, and must match the order in adc.h
enum ChannelPots { PITCH_POT, START_POT, LENGTH_POT, SAMPLE_POT };
enum ChannelCVs { PITCH_CV, START_CV, LENGTH_CV, SAMPLE_CV };

//
// Channel Settings are integer values related to states of operation
//

enum ChannelSettings { GATETRIG_START, GATETRIG_REV, GATETRIG_ENDOUT, NUM_CHAN_SETTINGS };

enum GateTrigTypes { GATE_MODE, TRIG_MODE };

enum Stereo_Modes { MONO, STEREO_LEFT, STEREO_RIGHT, STEREO_AVERAGE };

//
//Global
//Global Modes represent global states of functionality
//
enum GlobalModes {
	CALIBRATE, //calibration at the factory
	SYSTEM_MODE,
	MONITOR_RECORDING,
	ENABLE_RECORDING,
	EDIT_MODE,
	STEREO_MODE,
	ASSIGN_MODE,
	REC_24BITS,
	AUTO_STOP_ON_SAMPLE_CHANGE,
	LENGTH_FULL_START_STOP,
	QUANTIZE_CH1,
	QUANTIZE_CH2,
	ALLOW_SPLIT_MONITORING,
	PERC_ENVELOPE,
	FADEUPDOWN_ENVELOPE,
	STARTUPBANK_CH1,
	STARTUPBANK_CH2,
	TRIG_DELAY,
	LED_COLOR_ADJUST, //led calibration

	VIDEO_DIM, //dim button LEDs for shooting videos

	NUM_GLOBAL_MODES
};

enum AutoStopModes { AutoStop_OFF = 0, AutoStop_ALWAYS = 1, AutoStop_LOOPING = 2 };

enum MonitorModes { MONITOR_OFF = 0b00, MONITOR_BOTH = 0b11, MONITOR_LEFT = 0b01, MONITOR_RIGHT = 0b10 };

// play_trig_delay / ONE_SECOND is the delay in sec from detecting a trigger to calling start_playing()
// This is required to let Sample CV settle (due to the hardware LPF).
//
// play_trig_latch_pitch_time is how long the PITCH CV is latched when a play trigger is received
// This allows for a CV/gate sequencer to settle, and the internal LPF to settle, before using the new CV value
// This reduces slew and "indecision" when a step is advanced on the sequencer

enum RecordingSampleRates { REC_44K = 44100, REC_48K = 48000, REC_88K = 88200, REC_96K = 96000 };

typedef struct GlobalParams {
	uint32_t play_trig_delay;
	uint32_t play_trig_latch_pitch_time;
	enum RecordingSampleRates record_sample_rate;
	float f_record_sample_rate; //pre-converted to float to speed up calculations
	float fade_up_rate;			// Non-percussion playback mode fade up increment
	float fade_down_rate;		// Non-percussion playback mode fade down increment
	uint32_t fade_time_ms;		// Raw value in ms, independant of codec sample rate

	uint32_t auto_inc_slot_num_after_rec_trig;
} GlobalParams;

static inline float LowPassSmoothingFilter(float current_value, float new_value, float coef) {
	return (current_value * coef) + (new_value * (1.0f - coef));
}
float LowPassSmoothingFilter(float current_value, float new_value, float coef);

uint32_t apply_tracking_compensation(int32_t cv_adcval, float cal_amt);

void update_params(void);
void process_cv_adc(void);
void process_pot_adc(void);

void process_mode_flags(void);

void init_LowPassCoefs(void);
void init_params(void);
void init_modes(void);

uint8_t detent_num(uint16_t adc_val);
uint8_t detent_num_antihys(uint16_t adc_val, uint8_t cur_detent);

uint32_t calc_trig_delay(uint8_t trig_delay_setting);
uint32_t calc_pitch_latch_time(uint8_t trig_delay_setting);
float calc_fade_updown_rate(uint8_t time_ms);

#define adc_param_update_IRQHandler TIM1_BRK_TIM9_IRQHandler
void adc_param_update_IRQHandler(void);
