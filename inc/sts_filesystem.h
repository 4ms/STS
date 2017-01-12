/*
 * sts_filesystem.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#ifndef INC_STS_FILESYSTEM_H_
#define INC_STS_FILESYSTEM_H_
#include <stm32f4xx.h>
#include "ff.h"

typedef struct Sample {
	char	filename[_MAX_LFN];
	uint32_t sampleSize;
	uint32_t startOfData;
	uint8_t sampleByteSize;
	uint32_t sampleRate;
	uint8_t numChannels;
	uint8_t blockAlign;
} Sample;


uint8_t bank_to_color(uint8_t bank, char *color);
uint8_t load_bank_from_disk(uint8_t bank, uint8_t chan);
void clear_sample_header(Sample *s_sample);
void check_bank_dirs(void);
uint8_t next_enabled_bank(uint8_t bank);
void enable_bank(uint8_t bank);

#endif /* INC_STS_FILESYSTEM_H_ */
