/*
 * sample_file.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#ifndef INC_SAMPLE_FILE_H_
#define INC_SAMPLE_FILE_H_
#include <stm32f4xx.h>
#include "ff.h"
#include "sts_filesystem.h"


uint8_t load_sample_header(Sample *s_sample, FIL *sample_file);
void clear_sample_header(Sample *s_sample);
FRESULT reload_sample_file(FIL *fil, Sample *s_sample);
FRESULT create_linkmap(FIL *fil, uint8_t chan);


#endif /* INC_SAMPLE_FILE_H_ */
