/*
 * edit_mode.h
 *
 *  Created on: Feb 27, 2017
 *      Author: design
 */

#ifndef INC_EDIT_MODE_H
#define INC_EDIT_MODE_H
#include <stm32f4xx.h>
#include "sample_file.h"

enum AssignmentStates{
	ASSIGN_OFF,
	ASSIGN_UNUSED,
	ASSIGN_USED,

	ASSIGN_BLANK
};


uint8_t enter_assignment_mode(void);
uint8_t next_unassigned_sample(void);

void save_exit_assignment_mode(void);

void copy_sample(uint8_t dst_bank, uint8_t dst_sample, uint8_t src_bank, uint8_t src_sample);

// void set_sample_trim_start(Sample *s_sample, float coarse, float fine);
// void set_sample_trim_size(Sample *s_sample, float coarse);
void nudge_trim_start(Sample *s_sample, int32_t fine);
void nudge_trim_size(Sample *s_sample, int32_t fine);
void set_sample_gain(Sample *s_sample, float gain);

void exit_edit_mode(void);
void enter_edit_mode(void);

void save_undo_state(uint8_t bank, uint8_t samplenum);
void restore_undo_state(uint8_t bank, uint8_t samplenum);

#endif /* INC_EDIT_MODE_H */