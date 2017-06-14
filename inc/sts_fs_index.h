/*
 * index.h
 *
 *  Created on: Jun 13, 2017
 *      Author: Hugo Paris (hugoplho@gmail.com)
 */

#ifndef INC_STS_FS_INDEX_H_
#define INC_STS_FS_INDEX_H_
#include <stm32f4xx.h>
#include "ff.h"

uint8_t load_sampleindex_file(void);
uint8_t write_sampleindex_file(void);

#endif /* INC_STS_FS_INDEX_H_ */