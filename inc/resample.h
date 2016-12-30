/*
 * resample.h
 *
 *  Created on: Nov 11, 2016
 *      Author: design
 */

#ifndef INC_RESAMPLE_H_
#define INC_RESAMPLE_H_

#include <stm32f4xx.h>

#define MAX_RS 4

//void resample_read16(float rs, uint32_t input_buff_addr, uint32_t limit_addr, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, float *fractional_pos, int32_t *out);
void resample_read16(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, float *fractional_pos, int32_t *out);

#endif /* INC_RESAMPLE_H_ */
