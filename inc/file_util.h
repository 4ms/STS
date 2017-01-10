/*
 * file_util.h
 *
 *  Created on: Jan 3, 2017
 *      Author: design
 */

#ifndef INC_FILE_UTIL_H_
#define INC_FILE_UTIL_H_

#include <stm32f4xx.h>
#include "ff.h"

FRESULT find_next_ext_in_dir(DIR* dir, const char *ext, char *fname);

void str_cpy(char *dest, char *src);
uint32_t str_len(char* str);
uint32_t intToStr(uint32_t x, char *str, uint32_t d);


#endif /* INC_FILE_UTIL_H_ */
