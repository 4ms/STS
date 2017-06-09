#include "globals.h"
#include "params.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"
#include "wavefmt.h"

#define MAX_BANK_NAME_STRING 9

Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

//TODO: rename this file "bank.c/h"
//Put the sampleindex functions into a separate file (sample_index.c/h)
//put sample_header functions into separate file (sample_header.c/h)
//move find_next_ext_in_dir() to file_util.c

uint8_t bank_status[MAX_NUM_BANKS];


extern enum g_Errors g_error;
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 	flags[NUM_FLAGS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];
enum PlayStates play_state				[NUM_PLAY_CHAN];



uint8_t next_enabled_bank(uint8_t bank) //if bank==0xFF, we find the first enabled bank
{
	uint8_t esc=0;

	do{
		bank++;
		if (bank >= MAX_NUM_BANKS)	bank = 0;

	} while(!bank_status[bank] && (esc++<MAX_NUM_BANKS));

	return (bank);
}


uint8_t bank_to_color_string(uint8_t bank, char *color)
{
	switch(bank)
		{
		case 0:
			str_cpy(color, "Pink");
			return(4);
			break;

		case 1:
			str_cpy(color, "Red");
			return(3);
			break;

		case 2:
			str_cpy(color, "Orange");
			return(6);
			break;

		case 3:
			str_cpy(color, "Yellow");
			return(6);
			break;

		case 4:
			str_cpy(color, "Green");
			return(5);
			break;

		case 5:
			str_cpy(color, "Cyan");
			return(4);
			break;

		case 6:
			str_cpy(color, "Blue");
			return(4);
			break;

		case 7:
			str_cpy(color, "Violet");
			return(6);
			break;

		case 8:
			str_cpy(color, "Lavendar"); 
			return(8);
			break;

		case 9:
			str_cpy(color, "White");
			return(5);
			break;

		default:
			color[0]=0;
			return(0);
		}

}

uint8_t bank_to_color(uint8_t bank, char *color)
{
	uint8_t digit1, digit2, len;

	if (bank>9 && bank<100)
	{
		//convert a 2-digit bank number to a color and number

		//First digit: 49 ---> 4
		digit1 = bank / 10;

		//Second digit: 49 ---> (49 - 4*10) = 9
		digit2 = bank - (digit1*10); 

		//Second digit as a color string: "White"
		len = bank_to_color(digit2, color);

		//Number the banks starting with 1, not 0
		digit1++;

		//Omit the "1" for the first bank of a color, so "Red-1" is just "Red"
		//Otherwise append a dash and number to the color name
		if (digit1 > 1)
		{
			//Move pointer to the end of the string (overwrite the \0)
			color += len;

			//Append a dash: "White-"
			*color++ = '-';
			len++;

			//Append first digit as a string: "White-5"
			intToStr(digit1, color, 1);
			len += 2;
		}

		return (len);
	}
	else 
		return bank_to_color_string(bank, color);
}


// uint8_t bank_to_dual_color(uint8_t bank, char *color)
// {
// 	uint8_t bank1, bank2, len;


// 	if (bank>9 && bank<100)
// 	{
// 		//convert a 2-digit bank number to two single digit banks

// 		//First digit:
// 		bank1 = bank / 10;

// 		//Second digit:
// 		bank2 = bank - (bank1*10);

// 		//First color string:
// 		len = bank_to_color(bank1, color);

// 		//Move pointer to the end of the string (on the /0)
// 		color+=len;

// 		//Add a dash
// 		*color++ = '-';
// 		len++;

// 		//Second color string:
// 		len +=bank_to_color(bank2, color);

// 		return (len);
// 	}
// 	else 
// 		return bank_to_color_string(bank, color);

// }


void check_enabled_banks(void)
{
	uint32_t sample_num, bank;

	for (bank=0;bank<MAX_NUM_BANKS;bank++)
	{
		bank_status[bank] = 0; //does not exist
		for(sample_num=0; sample_num<NUM_SAMPLES_PER_BANK; sample_num++)
		{
			if (samples[bank][sample_num].filename[0] != 0)
			{
				bank_status[bank] = 1;
				break;
			}
		}
	}
}

uint8_t is_bank_enabled(uint8_t bank)
{
	return bank_status[bank] ? 1:0;
}

void enable_bank(uint8_t bank)
{
	bank_status[bank] = 1;
}
void disable_bank(uint8_t bank)
{
	bank_status[bank] = 0;
}

void clear_sample_header(Sample *s_sample)
{
	s_sample->filename[0] = 0;
	s_sample->sampleSize = 0;
	s_sample->sampleByteSize = 0;
	s_sample->sampleRate = 0;
	s_sample->numChannels = 0;
	s_sample->blockAlign = 0;
	s_sample->startOfData = 0;
	s_sample->PCM = 0;

	s_sample->inst_start = 0;
	s_sample->inst_end = 0;
	s_sample->inst_size = 0;
	s_sample->inst_gain = 1.0;
}




//
// Load the sample header from the provided file
//
uint8_t load_sample_header(Sample *s_sample, FIL *sample_file)
{
	WaveHeader sample_header;
	WaveFmtChunk fmt_chunk;
	FRESULT res;
	uint32_t br;
	uint32_t rd;
	WaveChunk chunk;
	uint32_t next_chunk_start;

	rd = sizeof(WaveHeader);
	res = f_read(sample_file, &sample_header, rd, &br);

	if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; return(res);}//file not opened
	else if (br < rd)	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);}//file ended unexpectedly when reading first header
	else if ( !is_valid_wav_header(sample_header) )	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);	}//first header error (not a valid wav file)

	else
	{
		//Look for a WaveFmtChunk (which starts off as a chunk)
		chunk.chunkId = 0;
		rd = sizeof(WaveChunk);

		while (chunk.chunkId != ccFMT)
		{
			res = f_read(sample_file, &chunk, rd, &br);

			if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; f_close(sample_file); break;}
			if (br < rd)		{g_error |= FILE_WAVEFORMATERR;	f_close(sample_file); break;}

			next_chunk_start = f_tell(sample_file) + chunk.chunkSize;
			//fast-forward to the next chunk
			if (chunk.chunkId != ccFMT)
				f_lseek(sample_file, next_chunk_start);

		}

		if (chunk.chunkId == ccFMT)
		{
			//Go back to beginning of chunk --probably could do this more elegantly by removing fmtID and fmtSize from WaveFmtChunk and just reading the next bit of data
			f_lseek(sample_file, f_tell(sample_file) - br);

			//Re-read the whole chunk (or at least the fields we need) since it's a WaveFmtChunk
			rd = sizeof(WaveFmtChunk);
			res = f_read(sample_file, &fmt_chunk, rd, &br);

			if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; return(res);}//file not read
			else if (br < rd)	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);}//file ended unexpectedly when reading format header
			else if ( !is_valid_format_chunk(fmt_chunk) )	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);	}//format header error (not a valid wav file)
			else
			{
				//We found the 'fmt ' chunk, now skip to the next chunk
				//Note: this is necessary in case the 'fmt ' chunk is not exactly sizeof(WaveFmtChunk) bytes, even though that's how many we care about
				f_lseek(sample_file, next_chunk_start);

				//Look for the DATA chunk
				chunk.chunkId = 0;
				rd = sizeof(WaveChunk);

				while (chunk.chunkId != ccDATA)
				{
					res = f_read(sample_file, &chunk, rd, &br);

					if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; f_close(sample_file); break;}
					if (br < rd)		{g_error |= FILE_WAVEFORMATERR;	f_close(sample_file); break;}

					//fast-forward to the next chunk
					if (chunk.chunkId != ccDATA)
						f_lseek(sample_file, f_tell(sample_file) + chunk.chunkSize);

					//Set the sampleSize as defined in the chunk
					else
					{
						if(chunk.chunkSize == 0)
						{
							f_close(sample_file);break;
						}

						//Check the file is really as long as the data chunkSize says it is
						if (f_size(sample_file) < (f_tell(sample_file) + chunk.chunkSize))
						{
							chunk.chunkSize = f_size(sample_file) - f_tell(sample_file);
						}

						s_sample->sampleSize = chunk.chunkSize;
						s_sample->sampleByteSize = fmt_chunk.bitsPerSample>>3;
						s_sample->sampleRate = fmt_chunk.sampleRate;
						s_sample->numChannels = fmt_chunk.numChannels;
						s_sample->blockAlign = fmt_chunk.numChannels * fmt_chunk.bitsPerSample>>3;
						s_sample->startOfData = f_tell(sample_file);
						if (fmt_chunk.audioFormat == 0xFFFE)
							s_sample->PCM = 3;
						else
							s_sample->PCM = fmt_chunk.audioFormat;

						s_sample->inst_end = s_sample->sampleSize ;//& 0xFFFFFFF8;
						s_sample->inst_size = s_sample->sampleSize ;//& 0xFFFFFFF8;
						s_sample->inst_start = 0;
						s_sample->inst_gain = 1.0;

						return(FR_OK);

					} //else chunk
				} //while chunk
			} //is_valid_format_chunk
		}//if ccFMT
	}//no file error

	return(FR_INT_ERR);
}



FRESULT find_next_ext_in_dir(DIR* dir, const char *ext, char *fname)
{
    FRESULT res;
	FILINFO fno;
	uint32_t i;
	char EXT[4];

	fname[0] = 0; //null string

	//make upper if all lower-case, or lower if all upper-case
	if (   (ext[1]>='a' && ext[1]<='z')
		&& (ext[2]>='a' && ext[2]<='z')
		&& (ext[3]>='a' && ext[3]<='z'))
	{
		EXT[1] = ext[1] - 0x20;
		EXT[2] = ext[2] - 0x20;
		EXT[3] = ext[3] - 0x20;
	}

	else if (  (ext[1]>='A' && ext[1]<='Z')
			&& (ext[2]>='A' && ext[2]<='Z')
			&& (ext[3]>='A' && ext[3]<='Z'))
	{
		EXT[1] = ext[1] + 0x20;
		EXT[2] = ext[2] + 0x20;
		EXT[3] = ext[3] + 0x20;
	}
	else
	{
		EXT[1] = ext[1];
		EXT[2] = ext[2];
		EXT[3] = ext[3];
	}
	EXT[0] = ext[0]; //dot



	//loop through dir until we find a file ending in ext
	for (;;) {

	    res = f_readdir(dir, &fno);

	    if (res != FR_OK || fno.fname[0] == 0)	return(1); //no more files found

	    if (fno.fname[0] == '.') continue;					//ignore files starting with a .

		i = str_len(fno.fname);
		if (i==0xFFFFFFFF) 						return (2); //file name invalid

		if (fno.fname[i-4] == ext[0] &&
			  (  (fno.fname[i-3] == ext[1] && fno.fname[i-2] == ext[2] && fno.fname[i-1] == ext[3])
			  || (fno.fname[i-3] == EXT[1] && fno.fname[i-2] == EXT[2] && fno.fname[i-1] == EXT[3])	 )
			)
		{
			str_cpy(fname, fno.fname);
			return(FR_OK);
		}
	}

	return (3); ///error
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
					default_bankname[len++] = '-';
					str_cpy(&(default_bankname[len]), foldername);
					// f_rename(foldername, default_bankname);

					// load_bank_from_disk(bank, default_bankname);

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

/*  sts_fs_index.c */

uint8_t write_sampleindex_file(void)
{
	FIL temp_file;
	FRESULT res;
	uint32_t sz, bw;
	uint32_t data[1];

	res = f_open(&temp_file,"sample.index", FA_WRITE | FA_CREATE_ALWAYS); //overwrite existing file

	if (res != FR_OK) return(1); //file cant be opened/created
	f_sync(&temp_file);

	sz = sizeof(Sample) * MAX_NUM_BANKS * NUM_SAMPLES_PER_BANK;
	res = f_write(&temp_file, samples, sz, &bw);
	f_sync(&temp_file);

	if (res != FR_OK)	{f_close(&temp_file); return(2);}//file write failed
	else if (bw < sz)	{f_close(&temp_file); return(3);}//not all data written

	data[0]=global_mode[STEREO_MODE];
	sz = 4;
	res = f_write(&temp_file, data, sz, &bw);
	f_sync(&temp_file);

	if (res != FR_OK)	{f_close(&temp_file); return(2);}//file write failed
	else if (bw < sz)	{f_close(&temp_file); return(3);}//not all data written


	f_close(&temp_file);
	return(0);
}

uint8_t load_sampleindex_file(void)
{
	FIL temp_file;
	FRESULT res;
	uint32_t rd, br;
	uint32_t data[1];

	res = f_open(&temp_file,"sample.index", FA_READ);
	if (res != FR_OK) return(1); //file not found
	f_sync(&temp_file);

	rd = sizeof(Sample) * MAX_NUM_BANKS * NUM_SAMPLES_PER_BANK;
	res = f_read(&temp_file, samples, rd, &br);

	if (res != FR_OK)	{return(2);}//file not read
	else if (br < rd)	{f_close(&temp_file); return(3);}//file ended unexpectedly
//	else if ( !is_valid_Sample_format(samples) )	{f_close(&temp_file); return(3);	}

	rd = 4;
	res = f_read(&temp_file, data, rd, &br);
	
	if (data[0] == 0) global_mode[STEREO_MODE] = 0;
	else global_mode[STEREO_MODE] = 1;

	if (res != FR_OK)	{return(2);}//file not read
	else if (br < rd)	{f_close(&temp_file); return(3);}//file ended unexpectedly


	f_close(&temp_file);
	return(0);	//OK

}


