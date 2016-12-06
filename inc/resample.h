/*
 * resample.h
 *
 *  Created on: Nov 11, 2016
 *      Author: design
 */

#ifndef INC_RESAMPLE_H_
#define INC_RESAMPLE_H_

#include <stm32f4xx.h>

void resample_read(float rs, uint32_t *play_buff_out_addr, uint32_t limit_addr, uint32_t buff_len, uint8_t chan, float *fractional_pos, int32_t *out);

#endif /* INC_RESAMPLE_H_ */
