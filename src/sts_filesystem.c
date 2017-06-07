#include "globals.h"
#include "params.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "file_util.h"
// #include <string.h>
// #include <stdio.h>

Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];



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

uint8_t bank_to_color(uint8_t bank, char *color)
{
	switch(bank)
	{
	case 0:
		str_cpy(color, "White");
		return(5);
		break;

	case 1:
		str_cpy(color, "Red");
		return(3);
		break;

	case 2:
		str_cpy(color, "Green");
		return(5);
		break;

	case 3:
		str_cpy(color, "Blue");
		return(4);
		break;

	case 4:
		str_cpy(color, "Yellow");
		return(6);
		break;

	case 5:
		str_cpy(color, "Cyan");
		return(4);
		break;

	case 6:
		str_cpy(color, "Orange");
		return(6);
		break;

	case 7:
		str_cpy(color, "Violet");
		return(6);
		break;

	case 8:
		str_cpy(color, "White-SAVE");
		return(10);
		break;

	case 9:
		str_cpy(color, "Red-SAVE");
		return(8);
		break;

	case 10:
		str_cpy(color, "Green-SAVE");
		return(10);
		break;

	case 11:
		str_cpy(color, "Blue-SAVE");
		return(9);
		break;

	case 12:
		str_cpy(color, "Yellow-SAVE");
		return(11);
		break;

	case 13:
		str_cpy(color, "Cyan-SAVE");
		return(9);
		break;

	case 14:
		str_cpy(color, "Orange-SAVE");
		return(11);
		break;

	case 15:
		str_cpy(color, "Violet-SAVE");
		return(11);
		break;

	default:
		color[0]=0;
		return(0);
	}


}

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


//void check_bank_dirs(void)
//{
//	uint32_t i;
//	uint32_t sample_num, bank_i, bank;
//	FRESULT res;
//	DIR dir;
//	char path[10];
//	char tname[_MAX_LFN+1];
//
//	for (bank_i=0;bank_i<MAX_NUM_BANKS;bank_i++)
//	{
//		bank_status[bank_i] = 0; //does not exist
//
//		bank = bank_i;
//
//		if (bank_i >= MAX_NUM_REC_BANKS)
//			bank -= MAX_NUM_REC_BANKS;
//
//		i = bank_to_color(bank, path);
//
//		if (bank_i != bank)
//		{
//			//Append "-SAVE" to directory name
//			path[i++] = '-';
//			path[i++] = 'S';
//			path[i++] = 'A';
//			path[i++] = 'V';
//			path[i++] = 'E';
//			path[i] = 0;
//		}
//
//		res = f_opendir(&dir, path);
//		if (res==FR_OK)
//		{
//			tname[0]=0;
//			res = find_next_ext_in_dir(&dir, ".wav", tname);
//
//			if (res==FR_OK)
//				bank_status[bank_i] = 1; //exists
//
//		}
//	}
//}

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
	s_sample->filename[0] 		= 0;
	s_sample->sampleSize 		= 0;
	s_sample->sampleByteSize 	= 0;
	s_sample->sampleRate 		= 0;
	s_sample->numChannels 		= 0;
	s_sample->blockAlign 		= 0;
	s_sample->startOfData 		= 0;
	s_sample->PCM 				= 0;

	s_sample->inst_start 		= 0;
	s_sample->inst_end 			= 0;
	s_sample->inst_size 		= 0;
	s_sample->inst_gain 		= 1.0;
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

						s_sample->sampleSize 		= chunk.chunkSize;
						s_sample->sampleByteSize 	= fmt_chunk.bitsPerSample>>3;
						s_sample->sampleRate 		= fmt_chunk.sampleRate;
						s_sample->numChannels 		= fmt_chunk.numChannels;
						s_sample->blockAlign 		= fmt_chunk.numChannels * fmt_chunk.bitsPerSample>>3;
						s_sample->startOfData 		= f_tell(sample_file);
						s_sample->inst_end 			= s_sample->sampleSize ;//& 0xFFFFFFF8;
						s_sample->inst_size 		= s_sample->sampleSize ;//& 0xFFFFFFF8;
						s_sample->inst_start 		= 0;
						s_sample->inst_gain 		= 1.0;
							
						if (fmt_chunk.audioFormat == 0xFFFE)
							s_sample->PCM = 3;
						else
							s_sample->PCM = fmt_chunk.audioFormat;

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



//returns number of samples loaded

uint8_t load_bank_from_disk(uint8_t bank)
{
	uint32_t i;
	uint32_t sample_num;
	FIL temp_file;
	FRESULT res;
	DIR dir;
	char path[10];
	char tname[_MAX_LFN+1];
	char path_tname[_MAX_LFN+1];


	for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
		clear_sample_header(&samples[bank][i]);

	sample_num=0;
	i = bank_to_color(bank, path);

	res = f_opendir(&dir, path);

	if (res==FR_NO_PATH)
		return(0);
	//ToDo: check for variations of capital letters (or can we just check the short fname?)

	if (res==FR_OK)
	{
		tname[0]=0;

		while (sample_num < NUM_SAMPLES_PER_BANK)
		{
			res = find_next_ext_in_dir(&dir, ".wav", tname);
			if (res!=FR_OK) break;

			i = str_len(path);
			str_cpy(path_tname, path);
			path_tname[i]='/';
			str_cpy(&(path_tname[i+1]), tname);

			res = f_open(&temp_file, path_tname, FA_READ);
			f_sync(&temp_file);

			if (res==FR_OK)
			{
				res = load_sample_header(&samples[bank][sample_num], &temp_file);

				if (res==FR_OK)
				{
					str_cpy(samples[bank][sample_num++].filename, path_tname);
	//				preloaded_clmt[sample_num] = load_file_clmt(&temp_file);
				}
			}
			f_close(&temp_file);
		}
		f_closedir(&dir);


		//Special try again using root directory for first bank (WHITE)
		if (bank==0 && sample_num < NUM_SAMPLES_PER_BANK)
		{
			res = f_opendir(&dir, "/");
			if (res==FR_OK)
			{
				tname[0]=0;

				while (sample_num < NUM_SAMPLES_PER_BANK && res==FR_OK)
				{
					res = find_next_ext_in_dir(&dir, ".wav", tname);
					if (res!=FR_OK) break;

					res = f_open(&temp_file, tname, FA_READ);
					f_sync(&temp_file);

					if (res==FR_OK)
					{
						res = load_sample_header(&(samples[bank][sample_num]), &temp_file);

						if (res==FR_OK)
						{
							str_cpy(samples[bank][sample_num++].filename, tname);
			//				preloaded_clmt[sample_num] = load_file_clmt(&temp_file);
						}
					}
					f_close(&temp_file);
				}
				f_closedir(&dir);
			}
		}

	}
	else
	{
		g_error=CANNOT_OPEN_ROOT_DIR;
		return(0);
	}

	return(sample_num);
}

/*  sts_fs_index.c */

uint8_t write_sampleindex_file(void)
{

	FIL 		temp_file;
	FRESULT 	res;
	uint32_t 	sz, bw;
	uint8_t 	i, j;
	char 		b_color[10];
	char 		path[_MAX_LFN+1];
	char 		bank_path[_MAX_LFN+1];
	char 		*ptr;
	char 		t_ptr[_MAX_LFN+1];

	// CREATE INDEX FILE - TXT
	// previous index files are replaced
	// f_open is a function of the module application interface
	// for the Generic FAT file system module  (ff.h)
	
	// create sample.index file
	// ... and its pointer &temp_file
	res = f_open(&temp_file,"sample_index.txt", FA_WRITE | FA_CREATE_ALWAYS); 

	// rise flags if index can't be opened/created
	if (res != FR_OK) return(1); 

	// write cached information of the file  to the volume
	// ... using the Generic FAT file system module (ff.h)
	// ... to preserve the FAT structure on the volume 
	// ... in case the write operation to the FAT volume is interrupted due to an accidental failure
	// ... such as sudden blackout, incorrect media removal and unrecoverable disk error
	f_sync(&temp_file);


	// WRITE 'SAMPLES' INFO TO INDEX FILE

	// For each bank
	for (i=0; i<MAX_NUM_BANKS; i++){

		// Print bank Color to index file
		bank_to_color(i, b_color);
 		f_printf(&temp_file, "--------------------\n%s\n--------------------\n", b_color);

		// For each sample in bank
		for (j=0; j<NUM_SAMPLES_PER_BANK; j++){
 		
			// - split path and filename			
			ptr = t_ptr;
			ptr = str_rstr(samples[i][j].filename, '/', path);
			
			// Print bank path to index file
			if(j==0){
				str_cpy(bank_path, path);
				f_printf(&temp_file, "path: %s\n\n", path);
			}
			
			// Print sample name to index file
			if (str_cmp(path, bank_path)) f_printf(&temp_file, "%s\n",ptr);
			else f_printf(&temp_file, "%s\n", samples[i][j].filename);

			// write unedditable sample info to index file
			switch (samples[i][j].numChannels){
				case 1:
					f_printf(&temp_file, "sample info: %dHz, %d-bit, mono,   %d samples\n", samples[i][j].sampleRate, samples[i][j].sampleByteSize*8, samples[i][j].sampleSize);
				case 2:
					f_printf(&temp_file, "sample info: %dHz, %d-bit, stereo, %d samples\n", samples[i][j].sampleRate, samples[i][j].sampleByteSize*8, samples[i][j].sampleSize);
				break;
			}
 			
			// write edditable sample info to index file
	 		// s_gain = (int)(100 * samples[i][j].inst_gain); // f_printf doesn't handle float. We use % instead.
	 		f_printf(&temp_file, "- Sample slot: %d\n- play start: %d\n- play size: %d\n- play gain(%%): %d\n\n", j+1, samples[i][j].inst_start, samples[i][j].inst_size, (int)(100 * samples[i][j].inst_gain));


		}
 		f_printf(&temp_file, "\n");
	}

	// Write global info to file
	f_printf(&temp_file, "\n------------------------------------------------------------\nGlobal stereo mode: %d\n", global_mode[STEREO_MODE]);
	f_printf(&temp_file, "Timestamp: %d\n", get_fattime());
	
		// timestamp  ((uint32_t)(2016 - 1980) 	<< 25)
				// 	| ((uint32_t)month 			<< 21)
				// 	| ((uint32_t)days 			<< 16)
				// 	| ((uint32_t)hours 			<< 11)
				// 	| ((uint32_t)mins 			<< 5)
				// 	| ((uint32_t)secs 			>> 1);

	// update the volume   
	f_sync(&temp_file);

	// FIXME: update flags
	// rise flags as needed
	if 		(res != FR_OK)	{f_close(&temp_file); return(2);} //file write failed
	else if (bw < sz)		{f_close(&temp_file); return(3);} //not all data written


	// CLOSE INDEX FILE
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


