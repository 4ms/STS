/*
 * resample.h
 *
 *  Created on: Nov 11, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

void resample_read16_left(float rs, CircularBuffer* buf, uint32_t buff_len, uint8_t block_align, uint8_t chan, int32_t *out);
void resample_read16_right(float rs, CircularBuffer* buf, uint32_t buff_len, uint8_t block_align, uint8_t chan, int32_t *out);
void resample_read16_avg(float rs, CircularBuffer* buf, uint32_t buff_len, uint8_t block_align, uint8_t chan, int32_t *out);




