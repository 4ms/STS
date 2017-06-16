#include "globals.h"
#include "params.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "sample_file.h"
#include "bank.h"
#include "sts_fs_index.h"

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];
extern volatile uint32_t sys_tmr;

extern enum g_Errors g_error;
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 	flags[NUM_FLAGS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern FATFS FatFs;


FRESULT reload_sdcard(void)
{
	FRESULT res;

	res = f_mount(&FatFs, "", 1);
	if (res != FR_OK)
	{
		//can't mount
		g_error |= SDCARD_CANT_MOUNT;
		res = f_mount(&FatFs, "", 0);
		return(FR_DISK_ERR);
	}
	return (FR_OK);
}



//
//Go through all default bank folder names,
//and see if each one is an actual folder
//If so, open it up.
//
uint8_t load_banks_by_default_colors(void)
{
	uint8_t bank;
	char bankname[_MAX_LFN];
	uint8_t banks_loaded;

	banks_loaded=0;
	for (bank=0;bank<MAX_NUM_BANKS;bank++)
	{
		bank_to_color(bank, bankname);

		if (load_bank_from_disk(bank, bankname))
		{
			enable_bank(bank); 
			banks_loaded++;
		}
		else 
			disable_bank(bank);
	}
	return(banks_loaded);
}


uint8_t load_banks_by_color_prefix(void)
{
	uint8_t bank;
	char foldername[_MAX_LFN];
	char default_bankname[_MAX_LFN];
	uint8_t banks_loaded;

	FRESULT res;
	DIR rootdir;
	DIR testdir;

	uint8_t test_path_loaded=0;

	banks_loaded=0;
	
	rootdir.obj.fs = 0; //get_next_dir() needs us to do this, to reset it

	while (1)
	{
		//Find the next directory in the root folder
		res = get_next_dir(&rootdir, "", foldername);

		if (res != FR_OK) break; //no more directories, exit the while loop

		test_path_loaded = 0;

		//Check if folder contains any .wav files
		res = f_opendir(&testdir, foldername);
		if (res!=FR_OK) continue;
		if (find_next_ext_in_dir(&testdir, ".wav", default_bankname) != FR_OK) continue;

		//Go through all the default bank names and
		//see if the folder we found matches one exactly.
		//If it does, try another folder (we already should have loaded this bank in load_banks_by_default_colors())
		//This could be replaced with:
		//bank = color_to_bank(foldername);
		//if (bank!=NOT_FOUND)...
		for (bank=0;(!test_path_loaded && bank<MAX_NUM_BANKS);bank++)
		{
			bank_to_color(bank, default_bankname);
			if (str_cmp(foldername, default_bankname))
			{
				test_path_loaded=1;
				break;
			}
		}

		//Go through all the default bank names with numbers (>10)
		//see if the folder we found has a bank name as a prefix
		//This finds things like "Red-3 - TV Clips" (but not yet "Red - Movie Clips")
		for (bank=10;(!test_path_loaded && bank<MAX_NUM_BANKS);bank++)
		{
			bank_to_color(bank, default_bankname);

			if (str_startswith(foldername, default_bankname))
			{
				//Make sure the bank is not already being used
				if (!is_bank_enabled(bank))
				{
					//...and if not, then try to load it as a bank
					if (load_bank_from_disk(bank, foldername))
					{
						enable_bank(bank);
						banks_loaded++;
						test_path_loaded = 1;
						break; //continue with the next folder
					}
				}
			}
		}

		//Go through all the non-numbered default bank names (Red, White, Pink, etc...)
		//and see if this bank has one of them as a prefix
		//If the bank is a non-numbered bank name (that is, bank< 9. Example: "Red" or "Blue", but not "Red-9" or "Blue-4")
		//Then check to see if subsequent numbered banks are disabled (so check for Red 2, Red 3, Red 4)
		//This allows us to have "Red - Synth Pads/" become Red bank, and "Red - Drum Loops/" become Red-2 bank
		for (bank=0;(!test_path_loaded && bank<10);bank++)
		{
			bank_to_color(bank, default_bankname);

			if (str_startswith(foldername, default_bankname))
			{
				//We found a prefix for this folder
				//If the base color name bank is still empty, load it there...
				if (!is_bank_enabled(bank))
				{
					if (load_bank_from_disk(bank, foldername))
					{
						enable_bank(bank);
						banks_loaded++;
						test_path_loaded = 1;
						break; //continue with the next folder
					}
				}
				//...Otherwise try loading it into the next higher-numbered bank (Red-2, Red-3...)
				else
				{
					bank+=10;
					while (bank<MAX_NUM_BANKS)
					{
						if (!is_bank_enabled(bank))
						{
							if (load_bank_from_disk(bank, foldername))
							{
								enable_bank(bank);
								banks_loaded++;
								test_path_loaded = 1;
								break; //exit while loop and continue with the next folder
							}
						} 
						bank+=10;
					}
				}
			}
		}
	}

	return(banks_loaded);

}

uint8_t load_banks_with_noncolors(void)
{
	uint8_t bank;
	uint8_t len;
	char foldername[_MAX_LFN];
	char default_bankname[_MAX_LFN];
	uint8_t banks_loaded;
	uint8_t test_path_loaded;

	FRESULT res;
	DIR rootdir;
	DIR testdir;

	banks_loaded=0;
	
	rootdir.obj.fs = 0; //get_next_dir() needs us to do this, to reset it

	while (1)
	{
		//Find the next directory in the root folder
		res = get_next_dir(&rootdir, "", foldername);

		if (res != FR_OK) break; //no more directories, exit the while loop

		test_path_loaded = 0;

		//Open the folder
		res = f_opendir(&testdir, foldername);
		if (res!=FR_OK) continue;

		//Check if folder contains any .wav files
		if (find_next_ext_in_dir(&testdir, ".wav", default_bankname) != FR_OK) continue;

		//See if it starts with a default bank color string
		for (bank=0;bank<10;bank++)
		{
			bank_to_color(bank, default_bankname);
			if (str_startswith(foldername, default_bankname))
			{
				test_path_loaded = 1;
				break;
			}
		}

		//Since we didn't find a bank for this folder yet, just put it in the first available bank

		for (bank=0;(!test_path_loaded && bank<MAX_NUM_BANKS);bank++)
		{
			if (!is_bank_enabled(bank))
			{
				if (load_bank_from_disk(bank, foldername))
				{
					enable_bank(bank);
					banks_loaded++;

					//Rename the folder with the bank name as a prefix
					len=bank_to_color(bank, default_bankname);
					default_bankname[len++] = ' ';
					default_bankname[len++] = '-';
					default_bankname[len++] = ' ';
					str_cpy(&(default_bankname[len]), foldername);

					f_rename(foldername, default_bankname);

					load_bank_from_disk(bank, default_bankname);

					break; //continue with the next folder
				}
			}
		}
	}

	return(banks_loaded);
}


//
//Tries to open the folder bankpath and load 10 files into samples[][]
//into the specified bank
//
//TODO: Sort the folder contents alphanbetically
//
//Returns number of samples loaded (0 if folder not found, and sample[bank][] will be cleared)
//
uint8_t load_bank_from_disk(uint8_t bank, char *bankpath)
{
	uint32_t i;
	uint32_t sample_num;

	FIL temp_file;
	FRESULT res;
	DIR dir;

	uint8_t path_len;
	char path[_MAX_LFN];
	char filename[256];


	for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
		clear_sample_header(&samples[bank][i]);

	//Copy bankpath into path so we can work with it
	if (bankpath[0] != '\0')	str_cpy(path, bankpath);
	else						return 0;

	res = f_opendir(&dir, path);
	if (res!=FR_OK) return 0;

	//Append '/' to path
	path_len = str_len(path);
	path[path_len++]='/';
	path[path_len]='\0';

	sample_num=0;
	filename[0]=0;

	while (sample_num < NUM_SAMPLES_PER_BANK)
	{
		//Find first .wav file in directory
		res = find_next_ext_in_dir(&dir, ".wav", filename);
		if (res!=FR_OK) break; 		//Stop if no more files found in directory
		if (str_len(filename) > (_MAX_LFN - path_len - 2)) continue; //Skip if filename is too long, can't use it

		//Append filename onto path
		str_cpy(&(path[path_len]), filename);

		//Open the file
		res = f_open(&temp_file, path, FA_READ);
		f_sync(&temp_file);

		if (res==FR_OK)
		{
			//Load the sample header info into samples[][]
			res = load_sample_header(&samples[bank][sample_num], &temp_file);

			if (res==FR_OK)
			{
				//Set the filename (full path)
				str_cpy(samples[bank][sample_num++].filename, path);
//				preloaded_clmt[sample_num] = load_file_clmt(&temp_file);
			}
		}
		f_close(&temp_file);
	}
	f_closedir(&dir);

	return(sample_num);
}


uint8_t load_all_banks(uint8_t force_reload)
{
	FRESULT res;

	if (!force_reload)
		force_reload = load_sampleindex_file();

	if (!force_reload) //sampleindex file was ok
	{
		check_enabled_banks();
//		check_sample_headers(); //TODO: Checks that all files in samples[][] exist, and their header info matches
	}
	else //sampleindex file was not ok, or we requested to force a reload from disk
	{
		//First pass: load all the banks that have default folder names
		load_banks_by_default_colors();

		//Second pass: look for folders that start with a bank name, example "Red - My Samples/"
		load_banks_by_color_prefix();

		//Third pass: go through all remaining folders and try to assign them to banks
		load_banks_with_noncolors();

		res = write_sampleindex_file();
		if (res) {g_error |= CANNOT_WRITE_INDEX; check_errors(); return 0;}
	}

	//Verify the channels are set to enabled banks, and correct if necessary
	if (!is_bank_enabled(i_param[0][BANK])) 
		i_param[0][BANK] = next_enabled_bank(i_param[0][BANK]);

	if (!is_bank_enabled(i_param[1][BANK])) 
		i_param[1][BANK] = next_enabled_bank(i_param[1][BANK]);

	return 1;
}


uint8_t new_filename(uint8_t bank, uint8_t sample_num, char *path)
{
	uint8_t i;
	FRESULT res;
	DIR dir;
	uint32_t sz;

	//
	//Figure out the folder to put a new recording in:
	//1st choice: Same folder as the sample already existing in the current REC bank/sample
	//2nd choice: Same folder as the first existing sample in the current REC bank
	//3rd choice: default folder name for current REC bank
	//

	path[0]=0;

	//1st) Check for the current REC sample/bank folder:
	//If file name is not blank, Split the filename at the last '/'
	if (samples[bank][sample_num].filename[0])
	{
		if (str_rstr(samples[bank][sample_num].filename,'/',path)!=0)
			path[str_len(path) - 1] = 0; // truncate the last '/'
		else
			path[0]=0;
	}
	//2nd) Check all the samples in REC bank
	if (!path[0])
	{
		for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
			if (samples[bank][i].filename[0])
			{
				if (str_rstr(samples[bank][i].filename,'/',path)!=0)
					path[str_len(path) - 1] = 0; // truncate the last '/'
				else
					path[0]=0;
			}
	}

	//3rd) Use the default bank name if the above methods failed
	if (!path[0])
		bank_to_color(bank, path);

	//Open the directory
	res = f_opendir(&dir, path);

	//If it doesn't exist, create it
	if (res==FR_NO_PATH)
		res = f_mkdir(path);

	//If we got an error opening or creating a dir
	//try reloading the SDCard, then opening the dir (and creating if needed)
	if (res!=FR_OK)
	{
		res = reload_sdcard();

		if (res==FR_OK)
		{
			res = f_opendir(&dir, path);

			if (res==FR_NO_PATH) 
				res = f_mkdir(path);
		}
	}

	if (res!=FR_OK)
	{
		return (FR_INT_ERR); //fail
	}

	//
	//Create the file name
	//
	//str_cpy(sample_fname_now_recording, path);
	sz = str_len(path);

	path[sz++] = '/';
	sz += intToStr(sample_num, &(path[sz]), 2);
	path[sz++] = '-';
	sz += intToStr(sys_tmr, &(path[sz]), 0);
	path[sz++] = '.';
	path[sz++] = 'w';
	path[sz++] = 'a';
	path[sz++] = 'v';
	path[sz++] = 0;

	return (FR_OK);

}


// Enhanced f_open
// takes file path as input
// tries to open file with given path
// then tries in other paths if failed with given path
// returns flag indicating:
//		- 0: path is correct
//		- 1: path was incorrect, and corrected
//		- 2: path couldn't be corrected
uint8_t fopen_checked(FIL *fp, char* filepath)
{
	uint8_t 	flag;
	FRESULT 	res, res_dir;
	char* 		filepath_attempt;
	char*		file_only; 
	char* 		col;
	char* 		removed_path;
	char 		file_only_ptr[_MAX_LFN+1];
	char 		removed_path_ptr[_MAX_LFN+1];
	uint8_t 	try_folders=1;
	uint8_t		try_root=1;
	uint8_t 	i=0;

	char 		foldername[_MAX_LFN];
	DIR 		rootdir;

	res = f_open(fp, filepath, FA_READ);
	f_sync(fp);

	// if file loaded properly, move on
	if (res==FR_OK) return(0);
	
	// otherwise...
	else
	{
		// extract file name
		file_only 		= file_only_ptr;
		removed_path 	= removed_path_ptr;
		file_only = str_rstr(filepath, '/', removed_path);

		// Until file opens properly
		while (res!=FR_OK)
		{
			// clear filepath
			filepath_attempt[0]='\0';

			// try every folder in the folder tree (alphabetically?)
			if (try_folders)
			{
				rootdir.obj.fs = 0; //get_next_dir() needs us to do this, to reset it

				// Check for the next folder and set it as the file path
				res_dir = get_next_dir(&rootdir, "", filepath_attempt);

				// if another folder was found
				if (res_dir == FR_OK)
				{
					// try opening file from folder
					res = f_open(fp, filepath_attempt, FA_READ);
					f_sync(fp);					
					
					// if file was found
					// return "1: path was incorrect, and corrected" 
					// exit function 
					if(res == FR_OK) return(1);
				}

				// Otherwise, stop checking folders 
				else try_folders=0; 
			}


			// ... then try root path
			else if (try_root)
			{
				// update filepath
				str_cat(filepath_attempt, "/", file_only);
				
				// try to open current file path
				res = f_open(fp, filepath_attempt, FA_READ);
				f_sync(fp);			

				// if file was found
				// return "1: path was incorrect, and corrected" 
				// exit function 
				if 		(res == FR_OK) return(1);
				
				// if file wasn't found 
				else if (res != FR_OK)
				{

					// close file
					fclose(fp);

					// return "2: path couldn't be corrected"
					// exit function 
					return(2);
				} 
			}
		}
	}

	return(0);
}
