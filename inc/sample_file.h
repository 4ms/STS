/*
 * sample_file.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>
#include "ff.h"

typedef struct Sample {
	char		filename[_MAX_LFN];
	uint32_t 	sampleSize;
	uint32_t 	startOfData;
	uint8_t 	sampleByteSize;
	uint32_t 	sampleRate;
	uint8_t 	numChannels;
	uint8_t 	blockAlign;

	uint32_t	inst_start;
	uint32_t	inst_end;
	uint32_t	inst_size;
	float		inst_gain;

	uint16_t	PCM;
	uint8_t		file_found;
} Sample;



uint8_t load_sample_header(Sample *s_sample, FIL *sample_file);
void clear_sample_header(Sample *s_sample);
FRESULT reload_sample_file(FIL *fil, Sample *s_sample);
FRESULT create_linkmap(FIL *fil, uint8_t chan, uint8_t samplenum);



