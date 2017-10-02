/*
 * file_util.h
 *
 *  Created on: Jan 3, 2017
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>
#include "ff.h"

enum INIT_FIND_ALPHA_ACTIONS{
  FIND_ALPHA_DONT_INIT,
  FIND_ALPHA_INIT_FOLDER
};

FRESULT 	get_next_dir(DIR *dir, char *parent_path, char *next_dir_path);
FRESULT 	find_next_ext_in_dir(DIR* dir, const char *ext, char *fname);
FRESULT 	find_next_ext_in_dir_alpha(char* path, const char *ext, char *fname, enum INIT_FIND_ALPHA_ACTIONS do_init);

uint8_t 	is_wav(char *string);



