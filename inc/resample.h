/*
 * resample.h
 *
 *  Created on: Nov 11, 2016
 *      Author: design
 */

#ifndef INC_RESAMPLE_H_
#define INC_RESAMPLE_H_

#include <stm32f4xx.h>

void resample_read16(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, float *fractional_pos, int32_t *out);
void resample_read24(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, int32_t *out);
void resample_read32f(float rs, CircularBuffer* buf, uint32_t buff_len, enum Stereo_Modes stereomode, uint8_t block_align, uint8_t chan, int32_t *out);

#endif /* INC_RESAMPLE_H_ */
