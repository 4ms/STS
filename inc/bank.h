/*
 * bank.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>
#include "ff.h"
#include "sample_file.h"

void bump_down_banks(uint8_t bank);

void copy_bank(Sample *dst, Sample *src);

uint8_t get_bank_path(uint8_t bank, char *path);

uint8_t find_filename_in_bank(uint8_t bank, char *filename);
uint8_t find_filename_in_all_banks(uint8_t bank, char *filename);

uint8_t bank_to_color_string(uint8_t bank, char *color);
uint8_t bank_to_color(uint8_t bank, char *color);
uint8_t color_to_bank(char *color);

uint8_t next_bank(uint8_t bank);
uint8_t next_enabled_bank(uint8_t bank);
uint8_t next_enabled_bank_0xFF(uint8_t bank);
uint8_t next_disabled_bank(uint8_t bank);

uint8_t prev_enabled_bank(uint8_t bank);
uint8_t prev_enabled_bank_0xFF(uint8_t bank);
uint8_t prev_disabled_bank(uint8_t bank);

void 	check_enabled_banks(void);
uint8_t is_bank_enabled(uint8_t bank);
void 	enable_bank(uint8_t bank);
void 	disable_bank(uint8_t bank);

uint8_t get_bank_color_digit(uint8_t bank);
uint8_t get_bank_blink_digit(uint8_t bank);


//void create_bank_path_index(void);
void init_banks(void);


