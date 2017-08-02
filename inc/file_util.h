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


FRESULT 	get_next_dir(DIR *dir, char *parent_path, char *next_dir_path);
FRESULT 	find_next_ext_in_dir(DIR* dir, const char *ext, char *fname);
FRESULT 	find_next_ext_in_dir_alpha(char* path, const char *ext, char *fname);

void 		str_cpy(char *dest, char *src);
void 		str_cat(char *dest, char *srca, char *srcb);
uint32_t 	str_len(char* str);
uint8_t 	str_split(char *string, char split_char, char *before_split, char *after_split);
char *		str_rstr(char *string, char split_char, char *before_split);
uint8_t 	is_wav(char *string);
void 		str_tok(char *in_string, char find, char *tokk);
uint32_t 	str_xt_int(char *string);
uint32_t 	intToStr(uint32_t x, char *str, uint32_t d);
char		upper(char a);
char 		lower(char a);
void 		str_to_upper(char* str_in, char* str_up);
void 		str_to_lower(char* str_in, char* str_lo);
uint8_t 	str_cmp(char *a, char *b);
uint8_t 	str_cmp_nocase(char *a, char *b);
int 		str_cmp_alpha(char *a, char *b);
uint8_t 	str_startswith(const char *string, const char *prefix);
uint8_t 	str_startswith_nocase(const char *string, const char *prefix);
char *		str_rstr_x(char *string, char splitchar, char *path);
uint32_t 	str_pos(char needle, char *haystack);
uint8_t 	str_found(char* str, char* find);



#endif /* INC_FILE_UTIL_H_ */
