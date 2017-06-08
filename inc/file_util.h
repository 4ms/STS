/*
 * file_util.h
 *
 *  Created on: Jan 3, 2017
 *      Author: design
 */

#ifndef INC_FILE_UTIL_H_
#define INC_FILE_UTIL_H_

#include <stm32f4xx.h>


void 		str_cpy(char *dest, char *src);
uint32_t 	str_len(char* str);
char * 		str_rstr(char *string, char find, char *path);
void *		str_tok(char *string, char find);
uint32_t 	str_xt_int(char *string);
uint32_t 	intToStr(uint32_t x, char *str, uint32_t d);
uint8_t 	str_cmp(char *a, char *b);


#endif /* INC_FILE_UTIL_H_ */
