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
#include "bank.h"
#include "sample_file.h"
#include "sts_fs_index.h"



uint8_t 				cur_assign_bank=0xFF;
DIR 					assign_dir;
enum AssignmentStates 	cur_assign_state=ASSIGN_OFF;
char 					cur_assign_bank_path[_MAX_LFN];

Sample					sample_undo_buffer;
uint8_t 				cur_assigned_sample_i;

 

uint8_t	cached_looping;
uint8_t cached_rev;
uint8_t cached_play_state;
uint8_t scrubbed_in_edit;


extern enum g_Errors g_error;
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 	flags[NUM_FLAGS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];
extern enum PlayStates play_state[NUM_PLAY_CHAN];




uint8_t enter_assignment_mode(void)
{
	FRESULT res;

	// Return 1 if we've already entered assignment_mode
	if (global_mode[ASSIGN_MODE]) return 1;

	//Enter assignment mode
	global_mode[ASSIGN_MODE] = 1;

	//Find channel 1's bank's path
	get_banks_path(i_param[0][BANK], cur_assign_bank_path);

	//Open the directory
	res = f_opendir(&assign_dir, cur_assign_bank_path);
	if (res==FR_NO_PATH) {global_mode[ASSIGN_MODE] = 0;return(0);} //unable to enter assignment mode

	cur_assign_state = ASSIGN_UNUSED;

	cur_assign_bank = i_param[0][BANK];

	save_undo_state(i_param[0][BANK], i_param[0][SAMPLE]);

	return 1;
}

void save_exit_assignment_mode(void)
{
	FRESULT res;

	check_enabled_banks(); //disables a bank if we cleared it out

	res = write_sampleindex_file();

	if (res!=FR_OK) {g_error|=CANNOT_WRITE_INDEX; check_errors();}
//	else
//		flags[Animate_Good_Save] = 16;

}



//Find the next file in the folder, 
//and when we get to the end we go to the next bank
//and after that we load the original file name
//then continue looking for the next wav file
//The benefit of this method would be that we aren't just limited to 32 samples per folder, and it uses less memory
uint8_t next_unassigned_sample(void)
{
	uint8_t is_file_found, file_is_in_bank;
	char filename[_MAX_LFN];
	char filepath[_MAX_LFN];
	FRESULT res;
	FIL temp_file;
	uint8_t i;


	play_state[0]=SILENT;
	play_state[1]=SILENT;

	//
	//Search for files in the bank we are currently checking (cur_assign_dir)
	//
	//First, go through all files that aren't being used in this bank
	//Then, go through the remaining files in the directory
	//

	is_file_found = 0;
	while (!is_file_found)
	{
		res = find_next_ext_in_dir(&assign_dir, ".wav", filename);

		if (res!=FR_OK && res!=0xFF) return(0); //error reading directory, abort

		if (res==0xFF) //no more .wav files found
		{
			if (cur_assign_state==ASSIGN_UNUSED)
			{
				//Hit the end of the directory: 
				//Go through the dir again, looking for used samples

				//Reset directory
				f_readdir(&assign_dir, (FILINFO *)0);

				//TODO: some kind of flash/color change to indicate we are now doing the already assigned samples
				cur_assign_state=ASSIGN_USED;
				continue;
			} 
			else
			{
				//Hit the end of the directory:
				//Try the next dir
				cur_assign_bank = next_enabled_bank(cur_assign_bank);

				//Todo: test if cur_assign_bank is now equal to i_param[0][BANK], meaning we've searched all the enabled bank folders
				//Now start looking for folders which aren't the names of banks yet
				//This might be a recursive search for all .wav files, ignoring ones that exist in some bank path folder???

				get_banks_path(cur_assign_bank, cur_assign_bank_path);

				res = f_opendir(&assign_dir, cur_assign_bank_path);
				if (res==FR_NO_PATH) return(0);

				cur_assign_state=ASSIGN_UNUSED;
				continue;
			}
		}

		//.wav file was found:
		//Add the path the beginning of the filename
		
		i = str_len(cur_assign_bank_path);
		str_cpy(filepath, cur_assign_bank_path);
		filepath[i]='/';
		str_cpy(&(filepath[i+1]), filename);

		//See if the file already is assigned to a sample slot in the current bank
		if (find_filename_in_bank(cur_assign_bank, filepath) == 0xFF)	file_is_in_bank = 0;
		else															file_is_in_bank = 1;

		//If the file in the dir is the original file, then treat it as if it was in the bank
		//Otherwise we would never be able to go back to the original file without exiting ASSIGN MODE
		if (str_cmp(filepath, sample_undo_buffer.filename))				file_is_in_bank = 1;

		if (	(cur_assign_state==ASSIGN_UNUSED && !file_is_in_bank) \
			|| 	(cur_assign_state==ASSIGN_USED && file_is_in_bank))
		{

			res = f_open(&temp_file, filepath, FA_READ);
			f_sync(&temp_file);

			if (res==FR_OK)
			{
				res = load_sample_header(&samples[i_param[0][BANK]][i_param[0][SAMPLE]], &temp_file);

				if (res==FR_OK)
				{
					str_cpy(samples[i_param[0][BANK]][i_param[0][SAMPLE]].filename, filepath);

					is_file_found = 1;
				}
				f_close(&temp_file);

			}
		}
	}
	if (is_file_found)
		return(1);
	else
		return(0);
}

//backs-up sample[][] data to an undo buffer
void save_undo_state(uint8_t bank, uint8_t samplenum)
{
	str_cpy(sample_undo_buffer.filename,  samples[bank][samplenum].filename);
	sample_undo_buffer.blockAlign 		= samples[bank][samplenum].blockAlign;
	sample_undo_buffer.numChannels 		= samples[bank][samplenum].numChannels;
	sample_undo_buffer.sampleByteSize 	= samples[bank][samplenum].sampleByteSize;
	sample_undo_buffer.sampleRate 		= samples[bank][samplenum].sampleRate;
	sample_undo_buffer.sampleSize 		= samples[bank][samplenum].sampleSize;
	sample_undo_buffer.startOfData 		= samples[bank][samplenum].startOfData;
	sample_undo_buffer.PCM 				= samples[bank][samplenum].PCM;

	sample_undo_buffer.inst_size 		= samples[bank][samplenum].inst_size  ;//& 0xFFFFFFF8;
	sample_undo_buffer.inst_start 		= samples[bank][samplenum].inst_start  ;//& 0xFFFFFFF8;
	sample_undo_buffer.inst_end 		= samples[bank][samplenum].inst_end  ;//& 0xFFFFFFF8;

}

//Restores sample[][] to the undo-state buffer
//
void restore_undo_state(uint8_t bank, uint8_t samplenum)
{
	str_cpy(samples[bank][samplenum].filename,  sample_undo_buffer.filename);
	samples[bank][samplenum].blockAlign 		= sample_undo_buffer.blockAlign;
	samples[bank][samplenum].numChannels 		= sample_undo_buffer.numChannels;
	samples[bank][samplenum].sampleByteSize 	= sample_undo_buffer.sampleByteSize;
	samples[bank][samplenum].sampleRate 		= sample_undo_buffer.sampleRate;
	samples[bank][samplenum].sampleSize 		= sample_undo_buffer.sampleSize;
	samples[bank][samplenum].startOfData 		= sample_undo_buffer.startOfData;
	samples[bank][samplenum].PCM 				= sample_undo_buffer.PCM;

	samples[bank][samplenum].inst_size 			= sample_undo_buffer.inst_size  ;//& 0xFFFFFFF8;
	samples[bank][samplenum].inst_start 		= sample_undo_buffer.inst_start  ;//& 0xFFFFFFF8;
	samples[bank][samplenum].inst_end 			= sample_undo_buffer.inst_end  ;//& 0xFFFFFFF8;

}

void enter_edit_mode(void)
{
	global_mode[EDIT_MODE] = 1;
	scrubbed_in_edit = 0;

	cached_looping 	= i_param[0][LOOPING];
	cached_rev 		= i_param[0][REV];

	if (play_state[0] == SILENT || play_state[0] == PLAY_FADEDOWN)
		cached_play_state = 0;
	else
		cached_play_state = 1;
}

void exit_edit_mode(void)
{

	global_mode[EDIT_MODE] = 0;

	i_param[0][LOOPING] = cached_looping;
	i_param[0][REV]		= cached_rev;

	if (scrubbed_in_edit)
	{
		if (cached_play_state)	flags[Play1Trig] = 1;
		else if (play_state[0] == PREBUFFERING) 	play_state[0] = SILENT;
		else if (play_state[0] != SILENT){
			/*if (cached_looping)						play_state[0] = RETRIG_FADEDOWN;
			else 									*/play_state[0] = PLAY_FADEDOWN;
		}
	}
}



//Copies sample header info from src to dst
//dst = src
void copy_sample(uint8_t dst_bank, uint8_t dst_sample, uint8_t src_bank, uint8_t src_sample)
{
	str_cpy(samples[dst_bank][dst_sample].filename,   samples[src_bank][src_sample].filename);
	samples[dst_bank][dst_sample].blockAlign 		= samples[src_bank][src_sample].blockAlign;
	samples[dst_bank][dst_sample].numChannels 		= samples[src_bank][src_sample].numChannels;
	samples[dst_bank][dst_sample].sampleByteSize 	= samples[src_bank][src_sample].sampleByteSize;
	samples[dst_bank][dst_sample].sampleRate 		= samples[src_bank][src_sample].sampleRate;
	samples[dst_bank][dst_sample].sampleSize 		= samples[src_bank][src_sample].sampleSize;
	samples[dst_bank][dst_sample].startOfData 		= samples[src_bank][src_sample].startOfData;
	samples[dst_bank][dst_sample].PCM 				= samples[src_bank][src_sample].PCM;

	samples[dst_bank][dst_sample].inst_size 		= samples[src_bank][src_sample].inst_size  ;//& 0xFFFFFFF8;
	samples[dst_bank][dst_sample].inst_start 		= samples[src_bank][src_sample].inst_start  ;//& 0xFFFFFFF8;
	samples[dst_bank][dst_sample].inst_end 			= samples[src_bank][src_sample].inst_end  ;//& 0xFFFFFFF8;
}




void set_sample_gain(Sample *s_sample, float gain)
{
	if (gain >= 5.1) 		gain = 5.1f;
	else if (gain <= 0.1)	gain = 0.1f;

	s_sample->inst_gain = gain;

}

//sets the trim start point between 0 and 100ms before the end of the sample file
// void set_sample_trim_start(Sample *s_sample, float coarse, float fine)
// {
// 	uint32_t trimstart;
// 	int32_t fine_trim;

// 	if (coarse >= 1.0f) 				trimstart = s_sample->sampleSize - 4420;
// 	else if (coarse <= (1.0/4096.0))	trimstart = 0;
// 	else 								trimstart = (s_sample->sampleSize - 4420) * coarse;

// 	fine_trim = fine * s_sample->sampleRate * s_sample->blockAlign * 0.5; // +/- 500ms

// 	if ((-1.0*fine_trim) > trimstart) trimstart = 0;
// 	else trimstart += fine_trim;

// 	s_sample->inst_start = align_addr(trimstart, s_sample->blockAlign);

// 	//Clip inst_end to the sampleSize (but keep inst_size the same)
// 	if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
// 		s_sample->inst_end = s_sample->sampleSize;
// 	else
// 		s_sample->inst_end = s_sample->inst_start + s_sample->inst_size;

// 	s_sample->inst_end = align_addr(s_sample->inst_end, s_sample->blockAlign);

// }

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



// void set_sample_trim_size(Sample *s_sample, float coarse)
// {
// 	uint32_t trimsize;
// 	int32_t fine_trim;


// 	if (coarse >= 1.0f) 				trimsize = s_sample->sampleSize;
// 	else if (coarse <= (1.0/4096.0))	trimsize = 4420;
// 	else 								trimsize = (s_sample->sampleSize - 4420) * coarse + 4420;

// 	s_sample->inst_size = align_addr(trimsize, s_sample->blockAlign);

// 	//Set inst_end to start+size and clip it to the sampleSize
// 	if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
// 		s_sample->inst_end = s_sample->sampleSize;
// 	else
// 		s_sample->inst_end = (s_sample->inst_start + s_sample->inst_size);

// 	s_sample->inst_end = align_addr(s_sample->inst_end, s_sample->blockAlign);


// }

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




