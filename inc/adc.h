/*
 * adc.h - adc setup
 */

#pragma once

#include <stm32f4xx.h>


enum PotADCs {
	PITCH1_POT,
	PITCH2_POT,
	START1_POT,
	START2_POT,
	LENGTH1_POT,
	LENGTH2_POT,
	SAMPLE1_POT,
	SAMPLE2_POT,
	RECSAMPLE_POT,

	NUM_POT_ADCS
};

enum CVADCs {
	PITCH1_CV,
	PITCH2_CV,
	START1_CV,
	START2_CV,
	LENGTH1_CV,
	LENGTH2_CV,
	SAMPLE1_CV,
	SAMPLE2_CV,
	
	NUM_CV_ADCS
};





void Init_Pot_ADC(uint16_t *ADC_Buffer, uint8_t num_adcs);
void Init_CV_ADC(uint16_t *ADC_Buffer, uint8_t num_adcs);

void Deinit_Pot_ADC(void);
void Deinit_CV_ADC(void);
