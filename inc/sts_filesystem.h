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

//ToDo: split up SampleFile and Sample so that we aren't repeating data if a sample file is loaded into multiple slots

// typedef struct SampleFile {
// 	char		filename[_MAX_LFN];
// 	uint32_t 	sampleSize;
// 	uint32_t 	startOfData;
// 	uint8_t 	sampleByteSize;
// 	uint32_t 	sampleRate;
// 	uint8_t 	numChannels;
// 	uint8_t 	blockAlign;
// 	float		peak_dBFS;
// } SampleFile;


// typedef struct Sample {
// 	SampleFile 	*sample;
// 	float		start;
// 	float		length;
// 	float		gain;
// } Sample;

typedef struct Sample {
	char		filename[_MAX_LFN];
	uint32_t 	sampleSize;
	uint32_t 	startOfData;
	uint8_t 	sampleByteSize;
	uint32_t 	sampleRate;
	uint8_t 	numChannels;
	uint8_t 	blockAlign;
	float		peak_dBFS; //not yet implemented
} Sample;

typedef struct Slot {
	float		start;
	float		length;
	float		gain;
} Slot;

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

void enter_assignment_mode(void);
void save_exit_assignment_mode(void);
void cancel_exit_assignment_mode(void);
uint8_t load_samples_to_assign(uint8_t bank);

void next_unassigned_sample(void);
uint8_t find_current_sample_in_assign(Sample *s);


#endif /* INC_STS_FILESYSTEM_H_ */
