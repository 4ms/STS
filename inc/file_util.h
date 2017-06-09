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

void str_cpy(char *dest, char *src);
uint32_t str_len(char* str);
uint32_t intToStr(uint32_t x, char *str, uint32_t d);
uint8_t str_cmp(char *a, char *b);
FRESULT get_next_dir(DIR *dir, char *parent_path, char *next_dir_path);
uint8_t str_startswith(const char *string, const char *prefix);


#endif /* INC_FILE_UTIL_H_ */
