/*
 * sts_filesystem.c
 *
 * Startup procedure: All button LEDs turn colors to indicate present operation:
 * Load index file: some color representing minor version number
 * Look for missing files and new folders: ORANGE
 * --Or: No index found, create all new banks from folders: WHITE
 * Write index file: MAGENTA
 * Write html file: LAVENDER
 * Done: OFF
 *
 *
 */

#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "str_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "sample_file.h"
#include "bank.h"
#include "sts_fs_index.h"
#include "sts_fs_renaming_queue.h"
#include "res/LED_palette.h"

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];
//extern char index_bank_path[MAX_NUM_BANKS][_MAX_LFN];

extern volatile uint32_t 	sys_tmr;

extern enum g_Errors 		g_error;
extern uint8_t				i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 				flags[NUM_FLAGS];
extern uint8_t 				global_mode[NUM_GLOBAL_MODES];
extern FATFS 				FatFs;

CCMDATA Sample				test_bank[NUM_SAMPLES_PER_BANK];


FRESULT check_sys_dir(void)
{
	FRESULT res;
	DIR	dir;

	res = f_opendir(&dir, SYS_DIR);

	//If it doesn't exist, create it
	if (res==FR_NO_PATH)
		res = f_mkdir(SYS_DIR);

	//If we got an error opening or creating a dir
	//try reloading the SDCard, then opening the dir (and creating if needed)
	if (res!=FR_OK)
	{
		res = reload_sdcard();

		if (res==FR_OK)
		{
			res = f_opendir(&dir, SYS_DIR);

			if (res==FR_NO_PATH)
				res = f_mkdir(SYS_DIR);
		}
	}

	if (res!=FR_OK)
	{
		return (FR_INT_ERR); //fail
	}

	return(FR_OK);
}

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


//Go through all enabled banks, looking for empty sample slots
//Search within the bank's folder, looking for a file to fill the slot
//
void load_empty_slots(void)
{
	uint8_t	bank, samplenum;
	char 	bankpath[_MAX_LFN+1];
	char 	bankpath_noslash[_MAX_LFN+1];
	char	filename[_MAX_LFN+1];
	char	fullpath[_MAX_LFN+1];

	FIL			temp_file;
	FRESULT		res=FR_OK;

	for (bank=0;bank<MAX_NUM_BANKS;bank++)												//Scan samples[][] for blank filenames
	{
		if (!is_bank_enabled(bank)) continue;											//Skip disabled banks

		if (!get_bank_path(bank, bankpath))												//Get the bank's predominant path
			{disable_bank(bank); continue;}												// Disable and skip banks with no samples

		str_cpy(bankpath_noslash, bankpath);
		trim_slash(bankpath_noslash);

		for(samplenum=0;samplenum<NUM_SAMPLES_PER_BANK;samplenum++)
		{
			if (!samples[bank][samplenum].filename[0])									//For each empty slot:
			{
				find_next_ext_in_dir_alpha(0, 0, 0, FIND_ALPHA_INIT_FOLDER); 				// Initialize alphabetical folder search
				samples[bank][samplenum].file_found = 0;

				while (!samples[bank][samplenum].file_found)
				{																			//Search alphabetically for any wav file in bankpath
					res = find_next_ext_in_dir_alpha(bankpath_noslash, ".wav", filename, FIND_ALPHA_DONT_INIT);
					if (res!=FR_OK)		{samplenum=NUM_SAMPLES_PER_BANK; break;}			//When we hit the end of the folder, stop searching this bank [exit while() and for() loops]

					str_cat(fullpath, bankpath, filename);									//Append filename to path

					if (find_filename_in_bank(bank, fullpath) != 0xFF) continue;			//Skip files that are already used in this bank

					res = f_open(&temp_file, fullpath, FA_READ);							//Open the file

					if (res==FR_OK)
					{
						res = load_sample_header(&(samples[bank][samplenum]), &temp_file);	//Load the sample header info into samples[][]

						if (res==FR_OK)														//If we can load the file's wav header, then success!
						{
							str_cpy(samples[bank][samplenum].filename, fullpath);				//Set the filename (full path)
							samples[bank][samplenum].file_found = 1;							//Mark it as found
						}
					}
					f_close(&temp_file);

				} //while !file_found
			} //if filename is blank
		} //for each sample
	}//for each bank
}



//Go through all banks and samples,
//If file_found==0, then look for a file to fill the slot:
//If samples[][].filename == "path/to/file.wav":
// - Scan all .wav files in path alphabetically (path/*.wav), in two passes:
//   --First pass: Look for a .wav file that isn't assigned anywhere
//   --Second pass: Look for a .wav file that isn't assigned in this bank (but may be assigned in some other bank)
// - If still none found, keep file_found=0 and exit
//
void load_missing_files(void)
{
	uint8_t	bank, samplenum;
	char 	path[_MAX_LFN+1];
	char 	path_noslash[_MAX_LFN+1];
	char	filename[_MAX_LFN+1];
	char	fullpath[_MAX_LFN+1];

	FIL			temp_file;
	FRESULT		res=FR_OK;
	// uint8_t		path_len;
	uint8_t		first_pass=1;


	for (bank=0;bank<MAX_NUM_BANKS;bank++)													//Scan samples[][] for non-blank filename with file_found==0
	{

		for(samplenum=0;samplenum<NUM_SAMPLES_PER_BANK;samplenum++)
		{
			if (samples[bank][samplenum].filename[0] && !samples[bank][samplenum].file_found)
			{
				if (str_split(samples[bank][samplenum].filename, '/', path, filename) == 0)		//split up filename into path and filename
				{
					//no slashes exist in filename, so path is the root dir
					path[0]='\0'; //root dir
					str_cpy(filename, samples[bank][samplenum].filename);
				}

				//Create a copy of path string that doesn't end in a slash
				str_cpy(path_noslash, path);
				trim_slash(path_noslash);

				// path_len = str_len(path);
				// str_cpy(path_noslash, path);
				// if (path[path_len-1] == '/')
				// 	path_noslash[path_len-1]='\0';

				find_next_ext_in_dir_alpha(0, 0, 0, FIND_ALPHA_INIT_FOLDER); 				// Initialize alphabetical folder search

				while (!samples[bank][samplenum].file_found) 								// for each file not found:
				{
																							//Search alphabetically for any wav file in original file's path
					res = find_next_ext_in_dir_alpha(path_noslash, ".wav", filename, FIND_ALPHA_DONT_INIT);

					if (res!=FR_OK)															//First pass: When our alphabetical search hits the end of the folder,
					{																		//initialize the search and do another pass
						if (first_pass) {first_pass=0; find_next_ext_in_dir_alpha(0, 0, 0, FIND_ALPHA_INIT_FOLDER);}
						else break;															//Second pass: When we hit the end of the folder, stop searching
					}

					str_cat(fullpath, path, filename);										//Append filename to path

					if (first_pass){
						if (find_filename_in_all_banks(bank, fullpath) != 0xFF) continue;}	//First pass: skip files that are used in *any* bank
					else{
						if (find_filename_in_bank(bank, fullpath) != 0xFF) continue;}		//Second pass: Skip files that are used in *this* bank

					res = f_open(&temp_file, fullpath, FA_READ);							//Open the file

					if (res==FR_OK)
					{
						res = load_sample_header(&(samples[bank][samplenum]), &temp_file);	//Load the sample header info into samples[][]

						if (res==FR_OK)
						{
							str_cpy(samples[bank][samplenum].filename, fullpath);			//Set the filename (full path)
							samples[bank][samplenum].file_found = 1;						//Mark it as found
							enable_bank(bank);
						}
					}
					f_close(&temp_file);

				}


				//If no files are found, try searching anywhere for the filename
				if (!samples[bank][samplenum].file_found)
				{
					samples[bank][samplenum].filename[0]='\0'; //not found: blank out filename
																							//ToDo: If we change indexing, we may not want to blank out the filename here
																							//so that we can write "path/notfoundfile.wav -- NOT FOUND" in the index text file
																							//For now, we leave this because sometimes we use a blank filename to indicate file is
																							//not able to be loaded
				}

			} //if file_found==0

		} //for each sample
	}//for each bank
}

//
//Given a path to a directory, see if any of the files in it are assigned to a sample slot
//
//Instead of going through each file in the directory in the filesystem
//and checking all the sample slots if it matches a filename,
//it's faster to search the samples[][] array for filenames that begin with this directory path + "/"
//
//Returns 1 if some sample slot uses this directory
//0 if not
//
//Note: path must end in a slash! We don't add the slash here because we don't know if path[str_len(path)+1] is a valid memory location
//This is important because path="Blue" could return a false positive for "Blue Loops/mysound.wav", when we really are looking for "Blue/*.wav"
//If we want to check the root dir, give "/" for path. This will search for samples who's entry starts with "/", or have no slash in their full path
uint8_t dir_contains_assigned_samples(char *path)
{
	uint8_t bank, samplenum;
	uint8_t l;

	l=str_len(path);
	if (path[l-1] != '/') return 0xFF; //fail, path must end in a slash

	for (bank=0;bank<MAX_NUM_BANKS;bank++)
	{
		for(samplenum=0;samplenum<NUM_SAMPLES_PER_BANK;samplenum++)
		{
			if (samples[bank][samplenum].filename[0] != 0)
				if (str_startswith_nocase(samples[bank][samplenum].filename, path) == 1) //
					return(1); //found! at least one sample filename begins with path
		}
	}

	return (0); //no filenames start with path
}


//
//
//
void load_new_folders(void)
{
	DIR 	root_dir, test_dir;
	FRESULT res;
	char 	foldername[_MAX_LFN+1];
	char 	default_bankname[_MAX_LFN];
	uint8_t bank;
	uint8_t bank_loaded;
	uint8_t l;
	uint8_t checked_root=0;

	//reset dir object, so it can be used by get_next_dir
	root_dir.obj.fs = 0;

	while (1)
	{
		bank_loaded=0;
		if (!checked_root)
		{
			checked_root = 1;//only check root the first time

			//Check the root dir first
			foldername[0] = '\0'; //root
			l = 0;

			//Check if root dir contains any .wav files
			res = f_opendir(&test_dir, foldername);

			if (res!=FR_OK) {f_closedir(&test_dir);continue;}
			if (find_next_ext_in_dir(&test_dir, ".wav", default_bankname) != FR_OK) {f_closedir(&test_dir);continue;}

			//Check if it contains any assigned samples
			if (dir_contains_assigned_samples("/")) continue;


		}
		else
		{
			//Get the name of the next folder inside the root dir
			res = get_next_dir(&root_dir, "", foldername);

			//exit if no more dirs found
			if (res != FR_OK) {f_closedir(&root_dir); break;}

			//Check if foldername contains any .wav files
			res = f_opendir(&test_dir, foldername);
			if (res!=FR_OK) continue;
			if (find_next_ext_in_dir(&test_dir, ".wav", default_bankname) != FR_OK) continue;

			//add a slash to folder_path if it doesn't have one
			l = str_len(foldername);
			if (foldername[l-1] !='/'){foldername[l] = '/'; foldername[l+1] = '\0';}

			if (dir_contains_assigned_samples(foldername)) continue;
		}

		//If we got here, we found a directory that contains wav files, and none of the files are assigned as samples
		//Now we will try to add this as a new bank

		//Strip the ending slash
		foldername[l]='\0';

		//Try to load it as a bank, to make sure there's actually some valid files inside (i.e. headers are not corrupted)
		if (load_bank_from_disk(test_bank, foldername))
		{
			//First see if the foldername begins with a color or color-blink name:
			//Search banks from end to start, so we look for "Yellow-2" before "Yellow"
			for (bank=(MAX_NUM_BANKS-1);bank!=0xFF;bank--)
			{
				bank_to_color(bank, default_bankname);

				if (str_startswith_nocase(foldername, default_bankname))
				{
					//If the bank is already being used (e.g. "Yellow-2" is already used)
					//Then bump down Yellow-2 ==> Yellow-3, Yellow-3 ==> Yellow-4, etc..
					if (is_bank_enabled(bank))
						bump_down_banks(bank);

					//Copy the test_bank into bank
					copy_bank(samples[bank], test_bank);
					enable_bank(bank);
					bank_loaded=1;
					break; //exit the for loop
				}
			}

			//If it's not a color/blink name, then load it into the first unused bank
			if (!bank_loaded)
			{
				bank = next_disabled_bank(MAX_NUM_BANKS);
				copy_bank(samples[bank], test_bank);
				enable_bank(bank);
				bank_loaded=1;
			}
		}
	}
}


//
//Go through all default bank folder names,
//and see if each one is an actual folder
//If so, open it up.
//
uint8_t load_banks_by_default_colors(void)
{
	uint8_t bank;
	char 	bankname[_MAX_LFN], bankname_case[_MAX_LFN];
	uint8_t banks_loaded;

	banks_loaded=0;
	for (bank=0;bank<MAX_NUM_BANKS;bank++)
	{
		bank_to_color(bank, bankname);

		// Color
		if (load_bank_from_disk((samples[bank]), bankname))
		{
			enable_bank(bank);
			banks_loaded++;
		}
		else
		{
			// COLOR
			str_to_upper(bankname, bankname_case);
			if (load_bank_from_disk((samples[bank]), bankname_case))
			{
				enable_bank(bank);
				banks_loaded++;
			}
			else
			{
				// color
				str_to_lower(bankname, bankname_case);
				if (load_bank_from_disk((samples[bank]), bankname_case))
				{
					enable_bank(bank);
					banks_loaded++;
				}
				else{disable_bank(bank);}
			}
		}
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
		// Find the next directory in the root folder
		res = get_next_dir(&rootdir, "", foldername);

		if (res != FR_OK) break; //no more directories, exit the while loop

		test_path_loaded = 0;

		// Check if folder contains any .wav files
		res = f_opendir(&testdir, foldername);
		if (res!=FR_OK) continue;
		if (find_next_ext_in_dir(&testdir, ".wav", default_bankname) != FR_OK) continue;

		// See if the folder we found is a default bank name
		// This would mean it's already loaded (with load_banks_by_default_colors())
		if (color_to_bank(foldername) < MAX_NUM_BANKS)
			test_path_loaded=1;

		// Go through all the numbered bank names (Red-1, Pink-1, etc...)
		// see if the folder we found has a numbered bank name as a prefix
		// This finds things like "Red-3 - TV Clips" (but not "Red - Movie Clips")
		// color-number fodlername
		for (bank=10;(!test_path_loaded && bank<MAX_NUM_BANKS);bank++)
		{
			bank_to_color(bank, default_bankname);

			if (str_startswith_nocase(foldername, default_bankname))
			{
				// Make sure the bank is not already being used
				if (!is_bank_enabled(bank))
				{
					// ...and if not, then try to load it as a bank
					if (load_bank_from_disk((samples[bank]), foldername))
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

			if (str_startswith_nocase(foldername, default_bankname))
			{
				//We found a prefix for this folder
				//If the base color name bank is still empty, load it there...
				//E.g. if we found "Red - Synth Pads" and Red bank is still available, load it there
				if (!is_bank_enabled(bank))
				{
					if (load_bank_from_disk((samples[bank]), foldername))
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
							if (load_bank_from_disk((samples[bank]), foldername))
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
	uint8_t checked_root;

	FRESULT res;
	DIR rootdir;
	DIR testdir;

	banks_loaded=0;

	checked_root = 0;
	rootdir.obj.fs = 0; //get_next_dir() needs us to do this, to reset it

	while (1)
	{
		if (!checked_root)
		{
			checked_root = 1; //only check root the first time
			foldername[0] = '\0'; //root
		}
		else
		{
			//Find the next directory in the root folder
			res = get_next_dir(&rootdir, "", foldername);

			if (res != FR_OK) break; //no more directories, exit the while loop
		}

		test_path_loaded = 0;

		//Open the folder
		res = f_opendir(&testdir, foldername);
		if (res!=FR_OK) continue;

		//Check if folder contains any .wav files
		if (find_next_ext_in_dir(&testdir, ".wav", default_bankname) != FR_OK) {f_closedir(&testdir);continue;}

		//See if it starts with a default bank color string
		for (bank=0;bank<10;bank++)
		{
			bank_to_color(bank, default_bankname);
			if (str_startswith_nocase(foldername, default_bankname))
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
				if (load_bank_from_disk((samples[bank]), foldername))
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
				else
					break; //if load_bank_from_disk() fails for this foldername, try the next folder
			}
		}
	}

	return(banks_loaded);
}


//
//Tries to open the folder bankpath and load 10 files into samples[][]
//into the specified bank
//
//Returns number of samples loaded (0 if folder not found, and sample[bank][] will be cleared)
//

uint8_t load_bank_from_disk(Sample *sample_bank, char *path_noslash)
{
	uint32_t i;
	uint32_t sample_num;

	FIL temp_file;
	FRESULT res;

	uint8_t path_len;
	char path[_MAX_LFN];
	char filename[256];


	for (i=0;i<NUM_SAMPLES_PER_BANK;i++)	clear_sample_header(&(sample_bank[i]));

	//Remove trailing slash
	trim_slash(path_noslash);

	// path_len = str_len(path_noslash);
	// if (path_noslash[path_len-1] == '/')
	// 	path_noslash[path_len-1]='\0';

	//Copy path_noslash into path and add a slash
	str_cpy(path, path_noslash);
	path_len = str_len(path);
	path[path_len++]='/';
	path[path_len]  ='\0';

	sample_num  = 0;
	filename[0] = 0;

	find_next_ext_in_dir_alpha(path_noslash, ".wav", 0, FIND_ALPHA_INIT_FOLDER);				// Initialize alphabetical searching in folder

	while (sample_num < NUM_SAMPLES_PER_BANK)													// for every sample slot in bank
	{
		res = find_next_ext_in_dir_alpha(path_noslash, ".wav", filename, FIND_ALPHA_DONT_INIT); // find next file alphabetically
		if (res!=FR_OK){break;}																	// Stop if no more files found/available

		str_cpy(&(path[path_len]), filename);													//Append filename to path
		res = f_open(&temp_file, path, FA_READ);												//Open file
		f_sync(&temp_file);

		if (res==FR_OK)
		{
			res = load_sample_header(&(sample_bank[sample_num]), &temp_file);					//Load the sample header info into samples[][]
			if (res==FR_OK)
			{
				str_cpy(sample_bank[sample_num++].filename, path);								//Set the filename (full path)
			}
		}
		f_close(&temp_file);
	}
	return(sample_num);
}


uint8_t load_all_banks(uint8_t force_reload)
{
	FRESULT res;
	FRESULT queue_valid;

	//Load the index file:
	if (FW_MINOR_VERSION==0)		flags[RewriteIndex]=YELLOW;
	else if (FW_MINOR_VERSION==1)	flags[RewriteIndex]=GREEN;
	else if (FW_MINOR_VERSION==2)	flags[RewriteIndex]=CYAN;
	else if (FW_MINOR_VERSION==3)	flags[RewriteIndex]=BLUE;
	else if (FW_MINOR_VERSION==4)	flags[RewriteIndex]=PINK;
	else if (FW_MINOR_VERSION==5)	flags[RewriteIndex]=RED;
	else if (FW_MINOR_VERSION==6)	flags[RewriteIndex]=ORANGE;
	else if (FW_MINOR_VERSION==7)	flags[RewriteIndex]=AQUA;
	else if (FW_MINOR_VERSION==8)	flags[RewriteIndex]=DIM_RED;
	else if (FW_MINOR_VERSION==9)	flags[RewriteIndex]=CYANER;


	//Load the index file, marking files found or not found with samples[][].file_found = 1/0;
	if (!force_reload)
		force_reload = load_sampleindex_file(USE_INDEX_FILE, MAX_NUM_BANKS);

	if (!force_reload) //sampleindex file was ok
	{
		//Look for new folders and missing files
		flags[RewriteIndex]=ORANGE;

		// Update the list of banks that are enabled
		// Banks with no file_found will be disabled (but filenames will be preserved, for use in load_missing_files)
		check_enabled_banks();

		// Check for new folders, and turn them into banks if possible
		load_new_folders();

		// Check for files that have file_found==0
		// Look for a file to fill this slot
		load_missing_files();

		// Check for empty slots
		load_empty_slots();
	}

	else //sampleindex file was not ok, or we requested to force a full reload from disk
	{

		// Ignore index and create new banks from disk:
		flags[RewriteIndex]=WHITE;

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
	// ... so sample info gets updated with latest .wav header content
	// Buttons are red for index file, then orange for html file

	res = index_write_wrapper(); //the wrapper sets flags[RewriteIndex] to ORANGE during html file write

	// Done re-indexing (buttons are normal)
	flags[RewriteIndex]=0;

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
// new_filename()
//
//Creates a new filename in the proper format
//The proper format is:
// PREFIX + TAKE# + SUFFIX + EXTENSION
// For the STS we set prefix to null and put the sample slot# after the suffix, so it's just:
// 		001-Sample02.wav
// Where 02 is the slot# we recorded into, and 001 is the Take#
// We also determine the best directory to place this file in, and make that part of path:
// 		BankFolder/001-Sample02.wav --> path
//
#define SLOT_DIGITS 2 /* number of digits in the Slot number: since we have max 10 slots, we need 2 digits */
#define TAKE_DIGITS 3 /* number of digits in the Take number: 3 means "001", 4 means "0001", etc. */
#define WAV_EXT ".wav"
uint8_t new_filename(uint8_t bank, uint8_t sample_num, char *path)
{
	uint8_t i;
	FRESULT res;
	DIR dir;
	uint32_t sz;

	char		slot_str[SLOT_DIGITS+1];
	char		take_str[TAKE_DIGITS+1];
	char		slot_prefix[10];
	char		slot_suffix[10];
	char		timestamp_str[12];
	uint16_t	highest_num, num;
	char		filename[_MAX_LFN+1];
	uint8_t		take_pos;
	uint32_t	max_take_num;
	uint8_t		do_add_timestamp=0;

	//
	//Figure out the folder to put a new recording in:
	//1st choice: Same folder as the sample already existing in the current REC bank/sample
	//2nd choice: Same folder as the first existing sample in the current REC bank
	//3rd choice: default folder name for current REC bank
	//

	path[0]=0;

	//1st) Check for the current REC sample/bank folder:
	//If file name is not blank, split the filename at the last '/'
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
	{
		res = f_mkdir(path);
		res = f_opendir(&dir, path);
	}

	//If we got an error opening or creating a dir
	//try reloading the SDCard, then opening the dir (and creating if needed)
	if (res!=FR_OK)
	{
		res = reload_sdcard();

		if (res==FR_OK)
		{
			res = f_opendir(&dir, path);

			if (res==FR_NO_PATH)
			{
				res = f_mkdir(path);
				res = f_opendir(&dir, path);
			}
		}
	}

	if (res!=FR_OK)
	{
		return (FR_INT_ERR); //fail
	}

	//
	//Create the file name
	//
	//
	//Find the highest numbered file (of our proper format), and name this file one higher
	//If no files are found of our format, start at 1

	highest_num = 0;

	//Compute the slot prefix
	// intToStr(sample_num+1, slot_str, 2);
	// str_cat(slot_prefix, "Slot", slot_str);
	// str_cat(slot_prefix, slot_prefix, "-");
	slot_prefix[0]='\0';

	//Compute the slot suffix
	str_cpy(slot_suffix, "-Sample");

	//Compute the slot string
	intToStr(sample_num+1, slot_str, SLOT_DIGITS);

	while(1)
	{
		res = find_next_ext_in_dir(&dir, WAV_EXT, filename);

		if (res!=FR_OK && res!=0xFF)
		{
			f_closedir(&dir);

			//filesystem error reading directory,
			//Just use a timestamp to be safe
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

			return (res);
		}
		if (res==0xFF)
		{
			//no more .wav files found
			//exit the while loop
			f_closedir(&dir);
			break;
		}
		if (filename[0])
		{
			//See if the file is of the form we're looking for:

			//Must be exactly the right length
			if (str_len(filename) != (str_len(slot_prefix) + TAKE_DIGITS + str_len(slot_suffix) + SLOT_DIGITS + str_len(WAV_EXT))) continue;

			//Must start with slot prefix
			if (!str_startswith_nocase(filename, slot_prefix)) continue;

			//Must contain the slot suffix
			if (str_found(filename, slot_suffix)==0) continue;

			//Three characters of the Take# must be digits
			take_pos = str_len(slot_prefix);
			if (!( filename[take_pos]>='0' && filename[take_pos]<='9' \
				&& filename[take_pos+1]>='0' && filename[take_pos+1]<='9' \
				&& filename[take_pos+2]>='0' && filename[take_pos+2]<='9')) continue;

			//Extract the take number string and convert to an int
			take_str[0] = filename[take_pos];
			take_str[1] = filename[take_pos+1];
			take_str[2] = filename[take_pos+2];
			take_str[3] = '\0';
			num = str_xt_int(take_str);

			//Update highest_num if we find a higher one
			if (num > highest_num)
				highest_num = num;

		}
	}

	//Go one higher than the highest found
	highest_num++;

	//Boundary check highest_num is not greater than the max number of digits
	max_take_num=1;
	for (i=0;i<TAKE_DIGITS;i++) max_take_num*=10;
	if (highest_num >= max_take_num) {do_add_timestamp=1;highest_num=(max_take_num-1);} //stay at the max num if we surpass it, and later we'll add a timestamp

	//Create the filename with path
	str_cat(path, path, "/");
	str_cat(path, path, slot_prefix);
	intToStr(highest_num, take_str, 3);
	str_cat(path, path, take_str);

	if (do_add_timestamp) {
		str_cat(path, path, "-");
		intToStr(sys_tmr, timestamp_str, 10);
		str_cat(path, path, timestamp_str);
	}
	str_cat(path, path, slot_suffix);
	str_cat(path, path, slot_str);
	str_cat(path, path, WAV_EXT);

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
//		- 3: folderpath/filename was incorrect, but filename worked and was corrected by adding a slash to the beginning
//Note: folderpath should be blank (root), or end in a '/'
uint8_t fopen_checked(FIL *fp, char* folderpath, char* filename)
{
	DIR 		rootdir;
	FRESULT 	res, res_dir;
	char 		fullpath[_MAX_LFN+1];
	// char 		fname[_MAX_LFN+1];
	char 		dumpedpath[_MAX_LFN+1];
	char* 		fileonly;
	char 		fileonly_ptr[_MAX_LFN+1];

	// str_cpy(fname,filename);

	// OPEN FILE FROM PROVIDED FOLDER

	// initialize full path to file
	str_cat(fullpath, folderpath, filename);

	// try to open full path
	res = f_open(fp, fullpath, FA_READ);
	f_close(fp);

	// if file loaded properly, move on
	if (res==FR_OK)
	{
		str_cpy(filename, fullpath);
		return(0);
	}

	// otherwise...
	else
	{
		// OPEN FILE FROM ROOT - AS WRITTEN IN INDEX
		// try opening file supposing its path is in the name field
		res = f_open(fp, filename, FA_READ);
		f_close(fp);

		// if file was found
		// return 3: folderpath/filename was incorrect, but filename worked
		// pre-pend a slash and exit function
		if (res==FR_OK) {
			str_cat(fullpath, "/", filename);
			str_cpy(filename, fullpath);
			return(3);
		}

		// OPEN FILE FROM ROOT - FILE.WAV ONLY
		// ...Otherwise, Remove any existing path before wav file name
		fileonly = fileonly_ptr;
		str_split(filename, '/', dumpedpath, fileonly);

		// if there was a path before filename
		if (fileonly[0]!=0)
		{

			// try opening file name only, from root
			res = f_open(fp, fileonly, FA_READ);
			f_close(fp);

			// if file was found
			// return 3: folderpath/filename was incorrect, but filename worked
			// pre-pend a slash and exit function
			if (res==FR_OK) {
				str_cat(fullpath, "/", filename);
				str_cpy(filename, fullpath);
				return(3);
			}
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
				f_close(fp);

				// if file was found
				// return "1: path was incorrect, and corrected"
				// exit function
				if(res == FR_OK)
				{
					str_cpy(filename, fullpath);
					return(1);
				}
			}

			// Otherwise, stop searching
			else
			{
				// return "2: path couldn't be corrected"
				// exit function
				return(2);
			}
		}
	}
	return(3); //unknown-- this is here to prevent compiler warning
}
