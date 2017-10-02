/*
 * edit_mode.h
 *
 *  Created on: Feb 27, 2017
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>
#include "sample_file.h"

enum AssignmentStates{
	ASSIGN_OFF,
	ASSIGN_IN_FOLDER,
	ASSIGN_UNUSED_IN_ROOT,
	ASSIGN_UNUSED_IN_FS,
	ASSIGN_USED,

	ASSIGN_BLANK
};


uint8_t enter_assignment_mode(void);
void exit_assignment_mode(void);

void do_assignment(uint8_t direction);

uint8_t next_unassigned_sample(void);
uint8_t next_assigned_sample(void);

FRESULT init_unassigned_scan(Sample *s_sample, uint8_t samplenum, uint8_t banknum);
void init_assigned_scan(void);


void copy_sample(uint8_t dst_bank, uint8_t dst_sample, uint8_t src_bank, uint8_t src_sample);

void nudge_trim_start(Sample *s_sample, int32_t fine);
void nudge_trim_size(Sample *s_sample, int32_t fine);
void set_sample_gain(Sample *s_sample, float gain);

void exit_edit_mode(void);
void enter_edit_mode(void);

void save_undo_state(uint8_t bank, uint8_t samplenum);
uint8_t restore_undo_state(uint8_t bank, uint8_t samplenum);
