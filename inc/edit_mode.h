/*
 * edit_mode.h
 *
 *  Created on: Feb 27, 2017
 *      Author: design
 */

#ifndef INC_EDIT_MODE_H
#define INC_EDIT_MODE_H
#include <stm32f4xx.h>

void enter_assignment_mode(void);
void save_exit_assignment_mode(void);
void cancel_exit_assignment_mode(void);
uint8_t load_samples_to_assign(uint8_t bank);

void next_unassigned_sample(void);
uint8_t find_current_sample_in_assign(Sample *s);

void set_sample_trim_start(Sample *s_sample, float st);
void set_sample_trim_end(Sample *s_sample, float en);
void set_sample_trim_size(Sample *s_sample, float coarse, float fine);

void set_sample_gain(Sample *s_sample, float gain);


#endif /* INC_EDIT_MODE_H */
