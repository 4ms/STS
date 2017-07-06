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

#define USE_BACKUP_FILE 1
#define USE_INDEX_FILE  0
#define ALL_BANKS MAX_NUM_BANKS

FRESULT write_sampleindex_file(void);
uint8_t write_samplelist(void);
uint8_t index_write_wrapper(void);
FRESULT backup_sampleindex_file(void);
uint8_t load_sampleindex_file(uint8_t use_backup, uint8_t banks);

#endif /* INC_STS_FS_INDEX_H_ */