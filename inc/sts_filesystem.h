/*
 * sts_filesystem.c - Bank and folder system for STS
 *
 * Author: Dan Green (danngreen1@gmail.com)
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

#include "ff.h"
#include "sample_file.h"
#include <stm32f4xx.h>

#define SYS_DIR "_STS.system"
#define SYS_DIR_SLASH "_STS.system/"

#define TMP_DIR "_tmp"
#define TMP_DIR_SLASH "_tmp/"

#define SAMPLELIST_FILE "__Sample List__.html"

#define SAMPLE_INDEX_FILE "sample_index.dat"
#define SAMPLE_BAK_FILE "sample_index-bak.dat"
#define SAMPLE_BOOTBAK_FILE "sample_index_boot-bak.dat"
#define RENAME_LOG_FILE "renamed_folders.txt"

#define RENAME_TMP_FILE "sts-renaming-queue.tmp"
#define ERROR_LOG_FILE "error-log.txt"
#define SETTINGS_FILE "settings.txt"

#define EOF_TAG "End of file"
#define EOF_PAD 10 // number of characters that can be left after EOF_TAG

#define MAX_FILES_IN_FOLDER 500
#define NO_MORE_AVAILABLE_FILES 0xFF

uint8_t new_filename(uint8_t bank, uint8_t sample_num, char *path);

//uint8_t get_banks_path(uint8_t bank, char *path);

void load_new_folders(void);
void load_missing_files(void);

uint8_t load_banks_by_color_prefix(void);
uint8_t load_banks_by_default_colors(void);
uint8_t load_banks_with_noncolors(void);

uint8_t load_all_banks(uint8_t force_reload);

uint8_t load_bank_from_disk(Sample *sample_bank, char *path_noslash);

uint8_t dir_contains_assigned_samples(char *path);

FRESULT check_sys_dir(void);
