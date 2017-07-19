/*
 * sts_filesystem.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#ifndef INC_STS_FILESYSTEM_H_
#define INC_STS_FILESYSTEM_H_
#include <stm32f4xx.h>
#include "ff.h"
#include "sample_file.h"

#define SYS_DIR 			"_STS.system"
#define SYS_DIR_SLASH		"_STS.system/"
#define SAMPLELIST_FILE		"sample_list.html"
#define SAMPLE_INDEX_FILE	"sample_index.dat"
#define SAMPLE_BAK_FILE		"sample_index-bak.dat"
#define SAMPLE_BOOTBAK_FILE	"sample_index_boot-bak.dat"
#define RENAME_LOG_FILE		"renamed_folders.txt"
#define RENAME_TMP_FILE		"sts-renaming-queue.tmp"
#define ERROR_LOG_FILE		"error-log.txt"
#define SETTINGS_FILE		"settings.txt"

#define EOF_TAG				"End of file"
#define EOF_PAD				10 				// number of characters that can be left after EOF_TAG

FRESULT reload_sdcard(void);

uint8_t new_filename(uint8_t bank, uint8_t sample_num, char *path);


//uint8_t get_banks_path(uint8_t bank, char *path);

void load_new_folders(void);
void load_missing_files(void);

uint8_t load_banks_by_color_prefix(void);
uint8_t load_banks_by_default_colors(void);
uint8_t load_banks_with_noncolors(void);

FRESULT load_all_banks(uint8_t force_reload);

uint8_t load_bank_from_disk(Sample *sample_bank, char *bankpath);

uint8_t dir_contains_assigned_samples(char *path);

uint8_t fopen_checked(FIL *fp, char* folderpath, char* filename);
FRESULT check_sys_dir(void);

#endif /* INC_STS_FILESYSTEM_H_ */
