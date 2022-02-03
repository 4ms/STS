/*
 * edit_mode.h: Edit Sample Mode
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

#include "sample_file.h"
#include <stm32f4xx.h>

enum AssignmentStates {
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
