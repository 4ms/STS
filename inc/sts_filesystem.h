/*
 * sts_filesystem.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#ifndef INC_STS_FILESYSTEM_H_
#define INC_STS_FILESYSTEM_H_
#include <stm32f4xx.h>
#include "ff.h"


FRESULT reload_sdcard(void);

uint8_t new_filename(uint8_t bank, uint8_t sample_num, char *path);


uint8_t get_banks_path(uint8_t sample, uint8_t bank, char *path);


uint8_t load_banks_by_color_prefix(void);
uint8_t load_banks_by_default_colors(void);
uint8_t load_banks_with_noncolors(void);

FRESULT load_all_banks(uint8_t force_reload);

uint8_t load_bank_from_disk(uint8_t bank, char *bankpath);

uint8_t fopen_checked(FIL *fp, char* folderpath, char* filename);

#endif /* INC_STS_FILESYSTEM_H_ */
