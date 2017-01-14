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

uint8_t load_sample_header(Sample *s_sample, FIL *sample_file);
void clear_sample_header(Sample *s_sample);

FRESULT find_next_ext_in_dir(DIR* dir, const char *ext, char *fname);
uint8_t load_bank_from_disk(uint8_t bank);

void check_enabled_banks(void);
uint8_t next_enabled_bank(uint8_t bank);

void enable_bank(uint8_t bank);
void disable_bank(uint8_t bank);

uint8_t load_sampleindex_file(void);
uint8_t write_sampleindex_file(void);

void enter_assignment_mode(uint8_t chan);
void save_exit_assignment_mode(uint8_t chan);
uint8_t load_samples_to_assign(uint8_t bank, uint8_t chan);

void next_unassigned_sample(uint8_t chan);


#endif /* INC_STS_FILESYSTEM_H_ */
