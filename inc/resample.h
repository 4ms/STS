/*
 * resample.h
 *
 *  Created on: Nov 11, 2016
 *      Author: design
 */

#ifndef INC_RESAMPLE_H_
#define INC_RESAMPLE_H_

#include <stm32f4xx.h>

int32_t resample(float rs, uint32_t num_samples_out, int32_t last_samp, int32_t *in, int32_t *out);

#endif /* INC_RESAMPLE_H_ */
