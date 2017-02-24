#include "globals.h"
#include "params.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"
#include "wavefmt.h"

Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

#define MAX_ASSIGNED 32
uint8_t cur_assigned_sample_i;
uint8_t end_assigned_sample_i;
uint8_t original_assigned_sample_i;
Sample t_assign_samples[MAX_ASSIGNED];


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
//
//	if (bank > MAX_NUM_REC_BANKS)
//	{
//		//Append "-SAVE" to directory name
//		path[i++] = '-';
//		path[i++] = 'S';
//		path[i++] = 'A';
//		path[i++] = 'V';
//		path[i++] = 'E';
//		path[i] = 0;
//	}

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
	s_sample->filename[0] = 0;
	s_sample->sampleSize = 0;
	s_sample->sampleByteSize = 0;
	s_sample->sampleRate = 0;
	s_sample->numChannels = 0;
	s_sample->blockAlign = 0;
	s_sample->startOfData = 0;

}




//
// Load the sample header from the provided file
//
uint8_t load_sample_header(Sample *s_sample, FIL *sample_file)
{
	WaveHeader sample_header;
	FRESULT res;
	uint32_t br;
	uint32_t rd;
	WaveChunk chunk;


	rd = sizeof(WaveHeader);
	res = f_read(sample_file, &sample_header, rd, &br);

	if (res != FR_OK)	{g_error |= FILE_READ_FAIL; return(res);}//file not opened
	else if (br < rd)	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);}//file ended unexpectedly when reading first header
	else if ( !is_valid_wav_format(sample_header) )	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);	}//first header error (not a valid wav file)

	else
	{
		chunk.chunkId = 0;
		rd = sizeof(WaveChunk);

		while (chunk.chunkId != ccDATA)
		{
			res = f_read(sample_file, &chunk, rd, &br);

			if (res != FR_OK)	{g_error |= FILE_READ_FAIL; f_close(sample_file); break;}
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
				s_sample->sampleSize = chunk.chunkSize;
				s_sample->sampleByteSize = sample_header.bitsPerSample>>3;
				s_sample->sampleRate = sample_header.sampleRate;
				s_sample->numChannels = sample_header.numChannels;
				s_sample->blockAlign = sample_header.numChannels * sample_header.bitsPerSample>>3;
				s_sample->startOfData = f_tell(sample_file);

				return(FR_OK);

			} //else chunk
		} //while chunk
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

		while (sample_num < NUM_SAMPLES_PER_BANK && res==FR_OK)
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
	FIL temp_file;
	FRESULT res;
	uint32_t sz, bw;

	res = f_open(&temp_file,"sample.index", FA_WRITE | FA_CREATE_ALWAYS); //overwrite existing file

	if (res != FR_OK) return(1); //file cant be opened/created
	f_sync(&temp_file);

	sz = sizeof(Sample) * MAX_NUM_BANKS * NUM_SAMPLES_PER_BANK;
	res = f_write(&temp_file, samples, sz, &bw);

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

	res = f_open(&temp_file,"sample.index", FA_READ);
	if (res != FR_OK) return(1); //file not found
	f_sync(&temp_file);

	rd = sizeof(Sample) * MAX_NUM_BANKS * NUM_SAMPLES_PER_BANK;
	res = f_read(&temp_file, samples, rd, &br);

	if (res != FR_OK)	{return(2);}//file not read
	else if (br < rd)	{f_close(&temp_file); return(3);}//file ended unexpectedly
//	else if ( !is_valid_Sample_format(samples) )	{f_close(&temp_file); return(3);	}

	f_close(&temp_file);
	return(0);	//OK

}

/**************************************************/
/**************************************************/
/**************************************************/
/**************************************************/
/**************************************************/
/* sts_fs_assignment.c */

uint8_t load_samples_to_assign(uint8_t bank)
{
	uint32_t i;
	uint32_t sample_num;
	FIL temp_file;
	FRESULT res;
	DIR dir;
	char path[10];
	char tname[_MAX_LFN+1];
	char path_tname[_MAX_LFN+1];

	if (bank >= MAX_NUM_REC_BANKS)	bank -= MAX_NUM_REC_BANKS;

	sample_num=0;
	i = bank_to_color(bank, path);

	res = f_opendir(&dir, path);
	if (res==FR_NO_PATH)	return(0);
	//ToDo: check for variations of capital letters (or can we just check the short fname?)

	if (res==FR_OK)
	{
		tname[0]=0;

		while (sample_num < MAX_ASSIGNED)
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
				res = load_sample_header(&t_assign_samples[sample_num], &temp_file);

				if (res==FR_OK)
				{
					str_cpy(t_assign_samples[sample_num++].filename, path_tname);
				}

			}
			f_close(&temp_file);
		}
		f_closedir(&dir);

		//Special try again using root directory for first bank
		if (bank==0 && sample_num < MAX_ASSIGNED)
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
						res = load_sample_header(&t_assign_samples[sample_num], &temp_file);

						if (res==FR_OK)
						{
							str_cpy(t_assign_samples[sample_num++].filename, tname);
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

uint8_t find_current_sample_in_assign(Sample *s)
{
	uint8_t i;

	original_assigned_sample_i = 0xFF;//error, not found

	for (i=0; i<end_assigned_sample_i; i++)
	{
		if (str_cmp(t_assign_samples[i].filename, s->filename))
		{
			original_assigned_sample_i = i;
			break;
		}
	}

	if (original_assigned_sample_i == 0xFF)
		return(1); //fail

	cur_assigned_sample_i = original_assigned_sample_i;
	return(0);

}


void enter_assignment_mode(void)
{
	uint8_t i;

	//force us to be on a non -SAVE bank
	if (i_param[0][BANK] >= MAX_NUM_REC_BANKS)
	{
		do i_param[0][BANK] = next_enabled_bank(i_param[0][BANK]);
		while (i_param[0][BANK] >= MAX_NUM_REC_BANKS);

		flags[PlayBank1Changed] = 1;
	}

	end_assigned_sample_i = load_samples_to_assign(i_param[0][BANK]);

	if (end_assigned_sample_i)
	{
		//find the current sample in the t_assigned_samples array
		i = find_current_sample_in_assign(&(samples[ i_param[0][BANK] ][ i_param[0][SAMPLE] ]));
		if (i)	{flags[AssigningEmptySample1] = 1;}

		//Add a blank/erase sample at the end
		t_assign_samples[end_assigned_sample_i].filename[0] = 0;
		t_assign_samples[end_assigned_sample_i].sampleSize = 0;
		end_assigned_sample_i ++;

		cur_assigned_sample_i = original_assigned_sample_i;
		global_mode[ASSIGN_CH1] = 1;

	} else
	{
		flags[AssignModeRefused1] = 4;
	}
}

void assign_sample(uint8_t assigned_sample_i)
{
	uint8_t sample, bank;

	bank = i_param[0][BANK];
	sample = i_param[0][SAMPLE];

	str_cpy(samples[bank][sample].filename,   t_assign_samples[ assigned_sample_i ].filename);
	samples[bank][sample].blockAlign 		= t_assign_samples[ assigned_sample_i ].blockAlign;
	samples[bank][sample].numChannels 		= t_assign_samples[ assigned_sample_i ].numChannels;
	samples[bank][sample].sampleByteSize 	= t_assign_samples[ assigned_sample_i ].sampleByteSize;
	samples[bank][sample].sampleRate 		= t_assign_samples[ assigned_sample_i ].sampleRate;
	samples[bank][sample].sampleSize 		= t_assign_samples[ assigned_sample_i ].sampleSize;
	samples[bank][sample].startOfData 		= t_assign_samples[ assigned_sample_i ].startOfData;

	flags[ForceFileReload1] = 1;

	if (samples[bank][sample].filename[0] == 0)
		flags[AssigningEmptySample1] = 1;
	else
		flags[AssigningEmptySample1] = 0;

}


void save_exit_assignment_mode(void)
{
	FRESULT res;

	global_mode[ASSIGN_CH1] = 0;

	check_enabled_banks(); //disables a bank if we cleared it out

	res = write_sampleindex_file();
	if (res!=FR_OK) {g_error|=CANNOT_WRITE_INDEX; check_errors();}

}

void cancel_exit_assignment_mode(void)
{
	assign_sample(original_assigned_sample_i);
	global_mode[ASSIGN_CH1] = 0;
}


void next_unassigned_sample(void)
{

	play_state[0]=SILENT;

	cur_assigned_sample_i ++;
	if (cur_assigned_sample_i >= end_assigned_sample_i || cur_assigned_sample_i >= MAX_ASSIGNED)
		cur_assigned_sample_i = 0;

	assign_sample(cur_assigned_sample_i);

	flags[Play1Trig]=1;
}


