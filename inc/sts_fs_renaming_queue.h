/*
 * sts_fs_renaming_queue.h
 *
 *  Created on: June 21, 2017
 *      Author: Dan Green (danngreen1@gmail.com)
 */

#pragma once

#include <stm32f4xx.h>
#include "ff.h"

FRESULT clear_renaming_queue(void);
FRESULT append_rename_queue(uint8_t bank, char *orig_name, char *new_name);
FRESULT process_renaming_queue(void);


