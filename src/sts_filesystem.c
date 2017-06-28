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
#include "sts_fs_renaming_queue.h"

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];
extern char index_bank_path[MAX_NUM_BANKS][_MAX_LFN];

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
	uint8_t existing_prefix_len, len;


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

		//See if the folder we found is a default bank name
		//This would mean it's already loaded (with load_banks_by_default_colors())
		if (color_to_bank(foldername) < MAX_NUM_BANKS)
			test_path_loaded=1;

		//Go through all the numbered bank names (Red-1, Pink-1, etc...)
		//see if the folder we found has a numbered bank name as a prefix
		//This finds things like "Red-3 - TV Clips" (but not "Red - Movie Clips")
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
		//If the bank is a non-numbered bank name (ie bank< 9, for example: "Red" or "Blue", but not "Red-9" or "Blue-4")
		//Then check to see if subsequent numbered banks of the same color are available (so check for Red-2, Red-3, Red-4)
		//This allows us to have "Red - Synth Pads/" become Red bank, and "Red - Drum Loops/" become Red-2 bank
		//
		for (bank=0;(!test_path_loaded && bank<10);bank++)
		{
			existing_prefix_len = bank_to_color(bank, default_bankname);

			if (str_startswith(foldername, default_bankname))
			{
				//We found a prefix for this folder
				//If the base color name bank is still empty, load it there...
				//E.g. if we found "Red - Synth Pads" and Red bank is still available, load it there
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
					bank+=10; //add 10 to go to the next numbered bank of the same color
					while (bank<MAX_NUM_BANKS)
					{
						if (!is_bank_enabled(bank))
						{
							if (load_bank_from_disk(bank, foldername))
							{
								enable_bank(bank);
								banks_loaded++;
								test_path_loaded = 1;

								//Queue the renaming of the bank's folder
								//Create the new name by removing the existing color prefix, and adding the actual bank prefix
								//e.g. "White - More Loops" becomes "White-3  - More Loops"
								//
								//In the special case where we have two banks that begin with a numbered bank prefix:
								//e.g. "Red-2 Drums" and "Red-2 Synths"
								//Then the first one found (Red-2 Drums) will go into Red-2, according to the previous for-loop.
								//The second one will be found in this for-loop because it is a folder name prefixed by "Red".
								//Thus it will think "-2 Synths" is the suffix. Assuming Red-3 is available,
								//the renaming will rename it to "Red-3" + "-2 Synths" = "Red-3 2 Synths" (dash is trimmed)
								//Should we detect a suffix as such and remove it? Or leave it to help the user know what it used to be?

								len=bank_to_color(bank, default_bankname);
								default_bankname[len++] = ' ';
								default_bankname[len++] = '-';
								default_bankname[len++] = ' ';
								
								//trim spaces and dashes off the beginning of the suffix
								while(foldername[existing_prefix_len]==' ' || foldername[existing_prefix_len]=='-') existing_prefix_len++;

								str_cpy(&(default_bankname[len]), &(foldername[existing_prefix_len]));

								append_rename_queue(bank, foldername, default_bankname);

								break; //exit inner while loop and continue with the next folder
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
					test_path_loaded = 1;

					//Queue the renaming of the bank's folder
					//Create the new name by adding the bank name as a prefix
					//e.g. "Door knockers" becomes "Cyan-3 - Door knockers"
					len=bank_to_color(bank, default_bankname);
					default_bankname[len++] = ' ';
					// if (bank<10) 
					// {
						default_bankname[len++] = '-';
						default_bankname[len++] = ' ';
					// }
					str_cpy(&(default_bankname[len]), foldername);

					append_rename_queue(bank, foldername, default_bankname);

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
	FRESULT queue_valid;
	uint8_t res_bak;

	if (!force_reload)
		force_reload = load_sampleindex_file(INDEX_FILE, MAX_NUM_BANKS);

	if (!force_reload) //sampleindex file was ok
	{	
		// check which samples are loaded
		// (looks for filename in samples strucure)
		check_enabled_banks();

	}

	else //sampleindex file was not ok, or we requested to force a reload from disk
	{

		// Backup the sampleindex file if it exists
		res_bak = backup_sampleindex_file();
		if (res_bak)
		{
			// ERRORLOG: sample index couldn't be backed up
		}
		
		//initialize the renaming queue
		queue_valid = clear_renaming_queue();

		//First pass: load all the banks that have default folder names
		load_banks_by_default_colors();

		//Second pass: look for folders that start with a bank name, example "Red - My Samples/"
		load_banks_by_color_prefix();

		//Third pass: go through all remaining folders and try to assign them to banks
		load_banks_with_noncolors();

		//process the folders/banks that need to be renamed
		if (queue_valid==FR_OK) 
			process_renaming_queue();
	}


	// Write samples struct to index
	// ... so sample info gets updated with latest .wave header content
	res = write_sampleindex_file();

	//Create a handy array of all the bank paths
	//TODO: this could be done in write_sampleindex_file() if we find it useful
	create_bank_path_index();

	// check if there was an error writing to index file
	// ToDo: push this to error log
	if (res) {g_error |= CANNOT_WRITE_INDEX; check_errors(); return 0;}


	//Verify the channels are set to enabled banks, and correct if necessary
	if (!is_bank_enabled(i_param[0][BANK])) 
		i_param[0][BANK] = next_enabled_bank(i_param[0][BANK]);

	if (!is_bank_enabled(i_param[1][BANK])) 
		i_param[1][BANK] = next_enabled_bank(i_param[1][BANK]);

	return 1;
}


//
//Tries to figure out the bank path
//
uint8_t get_banks_path(uint8_t bank, char *path)
{
	//Get path to bank's folder
	//

	str_cpy(path, index_bank_path[ bank ]);
	path[str_len(path)-1] = 0; //cut off the '/'

	//First try using the path from the current sample
	//
	// if (!(str_rstr_x(samples[ bank ][ sample ].filename, '/', path)))
	// {
	// 	//if that doesn't have a path, go through the whole bank until we find a path
	// 	sample = 0;
	// 	while (!(str_rstr_x(samples[ bank ][ sample ].filename, '/', path)))
	// 	{
	// 		if (++sample >= NUM_SAMPLES_PER_BANK)
	// 		{
	// 			//No path found (perhaps no samples in the bank?)
	// 			//Set path to the default bank name
	// 			bank_to_color(bank, path);
	// 			return(0); //not found
	// 		}
	// 	}
	// }
	if (path[0])
		return(1);//found
	else
		return(0);
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
// then tries in alternate paths if failed with given path
// returns flag indicating:
//		- 0: path is correct
//		- 1: path was incorrect, and corrected
//		- 2: path couldn't be corrected
uint8_t fopen_checked(FIL *fp, char* folderpath, char* filename)
{
	DIR 		rootdir;
	FRESULT 	res, res_dir;
	uint8_t 	flag;
	char 		fullpath[_MAX_LFN+1];	
	// char 		fname[_MAX_LFN+1];	
	char 		dumpedpath[_MAX_LFN+1];	
	char* 		fileonly;
	char 		fileonly_ptr[_MAX_LFN+1];	
	uint8_t 	try_folders	= 1;
	uint8_t		try_root	= 1;
	uint8_t 	i = 0;
	char*		src;

	// str_cpy(fname,filename);

	// OPEN FILE FROM PROVIDED FOLDER	

	// initialize full path to file
	str_cat(fullpath, folderpath, filename);

	// try to open full path
	res = f_open(fp, fullpath, FA_READ);
	f_sync(fp);

	// if file loaded properly, move on
	if (res==FR_OK)
	{					
		str_cpy(filename, fullpath); 
		return(0);
	}

	// otherwise...
	else
	{
		// close file
		f_close(fp);
		
		// remove first slash in path if there is one
		// if(filename[0]=='\\')
		// {
		// 	str_cpy(src, filename);
		// 	 *src++;
		// 	  while(*src!=0){
		// 	  	*filename++ = *src++;
		// 	  }
		// 	  *filename=0;
		// }



		// OPEN FILE FROM ROOT - AS WRITTEN IN INDEX
		// try opening file supposing its path is in the name field
		res = f_open(fp, filename, FA_READ);
		f_sync(fp);	

		// if file was found
		// return 0: path is correct (we don't want to rewrite index for that) 
		// exit function 
		if (res==FR_OK) return(0);
				
	
		// OPEN FILE FROM ROOT - FILE.WAV ONLY
		// ...Otherwise, Remove any existing path before wav file name
		fileonly = fileonly_ptr;
		fileonly = str_rstr(filename, '/', dumpedpath); // FixMe: we can prob dump in folderpath since it gets erased afterwards

		// if there was a path before filename
		if (fileonly!=0) 
		{
	
			// try opening file name only, from root
			res = f_open(fp, fileonly, FA_READ);
			f_sync(fp);	
			
			// if file was found
			// return 0: path is correct (we don't want to rewrite index for that) 
			// exit function 
			if (res==FR_OK) return(0);
		}

		// if file wasn't found 
		rootdir.obj.fs = 0; //get_next_dir() needs us to do this, to reset it

		// OPEN FILE FROM SD CARD FOLDERS
		// Until file opens properly
		while (res!=FR_OK)
		{
			
			// try every folder in the folder tree
			// ... faster this way (don't have to check for alternate spellings/upper-lower case etc...)
			// ... there shouldn't be any duplicate files within the folder tree anyway
			// ... since they're handled with the sample index

			// clear folder path
			folderpath[0]='\0';

			// find next folder path
			res_dir = get_next_dir(&rootdir, "", folderpath);

			// if another folder was found
			if (res_dir == FR_OK)
			{	
				// concatenate folder path and file name
				str_cat	(folderpath, folderpath, "/");
				str_cat	(fullpath, folderpath, filename);

				// try opening file from new path
				res = f_open(fp, fullpath, FA_READ);
				f_sync(fp);					
				
				// if file was found
				// return "1: path was incorrect, and corrected" 
				// exit function 
				if(res == FR_OK) 
				{
					str_cpy(filename, fullpath); 
					return(1);
				}

				// ... otherwise, close file
				else f_close(fp);
			}

			// Otherwise, stop searching 
			else
			{
				// close file
				f_close(fp);

				// return "2: path couldn't be corrected"
				// exit function 
				return(2);
			}  
		}
	}
}
