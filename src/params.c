/*
 * params.c
 *
 *  Created on: Mar 27, 2015
 *      Author: design
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
#include "pitch_pot_cv.h"
#include "edit_mode.h"
#include "calibration.h"
#include "bank.h"
#include "button_knob_combo.h"

extern float pitch_pot_cv[4096];
const float voltoct[4096];


extern enum PlayStates play_state[NUM_PLAY_CHAN];

extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern SystemCalibrations *system_calibrations;

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

extern uint8_t disable_mode_changes;

volatile float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
uint8_t settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

uint8_t	global_mode[NUM_GLOBAL_MODES];

uint8_t flags[NUM_FLAGS];
uint32_t play_trig_timestamp[2];

uint8_t flag_pot_changed[NUM_POT_ADCS];

extern uint8_t recording_enabled;

// extern uint8_t lock_start;
// extern uint8_t lock_length;
extern uint8_t scrubbed_in_edit;

extern enum ButtonStates button_state[NUM_BUTTONS];
volatile uint32_t 			sys_tmr;

extern ButtonKnobCombo g_button_knob_combo[NUM_BUTTON_KNOB_COMBO_BUTTONS][NUM_BUTTON_KNOB_COMBO_KNOBS];

int32_t MIN_POT_ADC_CHANGE[NUM_POT_ADCS];
int32_t MIN_CV_ADC_CHANGE[NUM_CV_ADCS];


float POT_LPF_COEF[NUM_POT_ADCS];
float CV_LPF_COEF[NUM_CV_ADCS];
float RAWCV_LPF_COEF[NUM_CV_ADCS];

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

int16_t prepared_cvadc[NUM_CV_ADCS];

//20 gives about 10ms slew
//40 gives about 18ms slew
//100 gives about 40ms slew
#define PITCH_LPF_SZ 40
int32_t pitch_lpf[NUM_PLAY_CHAN][PITCH_LPF_SZ];
uint32_t pitch_lpf_i[NUM_PLAY_CHAN];


#define PITCH_DELAY_BUFFER_SZ 1
int32_t delayed_pitch_cvadc_buffer[NUM_PLAY_CHAN][PITCH_DELAY_BUFFER_SZ];
uint32_t del_cv_i[NUM_PLAY_CHAN];

#define PLAY_TRIG_LATCH_PITCH_TIME 512 

#define PLAY_TRIG_DELAY 1024 
// delay in sec = # / 44100Hz
// There is an additional delay before audio starts, 5.8ms - 11.6ms due to the codec needing to be pre-loaded


//LPF of raw ADC values for calibration
float smoothed_rawcvadc[NUM_CV_ADCS];
int16_t i_smoothed_rawcvadc[NUM_CV_ADCS];

//Change in pot since last process_adc
int32_t pot_delta[NUM_POT_ADCS];
int32_t cv_delta[NUM_CV_ADCS];


void init_params(void)
{
	uint32_t chan,i;

	//set_default_calibration_values();

	for (chan=0;chan<NUM_PLAY_CHAN;chan++){
		f_param[chan][PITCH] 	= 1.0;
		f_param[chan][START] 	= 0.0;
		f_param[chan][LENGTH] 	= 1.0;

		i_param[chan][BANK] 		= 0;
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

}

//initializes modes that aren't read from flash ram
void init_modes(void)
{
	global_mode[CALIBRATE] = 0;
	global_mode[SYSTEM_SETTINGS] = 0;
	global_mode[MONITOR_RECORDING] = 0;
	global_mode[ENABLE_RECORDING] = 0;
	global_mode[EDIT_MODE] = 0;
	global_mode[ASSIGN_MODE] = 0;
}


void init_LowPassCoefs(void)
{
	float t;
	uint8_t i;

	t=15.0; //LPF for Pitch CV

	CV_LPF_COEF[PITCH_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[PITCH_CV*2+1] = 1.0-(1.0/t);

	t=50.0;

	CV_LPF_COEF[START_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[START_CV*2+1] = 1.0-(1.0/t);

	CV_LPF_COEF[LENGTH_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[LENGTH_CV*2+1] = 1.0-(1.0/t);

	t=1.0; //No LPF for Sample CV

	CV_LPF_COEF[SAMPLE_CV*2] = 1.0-(1.0/t);
	CV_LPF_COEF[SAMPLE_CV*2+1] = 1.0-(1.0/t);

	t=300.0;

	RAWCV_LPF_COEF[PITCH_CV*2] = 1.0-(1.0/t);
	RAWCV_LPF_COEF[PITCH_CV*2+1] = 1.0-(1.0/t);

	RAWCV_LPF_COEF[START_CV*2] = 1.0-(1.0/t);
	RAWCV_LPF_COEF[START_CV*2+1] = 1.0-(1.0/t);

	RAWCV_LPF_COEF[LENGTH_CV*2] = 1.0-(1.0/t);
	RAWCV_LPF_COEF[LENGTH_CV*2+1] = 1.0-(1.0/t);

	RAWCV_LPF_COEF[SAMPLE_CV*2] = 1.0-(1.0/t);
	RAWCV_LPF_COEF[SAMPLE_CV*2+1] = 1.0-(1.0/t);


	MIN_CV_ADC_CHANGE[PITCH_CV*2] = 20;
	MIN_CV_ADC_CHANGE[PITCH_CV*2+1] = 20;

	MIN_CV_ADC_CHANGE[START_CV*2] = 20;
	MIN_CV_ADC_CHANGE[START_CV*2+1] = 20;

	MIN_CV_ADC_CHANGE[LENGTH_CV*2] = 20;
	MIN_CV_ADC_CHANGE[LENGTH_CV*2+1] = 20;

	MIN_CV_ADC_CHANGE[SAMPLE_CV*2] = 1;
	MIN_CV_ADC_CHANGE[SAMPLE_CV*2+1] = 1;

	t=20.0; //50.0 = about 100ms to turn a knob fully

	POT_LPF_COEF[PITCH_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[PITCH_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[START_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[START_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[LENGTH_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[LENGTH_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[SAMPLE_POT*2] = 1.0-(1.0/t);
	POT_LPF_COEF[SAMPLE_POT*2+1] = 1.0-(1.0/t);

	POT_LPF_COEF[RECSAMPLE_POT] = 1.0-(1.0/t);



	MIN_POT_ADC_CHANGE[PITCH_POT*2] = 15;
	MIN_POT_ADC_CHANGE[PITCH_POT*2+1] = 15;

	MIN_POT_ADC_CHANGE[START_POT*2] = 20;
	MIN_POT_ADC_CHANGE[START_POT*2+1] = 20;

	MIN_POT_ADC_CHANGE[LENGTH_POT*2] = 20;
	MIN_POT_ADC_CHANGE[LENGTH_POT*2+1] = 20;

	MIN_POT_ADC_CHANGE[SAMPLE_POT*2] = 60;
	MIN_POT_ADC_CHANGE[SAMPLE_POT*2+1] = 60;

	MIN_POT_ADC_CHANGE[RECSAMPLE_POT] = 60;


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
		smoothed_rawcvadc[i]	=0;
		bracketed_cvadc[i]	=0;
		i_smoothed_cvadc[i]		=0x7FFF;
		i_smoothed_rawcvadc[i]	=0x7FFF;
		cv_delta[i]				=0;
	}

	for (i=0;i<PITCH_DELAY_BUFFER_SZ;i++)
	{
		delayed_pitch_cvadc_buffer[0][i]=2048;
		delayed_pitch_cvadc_buffer[1][i]=2048;
	}
	del_cv_i[0]=0;
	del_cv_i[1]=0;


	smoothed_cvadc[0] = 2048;
	smoothed_cvadc[1] = 2048;
	for (i=0;i<PITCH_LPF_SZ;i++)
	{
		pitch_lpf[0][i]=2048;
		pitch_lpf[1][i]=2048;
	}
	pitch_lpf_i[0]=0;
	pitch_lpf_i[1]=0;

}

void reset_cv_lowpassfilter(uint8_t cv_num)
{
	smoothed_cvadc[cv_num] = (float)(cvadc_buffer[cv_num] + system_calibrations->cv_calibration_offset[cv_num]);
	i_smoothed_cvadc[cv_num] = smoothed_cvadc[cv_num];
	bracketed_cvadc[cv_num] = smoothed_cvadc[cv_num];
}

void process_pitch_adc(void)
{//takes about 3.4us to run, at -O0

	uint8_t i;
	int32_t old_val, new_val;
	int32_t t;

	//This function assumes:
	//Channel 1's pitch cv = PITCH_CV*2   = 0
	//Channel 2's pitch cv = PITCH_CV*2+1 = 1


	for (i=0;i<NUM_PLAY_CHAN;i++)
	{
		//Apply Linear average LPF:

		//Pull out the oldest value (before overwriting it):
		old_val = pitch_lpf[i][pitch_lpf_i[i]];

		//Put in the new cv value (overwrite oldest value):
		new_val 					= cvadc_buffer[i] + system_calibrations->cv_calibration_offset[i];
		pitch_lpf[i][pitch_lpf_i[i]] 	= new_val;

		//Increment the index, wrapping around the whole buffer
		if (++pitch_lpf_i[i] >= PITCH_LPF_SZ) pitch_lpf_i[i]=0;

		//Calculate the arithmetic average (FIR LPF)
		smoothed_cvadc[i] = (float)((smoothed_cvadc[i] * PITCH_LPF_SZ) - old_val + new_val) / (float)PITCH_LPF_SZ;

		i_smoothed_cvadc[i] = (int16_t)smoothed_cvadc[i];
		if (i_smoothed_cvadc[i] < 0) i_smoothed_cvadc[i] = 0;
		if (i_smoothed_cvadc[i] > 4095) i_smoothed_cvadc[i] = 4095;


		//Apply bracketing, comparing the latest LPF'ed value with the most recent value we put into the delayed_ buffer
		t=i_smoothed_cvadc[i] - bracketed_cvadc[i];

//37 - 24 = 13 > 12
//37 - 12 = 25
//24==>25
		if (t>MIN_CV_ADC_CHANGE[i])
		{
			cv_delta[i] = t;
			//bracketed_cvadc[i] = i_smoothed_cvadc[i] - (MIN_CV_ADC_CHANGE[i]/2);
			bracketed_cvadc[i] = i_smoothed_cvadc[i] - (MIN_CV_ADC_CHANGE[i]);
		}
//11 - 24 = -13  < -12
//11 + 12 = 23
//24==>23
		else if (t<-MIN_CV_ADC_CHANGE[i])
		{
			cv_delta[i] = t;
		//	bracketed_cvadc[i] = i_smoothed_cvadc[i] + (MIN_CV_ADC_CHANGE[i]/2);
			bracketed_cvadc[i] = i_smoothed_cvadc[i] + (MIN_CV_ADC_CHANGE[i]);
		}

		// if ((t>MIN_CV_ADC_CHANGE[i]) || (t<-MIN_CV_ADC_CHANGE[i]))
		// {
		// 	cv_delta[i] = t;
		// 	bracketed_cvadc[i] = (bracketed_cvadc[i] + i_smoothed_cvadc[i])/2;
		// }

		//Set the final usable value to the oldest element in the delayed_ buffer
		prepared_cvadc[i] = delayed_pitch_cvadc_buffer[i][del_cv_i[i]];

		//Overwrite the oldest value with the newest value
		delayed_pitch_cvadc_buffer[i][del_cv_i[i]] = bracketed_cvadc[i];

		//Increment the index, wrapping around the whole buffer
		if (++del_cv_i[i] >= PITCH_DELAY_BUFFER_SZ) del_cv_i[i]=0;


	}
}

void process_adc(void)
{
	uint8_t i;
	int32_t t;
	int32_t new_val;

	static uint32_t track_moving_pot[NUM_POT_ADCS]={0,0,0,0,0,0,0,0,0};

	//
	// Run a LPF on the pots and CV jacks
	//
	for (i=0;i<NUM_POT_ADCS;i++)
	{
		flag_pot_changed[i]=0;

		smoothed_potadc[i] = LowPassSmoothingFilter(smoothed_potadc[i], (float)potadc_buffer[i], POT_LPF_COEF[i]);
		i_smoothed_potadc[i] = (int16_t)smoothed_potadc[i];

		t=i_smoothed_potadc[i] - bracketed_potadc[i];
		if ((t>MIN_POT_ADC_CHANGE[i]) || (t<-MIN_POT_ADC_CHANGE[i]))
			track_moving_pot[i]=250;

		if (track_moving_pot[i])
		{
			track_moving_pot[i]--;
			flag_pot_changed[i]=1;
			pot_delta[i] = t;
			bracketed_potadc[i] = i_smoothed_potadc[i];
		}
	}


	for (i=START_CV*2;i<NUM_CV_ADCS;i++) //skip the PITCH pots
	{
		// if (i==PITCH_CV*2 || i==PITCH_CV*2+1)
		// {
		// }
		// else
		// {
			new_val = cvadc_buffer[i] + system_calibrations->cv_calibration_offset[i];

			smoothed_cvadc[i] = LowPassSmoothingFilter(smoothed_cvadc[i], (float)(new_val), CV_LPF_COEF[i]);
		// }

		i_smoothed_cvadc[i] = (int16_t)smoothed_cvadc[i];
		if (i_smoothed_cvadc[i] < 0) i_smoothed_cvadc[i] = 0;
		if (i_smoothed_cvadc[i] > 4095) i_smoothed_cvadc[i] = 4095;

		if (global_mode[CALIBRATE])
		{
			smoothed_rawcvadc[i] = LowPassSmoothingFilter(smoothed_rawcvadc[i], (float)(cvadc_buffer[i]), RAWCV_LPF_COEF[i]);
			i_smoothed_rawcvadc[i] = (int16_t)smoothed_rawcvadc[i];
			if (i_smoothed_rawcvadc[i] < 0) i_smoothed_rawcvadc[i] = 0;
			if (i_smoothed_rawcvadc[i] > 4095) i_smoothed_rawcvadc[i] = 4095;
		}

		t=i_smoothed_cvadc[i] - bracketed_cvadc[i];
		if ((t>MIN_CV_ADC_CHANGE[i]) || (t<-MIN_CV_ADC_CHANGE[i]))
		{
			cv_delta[i] = t;
			bracketed_cvadc[i] = i_smoothed_cvadc[i];
		}
	}

}




void update_params(void)
{
	uint8_t chan;
	uint8_t knob;
	uint8_t old_val;
	uint8_t new_val;
	int32_t t_pitch_potadc;
	float t_f;

	uint32_t trial_bank;
	uint8_t samplenum, banknum;
	ButtonKnobCombo *t_this_bkc; //pointer to the currently active button/knob combo action
	ButtonKnobCombo *t_other_bkc; //pointer to another button/knob combo action (makes code more readable)

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

		if (flag_pot_changed[LENGTH_POT*2+1])
		{
			nudge_trim_size(&samples[banknum][samplenum], pot_delta[LENGTH_POT*2+1]);

			flag_pot_changed[LENGTH_POT*2+1] = 0;
			//pot_delta[LENGTH_POT*2+1] = 0;

			scrubbed_in_edit = 1;
			f_param[0][START] = 0.999f;
			f_param[0][LENGTH] = 0.201f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;

		}
		

		//
		// Trim Start
		// 

		if (flag_pot_changed[START_POT*2+1])
		{
			nudge_trim_start(&samples[banknum][samplenum], pot_delta[START_POT*2+1]);
			flag_pot_changed[START_POT*2+0] = 0;
			//pot_delta[START_POT*2+1] = 0;

			scrubbed_in_edit = 1;
			f_param[0][START] = 0.000f;
			f_param[0][LENGTH] = 0.201f;
			i_param[0][LOOPING] = 1;
			i_param[0][REV] = 0;
			if (play_state[0] == SILENT) flags[Play1Trig] = 1;
		}

	//	check_trim_bounds();

		//
		// Gain (sample ch2 pot): 
		// 0.1x when pot is at 0%
		// 1x when pot is at 50%
		// 5x when pot is at 100%
		// CV jack is disabled
		//
		if (flag_pot_changed[SAMPLE_POT*2+1])
		{
			if (bracketed_potadc[SAMPLE_POT*2+1] < 2020) 
				t_f = (bracketed_potadc[SAMPLE_POT*2+1] / 2244.44f) + 0.1; //0.1 to 1.0
			else if (bracketed_potadc[SAMPLE_POT*2+1] < 2080)
				t_f = 1.0;
			else
				t_f	= (bracketed_potadc[SAMPLE_POT*2+1] - 1577.5) / 503.5f; //1.0 to 5.0
			set_sample_gain(&samples[banknum][samplenum], t_f);
		}

		//
		// PITCH POT
		//

		t_pitch_potadc = i_smoothed_potadc[PITCH_POT*2+0] + system_calibrations->pitch_pot_detent_offset[0];
		if (t_pitch_potadc > 4095) t_pitch_potadc = 4095;
		if (t_pitch_potadc < 0) t_pitch_potadc = 0;


		f_param[0][PITCH] = pitch_pot_cv[t_pitch_potadc] * voltoct[prepared_cvadc[PITCH_CV*2+0]];
		if (f_param[0][PITCH] > MAX_RS)
			f_param[0][PITCH] = MAX_RS;

		//
		// SAMPLE POT + CV
		//
		old_val = i_param[0][SAMPLE];
		new_val = detent_num( bracketed_potadc[SAMPLE_POT*2+0] + bracketed_cvadc[SAMPLE_CV*2+0] );

		if (old_val != new_val)
		{
			i_param[0][SAMPLE] = new_val;
			flags[PlaySample1Changed] = 1;

			//Exit assignment mode (if we were in it)
			exit_assignment_mode();
		}



	} //if not EDIT_MODE
	else 
	{

		for (chan=0;chan<2;chan++)
		{
			//
			// LENGTH POT + CV
			//

			f_param[chan][LENGTH] 	= (bracketed_potadc[LENGTH_POT*2+chan] + bracketed_cvadc[LENGTH_CV*2+chan]) / 4096.0;

			if (f_param[chan][LENGTH] > 0.990)		f_param[chan][LENGTH] = 1.0;
			if (f_param[chan][LENGTH] <= 0.000244)	f_param[chan][LENGTH] = 0.000244;


			//
			// START POT + CV
			//

			f_param[chan][START] 	= (bracketed_potadc[START_POT*2+chan] + bracketed_cvadc[START_CV*2+chan]) / 4096.0;

			if (f_param[chan][START] > 0.99)		f_param[chan][START] = 1.0;
			if (f_param[chan][START] <= 0.0003)		f_param[chan][START] = 0.0;

			//
			// PITCH POT + CV
			//

			t_pitch_potadc = i_smoothed_potadc[PITCH_POT*2+chan] + system_calibrations->pitch_pot_detent_offset[chan];
			if (t_pitch_potadc > 4095) t_pitch_potadc = 4095;
			if (t_pitch_potadc < 0) t_pitch_potadc = 0;

			if(flags[latch1voctcv1+chan])
				f_param[chan][PITCH] = pitch_pot_cv[t_pitch_potadc] * voltoct[voct_latch_value[chan]];

			else
				f_param[chan][PITCH] = pitch_pot_cv[t_pitch_potadc] * voltoct[prepared_cvadc[PITCH_CV*2+chan]];

			if (f_param[chan][PITCH] > MAX_RS)
			    f_param[chan][PITCH] = MAX_RS;

			//
			// SAMPLE POT + CV
			//

			//
			//Button-Knob combo: Bank + Sample
			//Holding Bank X's button while turning a Sample knob changes channel X's bank
			//
			if (button_state[Bank1 + chan] >= DOWN)
			{
				//Go through both the knobs
				//
				for (knob = 0; knob < 2 /*NUM_BUTTON_KNOB_COMBO_KNOBS*/; knob++) //FixMe: This is not portable, it only works because we have Sample1 and Sample2 in the start of the bkc array
				{
					t_this_bkc 	= &g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1 + knob];
					t_other_bkc = &g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1 + 1-knob];

					new_val = detent_num(bracketed_potadc[SAMPLE_POT*2 + knob]);

					//If the combo is active, then use the Sample pot to update the Hover value
					//(which becomes the Bank param when we release the button)
					//
					if (t_this_bkc->combo_state == COMBO_ACTIVE)
					{
						//Calcuate the (tentative) new bank, based on the new_val and the current hover_value
						//
						if (knob==0) trial_bank = new_val + (get_bank_blink_digit(t_this_bkc->hover_value) * 10);
						if (knob==1) trial_bank = get_bank_color_digit(t_this_bkc->hover_value) + (new_val*10);

						//If the new bank is enabled (contains samples) then update the hover_value
						//
						if (is_bank_enabled(trial_bank))
						{
							//Set both hover_values to the same value, since they are linked
							//
							if (t_this_bkc->hover_value != trial_bank)
							{
								flags[PlayBankHover1Changed + chan] = 3;
								t_this_bkc->hover_value = trial_bank;
								t_other_bkc->hover_value = trial_bank;
							}		
						}
					}

					//If the combo is not active,
					//Activate it when we detect the knob was turned to a new detent
					else
					{
						old_val = detent_num(t_this_bkc->latched_value);

						if (new_val != old_val)
						{
							t_this_bkc->combo_state = COMBO_ACTIVE;

							//Initialize the hover value
							//Set both hover_values to the same value, since they are linked
							t_this_bkc->hover_value = i_param[chan][BANK];
							t_other_bkc->hover_value = i_param[chan][BANK];
						}
					}
				}
			}


			//
			//If we're not doing a button+knob combo, just change the sample
			//
			t_this_bkc 	= &g_button_knob_combo[bkc_Bank1][bkc_Sample1 + chan];
			t_other_bkc = &g_button_knob_combo[bkc_Bank2][bkc_Sample1 + chan];

			if (t_this_bkc->combo_state != COMBO_ACTIVE	&&	t_other_bkc->combo_state != COMBO_ACTIVE)
			{

				//See if we need to update the sample by comparing the old value to the new value
				//
				//If we recently ended a combo motion, use the latched value of the pot for the old value
				//Otherwise use the actual sample param
				//
				if (t_this_bkc->combo_state == COMBO_LATCHED)
					old_val = detent_num(t_this_bkc->latched_value);

				else
				if (t_other_bkc->combo_state == COMBO_LATCHED)
					old_val = detent_num(t_other_bkc->latched_value);

				else
					old_val = i_param[chan][SAMPLE];

				new_val = detent_num( bracketed_potadc[SAMPLE_POT*2+chan] + bracketed_cvadc[SAMPLE_CV*2+chan] );

				if (old_val != new_val)
				{
					//The knob has moved to a new detent (without the a Bank button down)

					//
					//Inactivate the combo motion
					// 
					t_this_bkc->combo_state = COMBO_INACTIVE;
					t_other_bkc->combo_state = COMBO_INACTIVE;

					//Update the sample parameter
					i_param[chan][SAMPLE] = new_val;

					flags[PlaySample1Changed + chan] = 1;

					//Exit assignment mode if we moved channel 1's Sample pot (or CV)
					if (chan==0)
						exit_assignment_mode();

					//
					//Set a flag to initiate a bright flash on the Play button
					//
					//We first have to know if there is a sample in the new place,
					//by seeing if there is a non-blank filename
					//
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
			}

		} //for chan


		//
		// REC SAMPLE POT
		//
		t_this_bkc 	= &g_button_knob_combo[bkc_RecBank][bkc_RecSample];

		if (button_state[RecBank] >= DOWN)
		{
			new_val = detent_num(i_smoothed_potadc[RECSAMPLE_POT]);

			// If the combo is not active,
			// Activate it when we detect the knob was turned to a new detent
			//
			if (t_this_bkc->combo_state != COMBO_ACTIVE)
			{
				old_val = detent_num(t_this_bkc->latched_value);

				if (new_val != old_val)
				{
					t_this_bkc->combo_state = COMBO_ACTIVE;

					//Initialize the hover value
					t_this_bkc->hover_value = i_param[REC][BANK];
				}
			}

			// If the combo is active, then use the RecSample pot to update the Hover value
			// (which becomes the RecBank param when we release the button)
			//	
			else			
			{
				//Calcuate the (tentative) new bank, based on the new_val and the current hover_value

				trial_bank = get_bank_color_digit(t_this_bkc->hover_value) + (new_val*10);

				//bring the # blinks down until we get a bank in the valid range
				while (trial_bank >= MAX_NUM_BANKS) trial_bank-=10;

				if (t_this_bkc->hover_value != trial_bank)
				{
					flags[RecBankHoverChanged] = 3;
					t_this_bkc->hover_value = trial_bank;
				}		
			}
		}

		//If the RecBank button is not down, just change the sample
		//Note: we don't have value-crossing for the RecBank knob, so this section
		//is simplier than the case of play Bank1/2 + Sample1/2
		else
		{
			old_val = i_param[REC][SAMPLE];
			new_val = detent_num(i_smoothed_potadc[RECSAMPLE_POT]);

			if (old_val != new_val)
			{
				i_param[REC][SAMPLE] = new_val;
				flags[RecSampleChanged] = 1;

				if (global_mode[MONITOR_RECORDING])
				{
					flags[RecSampleChanged_light] = 10;
				}
			}
		}

	} //else if EDIT_MODE


}


//
// Handle all flags to change modes
//
void process_mode_flags(void)
{

	if (!disable_mode_changes)
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

		if (flags[Play1Trig])
		{
			if ((sys_tmr - play_trig_timestamp[0]) > PLAY_TRIG_LATCH_PITCH_TIME)
				flags[latch1voctcv1] = 0;
			else
				flags[latch1voctcv1] = 1;

			if ((sys_tmr - play_trig_timestamp[0]) > PLAY_TRIG_DELAY) 
			{
				start_playing(0);
				flags[Play1Trig]	= 0;
				flags[latch1voctcv1] = 0;
				DEBUG3_OFF;
			}
			
		}

		if (flags[Play2Trig])
		{
			flags[latch1voctcv2] = 1;
			if ((sys_tmr - play_trig_timestamp[1]) > PLAY_TRIG_DELAY)
			{
				start_playing(1);
				flags[Play2Trig]	= 0;
				flags[latch1voctcv2] = 0;
			}
		}

		if (flags[RecTrig]==1)
		{
			flags[RecTrig]=0;
			toggle_recording();
		}

		if (flags[ToggleMonitor])
		{
			flags[ToggleMonitor] = 0;

			if (global_mode[ENABLE_RECORDING] && global_mode[MONITOR_RECORDING])
			{
				global_mode[ENABLE_RECORDING] = 0;
				global_mode[MONITOR_RECORDING] = 0;
				stop_recording();
			}
			else
			{
				global_mode[ENABLE_RECORDING] = 1;
				global_mode[MONITOR_RECORDING] = 1;
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


void adc_param_update_IRQHandler(void)
{

	if (TIM_GetITStatus(TIM9, TIM_IT_Update) != RESET) {


		process_adc();

		if (global_mode[CALIBRATE])
		{
			update_calibration();
			update_calibrate_leds();
		}
		else
			update_params();

		//process_mode_flags();

		if (global_mode[SYSTEM_SETTINGS])
		{
			//update_system_settings();
			//update_system_settings_leds();
		}


		//check_entering_system_mode();


		TIM_ClearITPendingBit(TIM9, TIM_IT_Update);

	}
}

