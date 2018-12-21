/*
 * sts_sampleindex.c - interface to the sample index file
 *
 * Author: Hugo Paris (hugoplho@gmail.com), Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
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

