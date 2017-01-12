#include "globals.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"

Sample samples[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

uint8_t bank_status[MAX_NUM_BANKS];


extern enum g_Errors g_error;



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
		return(5);
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

	default:
		color[0]=0;
		return(0);
	}

}

void check_bank_dirs(void)
{
	uint32_t i;
	uint32_t sample_num, bank_i, bank;
	FRESULT res;
	DIR dir;
	char path[10];
	char tname[_MAX_LFN+1];

	for (bank_i=0;bank_i<MAX_NUM_BANKS;bank_i++)
	{
		bank_status[bank_i] = 0; //does not exist

		bank = bank_i;

		if (bank_i >= MAX_NUM_REC_BANKS)
			bank -= MAX_NUM_REC_BANKS;

		i = bank_to_color(bank, path);

		if (bank_i != bank)
		{
			//Append "-SAVE" to directory name
			path[i++] = '-';
			path[i++] = 'S';
			path[i++] = 'A';
			path[i++] = 'V';
			path[i++] = 'E';
			path[i] = 0;
		}

		res = f_opendir(&dir, path);
		if (res==FR_OK)
		{
			tname[0]=0;
			res = find_next_ext_in_dir(&dir, ".wav", tname);

			if (res==FR_OK)
				bank_status[bank_i] = 1; //exists

		}
	}
}

void enable_bank(uint8_t bank)
{
	bank_status[bank] = 1;
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



//returns number of samples loaded

uint8_t load_bank_from_disk(uint8_t bank, uint8_t chan)
{
	uint32_t i;
	uint32_t sample_num;
	FIL temp_file;
	FRESULT res;
	DIR dir;
	char path[10];
	char tname[_MAX_LFN+1];
	char path_tname[_MAX_LFN+1];

	uint8_t bank_color;

	bank_color = bank;
	if (bank_color > MAX_NUM_REC_BANKS)
		bank_color -= MAX_NUM_REC_BANKS;


	for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
		clear_sample_header(&samples[chan][i]);

	sample_num=0;
	i = bank_to_color(bank_color, path);

	if (bank != bank_color)
	{
		//Append "-SAVE" to directory name
		path[i++] = '-';
		path[i++] = 'S';
		path[i++] = 'A';
		path[i++] = 'V';
		path[i++] = 'E';
		path[i] = 0;
	}

	res = f_opendir(&dir, path);

	if (res==FR_NO_PATH)
	{

		//ToDo: check for variations of capital letters (or can we just check the short fname?)

		if (bank != bank_color)
		{
			//color-SAVE/ dir not found, try just color/ dir
			bank_to_color(bank, path);
			res = f_opendir(&dir, path);
		}

		//If that's not found, create it
		if (res==FR_NO_PATH)
			res = f_mkdir(path);

	}
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
				res = load_sample_header(&samples[chan][sample_num], &temp_file);

				if (res==FR_OK)
				{
					str_cpy(samples[chan][sample_num++].filename, path_tname);
	//				preloaded_clmt[sample_num] = load_file_clmt(&temp_file);
				}
			}
			f_close(&temp_file);
		}
		f_closedir(&dir);



		//Special try again using root directory for White bank
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
						res = load_sample_header(&(samples[chan][sample_num]), &temp_file);

						if (res==FR_OK)
						{
							str_cpy(samples[chan][sample_num++].filename, tname);
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
