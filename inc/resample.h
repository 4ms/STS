/*
 * resample.h
 *
 *  Created on: Nov 11, 2016
 *      Author: design
 */

#ifndef INC_RESAMPLE_H_
#define INC_RESAMPLE_H_

#include <stm32f4xx.h>

int32_t resample(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out, int32_t last_sample);
float resample_nom1(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out, float start_fpos);

int32_t nosample(float rs, uint32_t num_samples_out, int32_t *in, int32_t *out);

void resample_read(float rs, uint32_t *play_buff_out_addr, uint32_t limit_addr, uint32_t buff_len, uint8_t chan, float *fractional_pos, int32_t *out);

#endif /* INC_RESAMPLE_H_ */
