/*
 * sts_filesystem.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#ifndef INC_BANK_H
#define INC_BANK_H
#include <stm32f4xx.h>
#include "ff.h"
#include "sample_file.h"


uint8_t bank_to_color_string(uint8_t bank, char *color);
uint8_t bank_to_color(uint8_t bank, char *color);
uint8_t next_enabled_bank(uint8_t bank);
void check_enabled_banks(void);
uint8_t is_bank_enabled(uint8_t bank);
void enable_bank(uint8_t bank);
void disable_bank(uint8_t bank);
uint8_t color_to_bank(char *color);



#endif /* INC_BANK_H */
