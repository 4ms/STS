/*
 * index.h
 *
 *  Created on: Jun 13, 2017
 *      Author: Hugo Paris (hugoplho@gmail.com)
 */

#pragma once

#include <stm32f4xx.h>
#include "ff.h"

#define USE_BACKUP_FILE 1
#define USE_INDEX_FILE  0
#define ALL_BANKS MAX_NUM_BANKS

#define	SAMPLE_SLOT 1
#define	PLAY_START	2
#define	PLAY_SIZE	3
#define	PLAY_GAIN	4

#define	PLAYDATTAG_SLOT 	"- sample slot"
#define	PLAYDATTAG_START 	"- play start"
#define	PLAYDATTAG_SIZE 	"- play size"
#define	PLAYDATTAG_GAIN 	"- play gain"

FRESULT write_sampleindex_file(void);
uint8_t write_samplelist(void);
uint8_t index_write_wrapper(void);
FRESULT backup_sampleindex_file(void);
uint8_t load_sampleindex_file(uint8_t use_backup, uint8_t banks);

uint8_t check_sampleindex_valid(char *indexfilename);

