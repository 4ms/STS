/*
 * params.c
 *
 *  Created on: Mar 27, 2015
 *      Author: Dan Green danngreen1@gmail.com
 */

#include "globals.h"
#include "adc.h"
#include "dig_pins.h"
#include "params.h"
#include "buttons.h"
#include "sampler.h"
#include "wav_recording.h"
#include "rgb_leds.h"
#include "equal_pow_pan_padded.h"
#include "voltoct.h"
#include "log_taper_padded.h"
#include "pitch_pot_lut.h"
#include "edit_mode.h"
#include "calibration.h"
#include "bank.h"
#include "button_knob_combo.h"
#include "system_mode.h"


#define MAX_FIR_LPF_SIZE 80
uint32_t FIR_LPF_SIZE[NUM_CV_ADCS];


extern uint8_t PCB_version;

//CV LPFs
CCMDATA int32_t 	fir_lpf		[NUM_CV_ADCS][MAX_FIR_LPF_SIZE];
uint32_t 			fir_lpf_i	[NUM_CV_ADCS];


extern const float	pitch_pot_lut[4096];
extern const float 	voltoct[4096];


extern enum PlayStates play_state[NUM_PLAY_CHAN];

extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern SystemCalibrations *system_calibrations;

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

extern uint8_t disable_mode_changes;

volatile float	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
uint8_t 		i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
uint8_t 		settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];
uint8_t			global_mode[NUM_GLOBAL_MODES];
GlobalParams	global_params;

uint8_t 		flags[NUM_FLAGS]; //FixMe: is it OK to use 32-bit flags?
uint32_t 		flags32[NUM_FLAGS];


uint32_t play_trig_timestamp[2];

uint8_t pot_changed[NUM_POT_ADCS];

extern uint8_t recording_enabled;

extern enum ButtonStates button_state[NUM_BUTTONS];
extern uint8_t button_ignore[NUM_BUTTONS];

volatile uint32_t 			sys_tmr;

extern ButtonKnobCombo g_button_knob_combo[NUM_BUTTON_KNOB_COMBO_BUTTONS][NUM_BUTTON_KNOB_COMBO_KNOBS];

//jack/pot LPF coefficients:
float POT_LPF_COEF[NUM_POT_ADCS];
float RAWCV_LPF_COEF[NUM_CV_ADCS];

//jack/pot LPF bracketing amounts:
int32_t POT_BRACKET[NUM_POT_ADCS];
int32_t CV_BRACKET[NUM_CV_ADCS];

// Latched 1voct values
uint32_t voct_latch_value[2];

//Low Pass filtered adc values:
float smoothed_potadc[NUM_POT_ADCS];
float smoothed_cvadc[NUM_CV_ADCS];

//Integer-ized LowPassFiltered adc values:
int16_t i_smoothed_potadc[NUM_POT_ADCS];
int16_t i_smoothed_cvadc[NUM_CV_ADCS];

//LPF -> Bracketed adc values
int16_t bracketed_cvadc[NUM_CV_ADCS];
int16_t bracketed_potadc[NUM_POT_ADCS];

//Latched pot value for use during edit mode
extern int16_t editmode_latched_potadc[NUM_POT_ADCS];

//LPF of raw ADC values for calibration
int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

//Change in pot since last process_adc
int32_t pot_delta[NUM_POT_ADCS];
int32_t cv_delta[NUM_CV_ADCS];


void init_params(void)
{
	uint32_t chan,i;

	for (chan=0;chan<NUM_PLAY_CHAN;chan++){
		f_param[chan][PITCH] 	= 1.0;
		f_param[chan][START] 	= 0.0;
		f_param[chan][LENGTH] 	= 1.0;
		f_param[chan][VOLUME] 	= 1.0;

		i_param[chan][BANK] 	= 0;
		i_param[chan][SAMPLE] 	= 0;
		i_param[chan][REV] 		= 0;
		i_param[chan][LOOPING]	 =0;
	}

	i_param[REC][BANK] = 0;
	i_param[REC][SAMPLE] = 0;

	for (i=0;i<NUM_FLAGS;i++)
	{
		flags[i]=0;
	}

	global_params.play_trig_delay 				= calc_trig_delay(global_mode[TRIG_DELAY]);
	global_params.play_trig_latch_pitch_time 	= calc_pitch_latch_time(global_mode[TRIG_DELAY]);

}

//initializes modes that aren't read from flash ram or disk
void init_modes(void)
{
	//global_mode[CALIBRATE] = 0;
	global_mode[SYSTEM_MODE] = 0;
	global_mode[MONITOR_RECORDING] = MONITOR_OFF;
	global_mode[ENABLE_RECORDING] = 0;
	global_mode[EDIT_MODE] = 0;
	global_mode[ASSIGN_MODE] = 0;

	global_mode[ALLOW_SPLIT_MONITORING] = 1;
}


void init_LowPassCoefs(void)
{
	float t;
	uint8_t i;

	t=300.0;

	RAWCV_LPF_COEF[PITCH1_CV] = 1.0f-(1.0f/t);
	RAWCV_LPF_COEF[PITCH2_CV] = 1.0f-(1.0f/t);

	RAWCV_LPF_COEF[START1_CV] = 1.0f-(1.0f/t);
	RAWCV_LPF_COEF[START2_CV] = 1.0f-(1.0f/t);

	RAWCV_LPF_COEF[LENGTH1_CV] = 1.0f-(1.0f/t);
	RAWCV_LPF_COEF[LENGTH2_CV] = 1.0f-(1.0f/t);

	RAWCV_LPF_COEF[SAMPLE1_CV] = 1.0f-(1.0f/t);
	RAWCV_LPF_COEF[SAMPLE2_CV] = 1.0f-(1.0f/t);

	CV_BRACKET[PITCH1_CV] = 5;
	CV_BRACKET[PITCH2_CV] = 5;

	CV_BRACKET[START1_CV] = 20;
	CV_BRACKET[START2_CV] = 20;

	CV_BRACKET[LENGTH1_CV] = 20;
	CV_BRACKET[LENGTH2_CV] = 20;

	CV_BRACKET[SAMPLE1_CV] = 20;
	CV_BRACKET[SAMPLE2_CV] = 20;

	t=20.0; //50.0 = about 100ms to turn a knob fully

	POT_LPF_COEF[PITCH1_POT] = 1.0f-(1.0f/t);
	POT_LPF_COEF[PITCH2_POT] = 1.0f-(1.0f/t);

	t=50.0;

	POT_LPF_COEF[START1_POT] = 1.0f-(1.0f/t);
	POT_LPF_COEF[START2_POT] = 1.0f-(1.0f/t);

	POT_LPF_COEF[LENGTH1_POT] = 1.0f-(1.0f/t);
	POT_LPF_COEF[LENGTH2_POT] = 1.0f-(1.0f/t);


	POT_LPF_COEF[SAMPLE1_POT] = 1.0f-(1.0f/t);
	POT_LPF_COEF[SAMPLE2_POT] = 1.0f-(1.0f/t);

	POT_LPF_COEF[RECSAMPLE_POT] = 1.0f-(1.0f/t);



	POT_BRACKET[PITCH1_POT] = 12;
	POT_BRACKET[PITCH2_POT] = 12;

	POT_BRACKET[START1_POT] = 20;
	POT_BRACKET[START2_POT] = 20;

	POT_BRACKET[LENGTH1_POT] = 20;
	POT_BRACKET[LENGTH2_POT] = 20;

	POT_BRACKET[SAMPLE1_POT] = 60;
	POT_BRACKET[SAMPLE2_POT] = 60;

	POT_BRACKET[RECSAMPLE_POT] = 60;

	for (i=0;i<NUM_POT_ADCS;i++)
	{
		smoothed_potadc[i]		=0;
		bracketed_potadc[i]=0;
		i_smoothed_potadc[i]	=0x7FFF;
		pot_delta[i]			=0;
	}
	for (i=0;i<NUM_CV_ADCS;i++)
	{
		smoothed_cvadc[i]		=0;
		bracketed_cvadc[i]	=0;
		i_smoothed_cvadc[i]		=0x7FFF;
		i_smoothed_rawcvadc[i]	=0x7FFF;
		cv_delta[i]				=0;
	}

	//Set FIR LPF buffer sizes
	if (PCB_version == 0)
	{
		FIR_LPF_SIZE[PITCH1_CV] = 80;
		FIR_LPF_SIZE[PITCH2_CV] = 80;
	}
	else
	{
		FIR_LPF_SIZE[PITCH1_CV] = 40;
		FIR_LPF_SIZE[PITCH2_CV] = 40;
	}

	FIR_LPF_SIZE[START1_CV] = 20;
	FIR_LPF_SIZE[START2_CV] = 20;
	FIR_LPF_SIZE[LENGTH1_CV] = 20;
	FIR_LPF_SIZE[LENGTH2_CV] = 20;
	FIR_LPF_SIZE[SAMPLE1_CV] = 1;
	FIR_LPF_SIZE[SAMPLE2_CV] = 1;

	//initialize FIR LPF

	for (i=0;i<MAX_FIR_LPF_SIZE;i++)
	{
		//PITCH CVs default to value of 2048
		fir_lpf[0][i]=2048;
		fir_lpf[1][i]=2048;

		//Other CVs default to 0
		fir_lpf[2][i]=0;
		fir_lpf[3][i]=0;
		fir_lpf[4][i]=0;
		fir_lpf[5][i]=0;
		fir_lpf[6][i]=0;
		fir_lpf[7][i]=0;

	}
	for(i=0;i<NUM_CV_ADCS;i++)
	{
		fir_lpf_i[i]=0;
		smoothed_cvadc[i] = fir_lpf[i][0];
	}

}


void process_cv_adc(void)
{//takes about 7us to run, at -O3

	uint8_t i;
	int32_t old_val, new_val;
	int32_t t;

	//Do PITCH CVs first
	//This function assumes:
	//Channel 1's pitch cv = PITCH1_CV   = 0
	//Channel 2's pitch cv = PITCH1_CV+1 = 1
	for (i=0;i<NUM_CV_ADCS;i++)
	{
		//Apply Linear average LPF:

		//Pull out the oldest value (before overwriting it):
		old_val = fir_lpf[i][fir_lpf_i[i]];

		//Put in the new cv value (overwrite oldest value):
		//Don't factor in the calibration offset if we're in calibration mode, or else we'll have a runaway feedback loop
		if (global_mode[CALIBRATE])
			new_val = cvadc_buffer[i];
		else
			new_val = cvadc_buffer[i] + system_calibrations->cv_calibration_offset[i];

		fir_lpf[i][fir_lpf_i[i]] 	= new_val;

		//Increment the index, wrapping around the whole buffer
		if (++fir_lpf_i[i] >= FIR_LPF_SIZE[i]) fir_lpf_i[i]=0;

		//Calculate the arithmetic average (FIR LPF)
		smoothed_cvadc[i] = (float)((smoothed_cvadc[i] * FIR_LPF_SIZE[i]) - old_val + new_val) / (float)FIR_LPF_SIZE[i];

		//Range check and convert to integer
		i_smoothed_cvadc[i] = (int16_t)smoothed_cvadc[i];
		if (i_smoothed_cvadc[i] < 0) i_smoothed_cvadc[i] = 0;
		if (i_smoothed_cvadc[i] > 4095) i_smoothed_cvadc[i] = 4095;

		//FixMe: rawcvadc is not needed (but test cv calibration before removing it, we may need to use a different LPF on it)
		i_smoothed_rawcvadc[i] 	= i_smoothed_cvadc[i];

		//Apply bracketing
		t=i_smoothed_cvadc[i] - bracketed_cvadc[i];
		if (t>CV_BRACKET[i])
		{
			cv_delta[i] = t;
			bracketed_cvadc[i] = i_smoothed_cvadc[i] - (CV_BRACKET[i]);
		}

		else if (t<-CV_BRACKET[i])
		{
			cv_delta[i] = t;
			bracketed_cvadc[i] = i_smoothed_cvadc[i] + (CV_BRACKET[i]);
		}

		//Additional bracketing for special-case of CV jack being near 0V:
		if (i==0 || i==1) //PITCH CV
		{
			if (bracketed_cvadc[i] >= (2048-3) && bracketed_cvadc[i] <= (2048+3))
				bracketed_cvadc[i] = 2048;
		}

	}
}

void process_pot_adc(void)
{
	uint8_t i;
	int32_t t;

	static uint32_t track_moving_pot[NUM_POT_ADCS]={0,0,0,0,0,0,0,0,0};

	//
	// Run a LPF on the pots and CV jacks
	//
	for (i=0;i<NUM_POT_ADCS;i++)
	{
		pot_changed[i]=0;

		smoothed_potadc[i] = LowPassSmoothingFilter(smoothed_potadc[i], (float)potadc_buffer[i], POT_LPF_COEF[i]);
		i_smoothed_potadc[i] = (int16_t)smoothed_potadc[i];

		t=i_smoothed_potadc[i] - bracketed_potadc[i];
		if ((t>POT_BRACKET[i]) || (t<-POT_BRACKET[i]))
			track_moving_pot[i]=300;

		if (track_moving_pot[i])
		{
			track_moving_pot[i]--;
			pot_changed[i]=1;
			pot_delta[i] = t;
			bracketed_potadc[i] = i_smoothed_potadc[i];
		}
	}
}

//
// Applies tracking comp for a bi-polar adc
// Assumes 2048 is the center value
//
uint32_t apply_tracking_compensation(int32_t cv_adcval, float cal_amt)
{
	float float_val;

	if (cv_adcval > 2048)
	{
		cv_adcval -= 2048;
		float_val = (float)cv_adcval * cal_amt;

		//round float_val to nearest integer
		cv_adcval = (uint32_t)float_val;
		if ((float_val - cv_adcval) >= 0.5) cv_adcval++;

		cv_adcval += 2048;
	}
	else
	{
		cv_adcval = 2048 - cv_adcval;
		float_val = (float)cv_adcval * cal_amt;

		//round float_val to nearest integer
		cv_adcval = (uint32_t)float_val;
		if ((float_val - cv_adcval) >= 0.5) cv_adcval++;

		cv_adcval = 2048 - cv_adcval;
	}
	if (cv_adcval > 4095) cv_adcval = 4095;
	if (cv_adcval < 0) cv_adcval = 0;
	return(cv_adcval);
}



//
// Returns a semitone-quantized tuning amount, given an ADC value
//
#define TWELFTH_ROOT_TWO 1.059463094
#define SEMITONE_ADC_WIDTH 34.0
#define OCTAVE_ADC_WIDTH (SEMITONE_ADC_WIDTH*12.0)

float quantized_semitone_voct(uint32_t adcval)
{
	int8_t oct;
	int8_t semitone;
	int32_t root_adc;
	int32_t root_adc_midpt;

	uint8_t oct_mult;
	float semitone_mult;

	float octave_mult;

	//every 405 adc values ocs an octtave
	//2048-(405*oct) ---> 2^oct, oct: 0..5 ---> 1..32
	//2048+(405*oct) ---> 1/(2^oct), oct: 0..5 --> 1..1/32

	if (adcval==2048) return(1.0);

//	if (adcval<2048)
	if (0)
	{
		//First, set oct_mult
		//oct_mult should be 32, 16, 8, 4, 2, or 1 to indicate the root note of the octave we're in
		oct_mult=32; //start by testing the +5 octave
		for (oct=5;oct>=0;oct--)
		{
			root_adc = 2048-(OCTAVE_ADC_WIDTH*oct);
			if (adcval<=root_adc) break; //exit for loop
			oct_mult>>=1; //drop down an octave
		}

		root_adc_midpt = root_adc - (SEMITONE_ADC_WIDTH/2.0);
		semitone_mult=1.0;
		for (semitone=0;semitone<12;semitone++)
		{
			if (adcval>(root_adc_midpt-(int32_t)(SEMITONE_ADC_WIDTH*(float)semitone))) break; //exit for loop
			semitone_mult*=TWELFTH_ROOT_TWO;
		}

		return ((float)oct_mult * semitone_mult);
	}
	else
	{
		//First, set oct_mult
		//oct_mult should be 32, 16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 0.0625, 0.03125 to indicate the root note of the octave we're in
		octave_mult=32.0;
		for (oct=5;oct>=-5;oct--)
		{
			root_adc = 2048-(OCTAVE_ADC_WIDTH*oct);
			if (adcval<=root_adc) break; //exit for loop
			octave_mult = octave_mult/2; //drop down an octave
		}

		root_adc_midpt = root_adc - (SEMITONE_ADC_WIDTH/2.0);
		semitone_mult=1.0;
		for (semitone=0;semitone<12;semitone++)
		{
			if (adcval>(root_adc_midpt-(int32_t)(SEMITONE_ADC_WIDTH*(float)semitone))) break; //exit for loop
			semitone_mult*=TWELFTH_ROOT_TWO;
		}

		return (octave_mult * semitone_mult);
	}

}


void update_params(void)
{
	uint8_t chan;
	uint8_t knob;
	uint8_t old_val;
	uint8_t new_val;
	uint8_t t_trig_delay_val;
	int32_t t_pitch_potadc;
	float t_f;
	uint16_t sample_pot;
	int16_t pitch_cv;

	uint32_t trial_bank;
	uint8_t samplenum, banknum;
	uint32_t compensated_pitch_cv;

	ButtonKnobCombo *this_bank_bkc; //pointer to the currently active button/knob combo action
	ButtonKnobCombo *other_bank_bkc; //pointer to the other channel's button/knob combo action
	ButtonKnobCombo *edit_bkc; //pointer to Edit+knob combo actions
	ButtonKnobCombo *vol_bkc; //pointer to Rev+Start combo actions

	recording_enabled=1;


	//
	// Edit mode
	//
	if (global_mode[EDIT_MODE])
	{
		samplenum = i_param[0][SAMPLE];
		banknum = i_param[0][BANK];

		//
		// Trim Size
		//
		if (pot_changed[LENGTH2_POT])
		{
			nudge_trim_size(&samples[banknum][samplenum], pot_delta[LENGTH2_POT]);

			pot_changed[LENGTH2_POT] = 0;

			f_param[0][START] = 0.999f;
			f_param[0][LENGTH] = 0.201f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;
		}


		//
		// Trim Start
		//
		if (pot_changed[START2_POT])
		{
			nudge_trim_start(&samples[banknum][samplenum], pot_delta[START2_POT]);
			pot_changed[START2_POT] = 0;

			f_param[0][START] = 0.000f;
			f_param[0][LENGTH] = 0.201f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;
		}


		//
		// Gain (sample ch2 pot):
		// 0.1x when pot is at 0%
		// 1x when pot is at 50%
		// 5x when pot is at 100%
		//
		if (pot_changed[SAMPLE2_POT])
		{
			if (bracketed_potadc[SAMPLE2_POT] < 2020)
				t_f = (bracketed_potadc[SAMPLE2_POT] / 2244.44f) + 0.1; //0.1 to 1.0
			else if (bracketed_potadc[SAMPLE2_POT] < 2080)
				t_f = 1.0;
			else
				t_f	= (bracketed_potadc[SAMPLE2_POT] - 1577.5) / 503.5f; //1.0 to 5.0
			set_sample_gain(&samples[banknum][samplenum], t_f);
		}

	}//if  EDIT_MODE

	else
	{
		//Check if pots moved with Edit Mode off
		//Unlatch and inactivate the combo mode
		//
		if (pot_changed[START2_POT])
			g_button_knob_combo[bkc_Edit][bkc_StartPos2].combo_state = COMBO_INACTIVE;

		if (pot_changed[LENGTH2_POT])
			g_button_knob_combo[bkc_Edit][bkc_Length2].combo_state = COMBO_INACTIVE;

		if (pot_changed[SAMPLE2_POT])
			g_button_knob_combo[bkc_Edit][bkc_Sample2].combo_state = COMBO_INACTIVE;

		if (pot_changed[SAMPLE2_POT])
			g_button_knob_combo[bkc_Edit][bkc_RecSample].combo_state = COMBO_INACTIVE;

	}



	for (chan=0;chan<2;chan++)
	{
		//
		// LENGTH POT + CV
		//

		//In Edit Mode, don't update channel 1's START and LENGTH normally, they are controlled by the Trim settings
		if (!(global_mode[EDIT_MODE] && chan==CHAN1)) //"skip this block if we're in edit mode and this is channel 1"
		{
			edit_bkc 	= &g_button_knob_combo[bkc_Edit][bkc_Length2];
			t_f = f_param[chan][LENGTH];

			//Use the latched pot value for channel 2 when in Edit Mode
			if (edit_bkc->combo_state!=COMBO_INACTIVE && chan==CHAN2)
				f_param[chan][LENGTH] 	= (edit_bkc->latched_value + bracketed_cvadc[LENGTH2_CV]) / 4096.0;
			else
				f_param[chan][LENGTH] 	= (bracketed_potadc[LENGTH1_POT+chan] + bracketed_cvadc[LENGTH1_CV+chan]) / 4096.0;


			if (f_param[chan][LENGTH] > 0.990f)	f_param[chan][LENGTH] = 1.0;
			if (f_param[chan][LENGTH] <= 0.005f)	f_param[chan][LENGTH] = 0.005;

			//
			// START POT + CV
			//

			edit_bkc 	= &g_button_knob_combo[bkc_Edit][bkc_StartPos2];
			vol_bkc 	= &g_button_knob_combo[bkc_Reverse1+chan][bkc_StartPos1+chan];

			//Use the latched pot value for channel 2 when Edit combo is active
			if (edit_bkc->combo_state != COMBO_INACTIVE && chan==CHAN2)
				f_param[chan][START] 	= (edit_bkc->latched_value + bracketed_cvadc[START2_CV]) / 4096.0;
			//Use the latched pot value when Volume combo is active
			else if (vol_bkc->combo_state != COMBO_INACTIVE)
				f_param[chan][START] 	= (vol_bkc->latched_value + bracketed_cvadc[START1_CV+chan]) / 4096.0;
			else
				f_param[chan][START] 	= (bracketed_potadc[START1_POT+chan] + bracketed_cvadc[START1_CV+chan]) / 4096.0;

			if (f_param[chan][START] > 0.99f)		
				f_param[chan][START] = 1.0;
			if (f_param[chan][START] <= 0.0003f)		
				f_param[chan][START] = 0.0;
		}


		//
		// Rev1 + Length1: Envelope Mode on/off
		//
		// Toggle envelope modes if Rev1 is held down while Length1 is at an extreme
		//

		vol_bkc 	= &g_button_knob_combo[bkc_Reverse1][bkc_StartPos1];

		if ( (chan==CHAN1) && 															// Only need to check it for channel 1
			 (button_state[Rev1]>=MED_PRESSED) &&										// Rev1 button is held down for a while
			 all_buttons_except(UP, (1<<Rev1)) &&										// No other buttons are down
			 (button_ignore[Rev1]==0) && 												// Feature hasn't been activated since last Rev1 button up event
			 (vol_bkc->combo_state == COMBO_INACTIVE) )									// Ignore if we're doing a Volume button/knob combo
		{
			if (bracketed_potadc[LENGTH1_POT]<50)	{
				global_mode[PERC_ENVELOPE] = !global_mode[PERC_ENVELOPE];
				button_ignore[Rev1] = 1;
				flags[PercEnvModeChanged] = 100;
			}
			if (bracketed_potadc[LENGTH1_POT]>4000)	{
				global_mode[FADEUPDOWN_ENVELOPE] = !global_mode[FADEUPDOWN_ENVELOPE];
				button_ignore[Rev1] = 1;
				flags[FadeEnvModeChanged] = 100;
			}
		}


		//
		// PITCH POT + CV
		//

		t_pitch_potadc = bracketed_potadc[PITCH1_POT+chan] + system_calibrations->pitch_pot_detent_offset[chan];
		if (t_pitch_potadc > 4095) t_pitch_potadc = 4095;
		if (t_pitch_potadc < 0) t_pitch_potadc = 0;

		if(flags[LatchVoltOctCV1+chan]) 	pitch_cv = voct_latch_value[chan];
		else								pitch_cv = bracketed_cvadc[PITCH1_CV+chan];

		if (global_mode[QUANTIZE_CH1+chan])
		{
			compensated_pitch_cv = apply_tracking_compensation(pitch_cv, system_calibrations->tracking_comp[chan]);
			f_param[chan][PITCH] = pitch_pot_lut[t_pitch_potadc] * quantized_semitone_voct(compensated_pitch_cv);
		}
		else
		{
			compensated_pitch_cv = apply_tracking_compensation(pitch_cv, system_calibrations->tracking_comp[chan]);
			f_param[chan][PITCH] = pitch_pot_lut[t_pitch_potadc] * voltoct[compensated_pitch_cv];

		}

		if (f_param[chan][PITCH] > MAX_RS)
		    f_param[chan][PITCH] = MAX_RS;



		//
		// Change bank with Bank + Sample combo
		//
		// Holding Bank X's button while turning a Sample knob changes channel X's bank
		//
		if (button_state[Bank1 + chan] >= DOWN)
		{
			//Go through both the knobs: bkc_Sample1 and bkc_Sample2
			//
			knob = bkc_Sample1;
			while(knob==bkc_Sample1 || knob==bkc_Sample2)
			{
				this_bank_bkc 	= &g_button_knob_combo[chan==0? bkc_Bank1 : bkc_Bank2][knob==0? bkc_Sample1 : bkc_Sample2];
				other_bank_bkc = &g_button_knob_combo[chan==0? bkc_Bank1 : bkc_Bank2][knob==0? bkc_Sample2 : bkc_Sample1];

				new_val = detent_num(bracketed_potadc[SAMPLE1_POT + knob]);

				//If the combo is active, then use the Sample pot to update the Hover value
				//(which becomes the Bank param when we release the button)
				//
				if (this_bank_bkc->combo_state == COMBO_ACTIVE)
				{
					//Calcuate the (tentative) new bank, based on the new_val and the current hover_value
					//
					if (knob==bkc_Sample1) trial_bank = new_val + (get_bank_blink_digit(this_bank_bkc->hover_value) * 10);
					if (knob==bkc_Sample2) trial_bank = get_bank_color_digit(this_bank_bkc->hover_value) + (new_val*10);

					//If the new bank is enabled (contains samples) then update the hover_value
					//
					if (is_bank_enabled(trial_bank))
					{
						//Set both hover_values to the same value, since they are linked
						//
						if (this_bank_bkc->hover_value != trial_bank)
						{
							flags[PlayBankHover1Changed + chan] = 3;
							this_bank_bkc->hover_value = trial_bank;
							other_bank_bkc->hover_value = trial_bank;
						}
					}
				}

				//If the combo is not active,
				//Activate it when we detect the knob was turned to a new detent
				else
				{
					if (pot_changed[SAMPLE1_POT + knob])
					{
						this_bank_bkc->combo_state = COMBO_ACTIVE;

						//Initialize the hover value
						//Set both hover_values to the same value, since they are linked
						this_bank_bkc->hover_value = i_param[chan][BANK];
						other_bank_bkc->hover_value = i_param[chan][BANK];
					}
				}

				if (knob==bkc_Sample1) knob = bkc_Sample2;
				else break;
			}
		}

		//
		// Update combo Bank+Sample pot's latch status
		//
		//The combo to latch is the other channel's Bank button with this channel's Sample knob
		//because:
		//this Bank + this Sample changes this bank, so it's ok to not latch sample value
		//other Bank + other Sample changes the other bank, same reason as above
		//this Bank + other Sample will be handled in this for loop
		if (chan==CHAN1)
		{
			this_bank_bkc 	= &g_button_knob_combo[bkc_Bank2][bkc_Sample1];
			other_bank_bkc = &g_button_knob_combo[bkc_Bank1][bkc_Sample1];
		}
		else
		{
			this_bank_bkc 	= &g_button_knob_combo[bkc_Bank1][bkc_Sample2];
			other_bank_bkc = &g_button_knob_combo[bkc_Bank2][bkc_Sample2];
		}


		if (this_bank_bkc->combo_state == COMBO_LATCHED && pot_changed[SAMPLE1_POT+chan])
			this_bank_bkc->combo_state = COMBO_INACTIVE;

		if (other_bank_bkc->combo_state == COMBO_LATCHED && pot_changed[SAMPLE1_POT+chan])
			other_bank_bkc->combo_state = COMBO_INACTIVE;




		// Sample Pot+CV
		//
		// Sample knob: If we're not doing a button+knob combo, just change the sample
		//

		// Use the pot adc for the samplepot value if there's no combo active
		// Or else use the latched value if there's an active combo happening
		//
		if (this_bank_bkc->combo_state == COMBO_INACTIVE && other_bank_bkc->combo_state == COMBO_INACTIVE)
			sample_pot = bracketed_potadc[SAMPLE1_POT+chan];
		else
		{
			if (this_bank_bkc->combo_state != COMBO_INACTIVE)
				sample_pot = this_bank_bkc->latched_value;
			else
				sample_pot = other_bank_bkc->latched_value;
		}

		edit_bkc 	= &g_button_knob_combo[bkc_Edit][bkc_Sample2];

		old_val = i_param[chan][SAMPLE];

		//Use the latched pot value for channel 2 when latched from Edit Mode
		if (edit_bkc->combo_state != COMBO_INACTIVE && chan==CHAN2)
			new_val = detent_num_antihys( edit_bkc->latched_value + bracketed_cvadc[SAMPLE2_CV], old_val );
		else
			new_val = detent_num_antihys( sample_pot + bracketed_cvadc[SAMPLE1_CV+chan], old_val );

		if (old_val != new_val)
		{
			//The knob has moved to a new detent (without the a Bank button down)

			//Update the sample parameter
			i_param[chan][SAMPLE] = new_val;

			flags[PlaySample1Changed + chan] = 1;

			//Set a flag to initiate a bright flash on the Play button
			if (samples[ i_param[chan][BANK] ][ new_val ].filename[0] == 0) //not a valid sample
			{
				flags[PlaySample1Changed_empty + chan] = 6;
				flags[PlaySample1Changed_valid + chan] = 0;
			}
			else
			{
				flags[PlaySample1Changed_empty + chan] = 0;
				flags[PlaySample1Changed_valid + chan] = 6;
			}
		}


		//
		// Volume
		//
		// Rev + StartPos
		//

		if (chan==CHAN1)	vol_bkc = &g_button_knob_combo[bkc_Reverse1][bkc_StartPos1];
		else				vol_bkc = &g_button_knob_combo[bkc_Reverse2][bkc_StartPos2];

		// Activate the combo alt feature if the StartPos pot moves while the Rev button is down
		if (button_state[Rev1+chan] >= DOWN && pot_changed[START1_POT+chan])
		{
			vol_bkc->combo_state = COMBO_ACTIVE;
			vol_bkc->value_crossed = 0;
		}

		// Update the volume param when the combo is active
		if (vol_bkc->combo_state == COMBO_ACTIVE)
		{
			if (!vol_bkc->value_crossed)
			{
				t_f = f_param[chan][VOLUME] - (bracketed_potadc[START1_POT + chan] / 4096.0);

				//Value-crossing enables VOLUME
				//Just because combo is active, doesn't mean we're updating the volume param yet..
				//...we have to get within +/-4% of current volume before we start tracking the pot
				//...this prevents volume pops when the combo begins
				if (t_f > -0.04 && t_f < 0.04)
					vol_bkc->value_crossed = 1;
			}
			if (vol_bkc->value_crossed)
				f_param[chan][VOLUME] = bracketed_potadc[START1_POT + chan] / 4096.0;
		}

		// Inactivate the combo if the StartPos pot moves while the Rev button is up
		if (button_state[Rev1+chan] == UP && pot_changed[START1_POT+chan])
			vol_bkc->combo_state = COMBO_INACTIVE;



	} //for chan


	//
	// REC SAMPLE POT
	//

	//
	// RecBank + RecSample = change rec bank blink
	//
	if (button_state[RecBank] >= DOWN)
	{
		this_bank_bkc 	= &g_button_knob_combo[bkc_RecBank][bkc_RecSample];
		new_val = detent_num(bracketed_potadc[RECSAMPLE_POT]);

		// If the combo is not active, activate it when we detect the knob was turned to a new detent
		//
		if (this_bank_bkc->combo_state != COMBO_ACTIVE)
		{
			old_val = detent_num(this_bank_bkc->latched_value);

			if (new_val != old_val)
			{
				this_bank_bkc->combo_state = COMBO_ACTIVE;
				this_bank_bkc->hover_value = i_param[REC][BANK];
			}
		}

		// If the combo is active, then use the RecSample pot to update the Hover value
		// (which becomes the RecBank param when we release the button)
		//
		else
		{
			//Calcuate the (tentative) new bank, based on the new_val and the current hover_value
			trial_bank = get_bank_color_digit(this_bank_bkc->hover_value) + (new_val*10);

			//bring the # blinks down until we get a bank in the valid range
			while (trial_bank >= MAX_NUM_BANKS) trial_bank-=10;

			if (this_bank_bkc->hover_value != trial_bank)
			{
				flags[RecBankHoverChanged] = 3;
				this_bank_bkc->hover_value = trial_bank;
			}
		}
	}

	//
	// Edit + RecSample knob = control trig delay
	//
	else if (global_mode[EDIT_MODE])
	{
		edit_bkc = &g_button_knob_combo[bkc_Edit][bkc_RecSample];

		new_val = detent_num( bracketed_potadc[RECSAMPLE_POT] ); //valid values are 1 to 10

		// If the combo is not active, activate it when we detect the knob was turned to a new detent
		if (edit_bkc->combo_state != COMBO_ACTIVE)
		{
			old_val = detent_num( edit_bkc->latched_value );
			if (new_val != old_val)
				edit_bkc->combo_state = COMBO_ACTIVE;
		}

		// If the combo is active, see if the value has changed. Then update the params and flag for LED response
		else
		{
			t_trig_delay_val = new_val + 1; //convert detents 0..9 to 1..10
			if ( t_trig_delay_val != global_mode[TRIG_DELAY] )
			{
				global_params.play_trig_delay 				= calc_trig_delay( t_trig_delay_val );
				global_params.play_trig_latch_pitch_time 	= calc_pitch_latch_time( t_trig_delay_val );

				flags[ChangedTrigDelay] = 1;
				global_mode[TRIG_DELAY] = t_trig_delay_val;

				flags32[SaveUserSettingsLater] = 0x800000; //10-15s

			}
		}


	}

	//If the RecBank and Edit buttons are not down, just change the sample
	//Note: we don't have value-crossing for the RecBank knob, so this section
	//is simplier than the case of play Bank1/2 + Sample1/2
	else
	{
		old_val = i_param[REC][SAMPLE];
		new_val = detent_num(bracketed_potadc[RECSAMPLE_POT]);

		if (old_val != new_val)
		{
			i_param[REC][SAMPLE] = new_val;
			flags[RecSampleChanged] = 1;

			if (global_mode[MONITOR_RECORDING] != MONITOR_OFF)
			{
				flags[RecSampleChanged_light] = 10;
			}
		}
	}


}


//
// Handle all flags to change modes
//
void process_mode_flags(void)
{

	if (flags[Rev1Trig])
	{
		flags[Rev1Trig]=0;
		toggle_reverse(0);
	}
	if (flags[Rev2Trig])
	{
		flags[Rev2Trig]=0;
		toggle_reverse(1);
	}

	if (flags[Play1But])
	{
		flags[Play1But]=0;
		toggle_playing(0);
	}
	if (flags[Play2But])
	{
		flags[Play2But]=0;
		toggle_playing(1);
	}

	if (flags[Play1TrigDelaying])
	{
		if ((sys_tmr - play_trig_timestamp[0]) > global_params.play_trig_latch_pitch_time)
			flags[LatchVoltOctCV1] = 0;
		else
			flags[LatchVoltOctCV1] = 1;

		if ((sys_tmr - play_trig_timestamp[0]) > global_params.play_trig_delay)
		{
			flags[Play1Trig] 			= 1;
			flags[Play1TrigDelaying]	= 0;
			flags[LatchVoltOctCV1] 		= 0;
		}
	}
	if (flags[Play1Trig])
	{
		start_playing(0);
		flags[Play1Trig]	= 0;
	}



	if (flags[Play2TrigDelaying])
	{
		if ((sys_tmr - play_trig_timestamp[0]) > global_params.play_trig_latch_pitch_time)
			flags[LatchVoltOctCV2] = 0;
		else
			flags[LatchVoltOctCV2] = 1;

		if ((sys_tmr - play_trig_timestamp[1]) > global_params.play_trig_delay)
		{
			flags[Play2Trig] 			= 1;
			flags[Play2TrigDelaying]	= 0;
			flags[LatchVoltOctCV2]		= 0;
			// DEBUG3_OFF;
		}
	}
	if (flags[Play2Trig])
	{
		// DEBUG3_OFF;
		start_playing(1);
		flags[Play2Trig]	 = 0;
		flags[LatchVoltOctCV2] = 0;
	}

	if (flags[RecTrig]==1)
	{
		flags[RecTrig]=0;
		toggle_recording();
	}

	if (flags[ToggleMonitor])
	{
		flags[ToggleMonitor] = 0;

		if (global_mode[ENABLE_RECORDING] && global_mode[MONITOR_RECORDING] != MONITOR_OFF)
		{
			global_mode[ENABLE_RECORDING] 	= 0;
			global_mode[MONITOR_RECORDING] 	= MONITOR_OFF;
			stop_recording();
		}
		else
		{
			global_mode[ENABLE_RECORDING] = 1;
			global_mode[MONITOR_RECORDING] = MONITOR_BOTH; //monitor channel 1 and 2
			i_param[0][LOOPING] = 0;
			i_param[1][LOOPING] = 0;
		}
	}


	if (flags[ToggleLooping1])
	{
		flags[ToggleLooping1] = 0;

		if (i_param[0][LOOPING])
		{
			i_param[0][LOOPING] = 0;
		}
		else
		{
			i_param[0][LOOPING] = 1;
			if (play_state[0] == SILENT)
				flags[Play1But] = 1;
		}


	}

	if (flags[ToggleLooping2])
	{
		flags[ToggleLooping2] = 0;

		if (i_param[1][LOOPING])
		{
			i_param[1][LOOPING] = 0;
		}
		else
		{
			i_param[1][LOOPING] = 1;
			if (play_state[1] == SILENT)
				flags[Play2But] = 1;
		}
	}
}

uint8_t detent_num(uint16_t adc_val)
{
	if (adc_val<=212)
		return(0);
	else if (adc_val<=625)
		return(1);
	else if (adc_val<=1131)
		return(2);
	else if (adc_val<=1562)
		return(3);
	else if (adc_val<=1995)
		return(4);
	else if (adc_val<=2475)
		return(5);
	else if (adc_val<=2825)
		return(6);
	else if (adc_val<=3355)
		return(7);
	else if (adc_val<=3840)
		return(8);
	else
		return(9);
}

//const uint16_t detent_tops[10] = {212, 625, 1131, 1562, 1995, 2475, 2825, 3355, 3840, 4095};
#define DETENT_MIN_DEPTH 40

uint8_t detent_num_antihys(uint16_t adc_val, uint8_t cur_detent)
{
	uint8_t raw_detent;
	int16_t lower_adc_bound, upper_adc_bound;

	raw_detent = detent_num(adc_val);

	if (raw_detent > cur_detent)
	{
		lower_adc_bound = (int16_t)adc_val - DETENT_MIN_DEPTH;
		if (detent_num(lower_adc_bound) > cur_detent)
			return(raw_detent);
	}
	else
	if (cur_detent > raw_detent)
	{
		upper_adc_bound = (int16_t)adc_val + DETENT_MIN_DEPTH;

		if (detent_num(upper_adc_bound) < cur_detent)
			return(raw_detent);
	}

	return cur_detent;
}


//PCB version 1.0 (==0): 0..630
//PCB version 1.1 (==1): 0..84, 180, 360

uint32_t calc_trig_delay(uint8_t trig_delay_setting)
{
	if (trig_delay_setting > 10 || trig_delay_setting < 1) trig_delay_setting = 1; //range assertation

	if ( PCB_version == 0 )		return ((trig_delay_setting-1)*70);
	if ( PCB_version > 0 )		{
		if (trig_delay_setting<=8)	return (trig_delay_setting*12) - 8;
		if (trig_delay_setting==9)	return 180;
		if (trig_delay_setting==10)	return 360;
	}
	return(0);
}

//PCB version 1.0 (==0): 0..315
//PCB version 1.1 (==1): 0..42, 50, 60

uint32_t calc_pitch_latch_time(uint8_t trig_delay_setting)
{
	if (trig_delay_setting > 10 || trig_delay_setting < 1) trig_delay_setting = 1; //range assertation

	if ( PCB_version == 0 )		return ((trig_delay_setting-1)*35);
	if ( PCB_version > 0 )		{
		if (trig_delay_setting<=8)	return ((trig_delay_setting-1)*6);
		if (trig_delay_setting==9)	return 50;
		if (trig_delay_setting==10)	return 60;
	}
	return(0);
}


void adc_param_update_IRQHandler(void)
{

	if (TIM_GetITStatus(TIM9, TIM_IT_Update) != RESET) {

		process_pot_adc();

		if (global_mode[CALIBRATE])
		{
			update_calibration(1);
			update_calibrate_leds();
		}
		else
		if (global_mode[SYSTEM_MODE])
		{
			update_system_mode();
			update_system_mode_leds();
		}
		else
			update_params();


		TIM_ClearITPendingBit(TIM9, TIM_IT_Update);

	}
}

