/* assignment_mode.c */
#include "globals.h"
#include "params.h"

#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "audio_util.h"

#include "edit_mode.h"



#define MAX_ASSIGNED 32
uint8_t cur_assigned_sample_i;
uint8_t end_assigned_sample_i;
uint8_t original_assigned_sample_i;
Sample t_assign_samples[MAX_ASSIGNED];
uint8_t cur_assign_bank=0xFF;

extern enum g_Errors g_error;
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 	flags[NUM_FLAGS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];
extern enum PlayStates play_state				[NUM_PLAY_CHAN];


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

	//Loads first 32 sample files in a folder into an assignment array

	if (cur_assign_bank != i_param[0][BANK])
	{
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
			if (i)	{flags[AssigningEmptySample] = 1;}

			//Add a blank/erase sample at the end
			t_assign_samples[end_assigned_sample_i].filename[0] = 0;
			t_assign_samples[end_assigned_sample_i].sampleSize = 0;
			end_assigned_sample_i ++;

			cur_assigned_sample_i = original_assigned_sample_i;
			cur_assign_bank = i_param[0][BANK];

		} else
		{
			flags[AssignModeRefused] = 4;
		}
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

	samples[bank][sample].inst_size 		= t_assign_samples[ assigned_sample_i ].inst_size  ;//& 0xFFFFFFF8;
	samples[bank][sample].inst_start 		= t_assign_samples[ assigned_sample_i ].inst_start  ;//& 0xFFFFFFF8;
	samples[bank][sample].inst_end 			= t_assign_samples[ assigned_sample_i ].inst_end  ;//& 0xFFFFFFF8;

	flags[ForceFileReload1] = 1;

	if (samples[bank][sample].filename[0] == 0)
		flags[AssigningEmptySample] = 1;
	else
		flags[AssigningEmptySample] = 0;

}


void save_exit_assignment_mode(void)
{
	FRESULT res;

	//global_mode[EDIT_MODE] = 0;

	check_enabled_banks(); //disables a bank if we cleared it out

	res = write_sampleindex_file();
	if (res!=FR_OK) {g_error|=CANNOT_WRITE_INDEX; check_errors();}
//	else
//		flags[Animate_Good_Save] = 16;

}

void cancel_exit_assignment_mode(void)
{
	load_sampleindex_file();
	//flags[Animate_Revert] = 8;

//	assign_sample(original_assigned_sample_i);
	//global_mode[EDIT_MODE] = 0;
}

//Loads the next sample in the array.
//Note: This could be changed to just find the next file in the folder, 
//and when we get to the end we have a blank sample
//and after that we load the original file name
//then continue looking for the next wav file
//The benefit of this method would be that we aren't just limited to 32 samples per folder, and it uses less memory
void next_unassigned_sample(void)
{

	play_state[0]=SILENT;
	play_state[1]=SILENT;

	cur_assigned_sample_i ++;
	if (cur_assigned_sample_i >= end_assigned_sample_i || cur_assigned_sample_i >= MAX_ASSIGNED)
		cur_assigned_sample_i = 0;

	assign_sample(cur_assigned_sample_i);

	flags[Play1But]=1;
}


void assign_sample_from_other_bank(uint8_t src_bank, uint8_t src_sample)
{

	uint8_t sample, bank;

	bank = i_param[0][BANK];
	sample = i_param[0][SAMPLE];

	str_cpy(samples[bank][sample].filename,   samples[src_bank][src_sample].filename);
	samples[bank][sample].blockAlign 		= samples[src_bank][src_sample].blockAlign;
	samples[bank][sample].numChannels 		= samples[src_bank][src_sample].numChannels;
	samples[bank][sample].sampleByteSize 	= samples[src_bank][src_sample].sampleByteSize;
	samples[bank][sample].sampleRate 		= samples[src_bank][src_sample].sampleRate;
	samples[bank][sample].sampleSize 		= samples[src_bank][src_sample].sampleSize;
	samples[bank][sample].startOfData 		= samples[src_bank][src_sample].startOfData;

	samples[bank][sample].inst_size 		= samples[src_bank][src_sample].inst_size  ;//& 0xFFFFFFF8;
	samples[bank][sample].inst_start 		= samples[src_bank][src_sample].inst_start  ;//& 0xFFFFFFF8;
	samples[bank][sample].inst_end 			= samples[src_bank][src_sample].inst_end  ;//& 0xFFFFFFF8;

	flags[ForceFileReload1] = 1;

	if (samples[bank][sample].filename[0] == 0)
		flags[AssigningEmptySample] = 1;
	else
		flags[AssigningEmptySample] = 0;

	//flags[Animate_Copy_2_1] = 4;

}





void set_sample_gain(Sample *s_sample, float gain)
{
	if (gain >= 5.1) 		gain = 5.1f;
	else if (gain <= 0.1)	gain = 0.1f;

	s_sample->inst_gain = gain;

}

//sets the trim start point between 0 and 100ms before the end of the sample file
void set_sample_trim_start(Sample *s_sample, float coarse, float fine)
{
	uint32_t trimstart;
	int32_t fine_trim;

	if (coarse >= 1.0f) 				trimstart = s_sample->sampleSize - 4420;
	else if (coarse <= (1.0/4096.0))	trimstart = 0;
	else 								trimstart = (s_sample->sampleSize - 4420) * coarse;

	fine_trim = fine * s_sample->sampleRate * s_sample->blockAlign * 0.5; // +/- 500ms

	if ((-1.0*fine_trim) > trimstart) trimstart = 0;
	else trimstart += fine_trim;

	s_sample->inst_start = align_addr(trimstart, s_sample->blockAlign);

	//Clip inst_end to the sampleSize (but keep inst_size the same)
	if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
		s_sample->inst_end = s_sample->sampleSize;
	else
		s_sample->inst_end = s_sample->inst_start + s_sample->inst_size;

	s_sample->inst_end = align_addr(s_sample->inst_end, s_sample->blockAlign);

}

#define TS_MIN 4412
#define TSZC 10
#define TSZF 1

void nudge_trim_start(Sample *s_sample, int32_t fine)
{
	int32_t trimdelta;

	float sample_size_secs;
	uint32_t samples_per_adc;

	sample_size_secs = (float)s_sample->sampleSize / (float)(s_sample->sampleRate * s_sample->blockAlign); // Bytes / (samples/sec * bytes/sample)

	if (sample_size_secs > TSZF)
		samples_per_adc = (TSZF * s_sample->sampleRate * s_sample->blockAlign) >> 12;
	else if (s_sample->sampleSize >= 32768)
		samples_per_adc = s_sample->sampleSize >> 14;
	else
		samples_per_adc = 1;

	trimdelta = samples_per_adc * fine * fine;
	if (fine < 0) trimdelta *= -1;

	if (trimdelta<0)
	{
		trimdelta = -1 * trimdelta;

		if (s_sample->inst_start > trimdelta)
			s_sample->inst_start -= trimdelta;
		else
			s_sample->inst_start = 0;
	}
	else
	{
		s_sample->inst_start += trimdelta;
	}

	if (s_sample->inst_start > s_sample->inst_end)
		s_sample->inst_start = s_sample->inst_end;

	s_sample->inst_start = align_addr(s_sample->inst_start, s_sample->blockAlign);

	//Clip inst_end to the sampleSize (but keep inst_size the same)
	if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
		s_sample->inst_end = s_sample->sampleSize;
	else
		s_sample->inst_end = s_sample->inst_start + s_sample->inst_size;

	s_sample->inst_end = align_addr(s_sample->inst_end, s_sample->blockAlign);

}



void set_sample_trim_size(Sample *s_sample, float coarse)
{
	uint32_t trimsize;
	int32_t fine_trim;


	if (coarse >= 1.0f) 				trimsize = s_sample->sampleSize;
	else if (coarse <= (1.0/4096.0))	trimsize = 4420;
	else 								trimsize = (s_sample->sampleSize - 4420) * coarse + 4420;

	s_sample->inst_size = align_addr(trimsize, s_sample->blockAlign);

	//Set inst_end to start+size and clip it to the sampleSize
	if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
		s_sample->inst_end = s_sample->sampleSize;
	else
		s_sample->inst_end = (s_sample->inst_start + s_sample->inst_size);

	s_sample->inst_end = align_addr(s_sample->inst_end, s_sample->blockAlign);


}

//
//fine ranges from -4095 to +4095
//
// If total sample time is > XX seconds then coarse +/-4095 adjusts length by +/- XX seconds
// Else coarse +/-4095 adjusts length by total sample time
//

void nudge_trim_size(Sample *s_sample, int32_t fine)
{
	int32_t trimdelta;

	float sample_size_secs;
	uint32_t samples_per_adc;

	sample_size_secs = (float)s_sample->sampleSize / (float)(s_sample->sampleRate * s_sample->blockAlign); // Bytes / (samples/sec * bytes/sample)

	// if (sample_size_secs > TSZC)
	// 	samples_per_adc = (TSZC * s_sample->sampleRate * s_sample->blockAlign) >> 12;
	// else if (s_sample->sampleSize >= 8192)
	// 	samples_per_adc = s_sample->sampleSize >> 12;
	// else
	// 	samples_per_adc = 1;

	// trimdelta = samples_per_adc * coarse * coarse;
	// if (coarse < 0) trimdelta *= -1;

	if (sample_size_secs > TSZF)
		samples_per_adc = (TSZF * s_sample->sampleRate * s_sample->blockAlign) >> 12;
	else if (s_sample->sampleSize >= 32768)
		samples_per_adc = s_sample->sampleSize >> 14;
	else
		samples_per_adc = 1;

	trimdelta = samples_per_adc * fine * fine;
	if (fine < 0) trimdelta *= -1;

	//clip ->inst_size+trimdelta to TS_MIN..inst_size
	//Is there a more efficient way of doing this?
	//We don't want to convert ->sampleSize or ->instSize to an int, it must stay unsigned
	if (trimdelta<0)
	{
		trimdelta = -1 * trimdelta;
		if (s_sample->inst_size > trimdelta)
			s_sample->inst_size -= trimdelta;
		else
			s_sample->inst_size = TS_MIN;
	}
	else
	{
		s_sample->inst_size += trimdelta;
	}

	//Clip inst_size to sampleSize
	if (s_sample->inst_size > s_sample->sampleSize)
		s_sample->inst_size = s_sample->sampleSize;

	s_sample->inst_size = align_addr(s_sample->inst_size, s_sample->blockAlign);

	//Set inst_end to inst_start+inst_size and clip it to the sampleSize
	if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
		s_sample->inst_end = s_sample->sampleSize;
	else
		s_sample->inst_end = s_sample->inst_start + s_sample->inst_size;

	s_sample->inst_end = align_addr(s_sample->inst_end, s_sample->blockAlign);

}




