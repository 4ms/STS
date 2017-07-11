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
int32_t 				cur_assign_sample=0; 

Sample					undo_sample;
uint8_t					undo_samplenum, undo_banknum;

 

uint8_t					cached_looping;
uint8_t 				cached_rev;
uint8_t 				cached_play_state;
uint8_t 				scrubbed_in_edit;


extern enum 			g_Errors g_error;
extern uint8_t			i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 			flags[NUM_FLAGS];
extern uint8_t 			global_mode[NUM_GLOBAL_MODES];

extern Sample 			samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];
extern enum 			PlayStates play_state[NUM_PLAY_CHAN];

DIR root_dir;

//Wrapper for entering assignment mode,
//going to the next assigned or unassigned sample
//and handling the playback and visual feedback flags
//
//direction == 1 means go forward one sample
//direction == 2 means go back one bank
//
void do_assignment(uint8_t direction)
{
	uint8_t found_sample=0;
	FRESULT res;

	if (enter_assignment_mode())
	{
		//Next sample:
		if (direction==1)
		{
			// browsing unused samples (starting from current folder)
			if (cur_assign_bank == 0xFF)
				found_sample = next_unassigned_sample();

			// browsing used samples in bank order
			else
				found_sample = next_assigned_sample();
		}

		//Previous bank:
		else if (direction==2)
		{
			// If we're at the beginning of the bank,
			// Go to the previous bank
			if (cur_assign_sample==0)
				cur_assign_bank = prev_enabled_bank_0xFF(cur_assign_bank);

			//Now, go to the beginning of the bank we're in:

			//Unassigned 'bank': initialize properly
			if (cur_assign_bank == 0xFF)
			{
				res = init_unassigned_scan(&undo_sample);

				if (res!=FR_OK) {exit_assignment_mode(); return;} //unable to enter assignment mode

				found_sample = next_unassigned_sample();
			}

			//Assigned bank: de-init unassigned 'bank', and initialize
			else
			{
				f_closedir(&root_dir);
				f_closedir(&assign_dir);

				cur_assign_sample = -1; //will wrap around to 0 when we first call next_assigned_sample()
				cur_assign_state = ASSIGN_USED;

				found_sample = next_assigned_sample();
			}
		}

		if (found_sample)
		{
			flags[ForceFileReload1] = 1;
			flags[Play1Trig]		= 1;

			if (direction==1) 	flags[AssignedNextSample] 	= 10;
			else
			if (direction==2) 	flags[AssignedPrevBank] 	= 10;
		}
	} 

}



// Assignment mode:
// First we set cur_assign_bank to 0xFF 
// Then we go through all unassigned files in the current sample's folder
// Set cur_assign_bank to 0xFE
// Then we go through the entire SDCard for all unassigned samples (only look 1 folder level deep, for now)
// Then we go through all assigned samples in the banks in order starting from White
//
uint8_t enter_assignment_mode(void)
{
	FRESULT res;

	// Return 1 if we've already entered assignment_mode
	if (global_mode[ASSIGN_MODE]) return 1;

	//Enter assignment mode
	global_mode[ASSIGN_MODE] = 1;

	//Initialize the scan of unassigned samples
	res = init_unassigned_scan(&(samples[i_param[0][BANK]][i_param[0][SAMPLE]]));

	if (res!=FR_OK) {exit_assignment_mode(); return(0);} //unable to enter assignment mode

	//Make a copy of the current sample info, so we can undo our changes later
	save_undo_state(i_param[0][BANK], i_param[0][SAMPLE]);

	return 1; //success
}

void exit_assignment_mode(void)
{		
	global_mode[ASSIGN_MODE] = 0;
	f_closedir(&root_dir);
	f_closedir(&assign_dir);
	flags[UndoSampleExists] = 0;
}

FRESULT init_unassigned_scan(Sample *s_sample)
{
	char tmp[_MAX_LFN];

	//Start with the unassigned samples
	cur_assign_bank = 0xFF;
	cur_assign_sample = -1;

	//Start with the current sample's folder
	cur_assign_state = ASSIGN_UNUSED_IN_FOLDER;

	//Get the folder path from the samples's filename

	if (str_split(s_sample->filename, '/', cur_assign_bank_path, tmp))
		cur_assign_bank_path[str_len(cur_assign_bank_path)-1]='\0'; //remove trailing slash
	else
		cur_assign_bank_path[0]='\0'; //filename contained no slash: use root dir

	//Verify the folder exists and can be opened
	//Leave it open, so we can browse it with next_unassigned_sample
	return (f_opendir(&assign_dir, cur_assign_bank_path));

}

//Find the next file in the folder, 
//and when we get to the end we go to the next bank
//and after that we load the original file name
//then continue looking for the next wav file
//The benefit of this method would be that we aren't just limited to 32 samples per folder, and it uses less memory
uint8_t next_unassigned_sample(void)
{
	uint8_t	is_unassigned_file_found;
	uint8_t	is_file_already_assigned;
	char 	filename[_MAX_LFN];
	char 	filepath[_MAX_LFN];
	FRESULT	res;
	FIL 	temp_file;
	uint8_t	i;
	uint8_t	bank, orig_bank;
	uint32_t max_folders_bailout=1024;

	play_state[0]=SILENT;
	play_state[1]=SILENT;

	//
	//Search for files in the bank we are currently checking (assign_dir)
	//
	//First, go through all files that aren't being used in this bank
	//Then, go through the remaining files in the directory
	//

	is_unassigned_file_found = 0;
	while (!is_unassigned_file_found)
	{
		res = find_next_ext_in_dir(&assign_dir, ".wav", filename);

		if (res!=FR_OK && res!=0xFF)
			return(0); //filesystem error reading directory, abort

		if (res==0xFF) //no more .wav files found
		{
			if (cur_assign_state==ASSIGN_UNUSED_IN_FOLDER)
			{
				//We hit the end of the original sample's folder: 
				//Go through the entire filesystem

				//Close the assign_dir
				f_closedir(&assign_dir);

				cur_assign_state=ASSIGN_UNUSED_IN_ROOT;
			} 
			
			if (cur_assign_state==ASSIGN_UNUSED_IN_FS || cur_assign_state==ASSIGN_UNUSED_IN_ROOT)
			{
				// Hit the end of the folder:
				// Find the next folder in the root

				f_closedir(&assign_dir);
				while (max_folders_bailout--)
				{
					//Searching in root dir: set path to root
					if (cur_assign_state==ASSIGN_UNUSED_IN_ROOT)
					{
						cur_assign_bank_path[0]='\0';

						//Set the state to check the folders in the FS, so that the next time 
						//the while loop executes, we'll be ready
						cur_assign_state=ASSIGN_UNUSED_IN_FS;

						//Reset the root_dir for use with get_next_dir()
						root_dir.obj.fs = 0;
					}

					//Searching in folders: Try the next folder inside the root_dir
					else if (cur_assign_state==ASSIGN_UNUSED_IN_FS)
					{
						res = get_next_dir(&root_dir, "", cur_assign_bank_path);

						// No more folders in the root_dir!
						// Set cur_assign_bank to the first enabled bank
						// And call next_assigned_sample()
						if (res != FR_OK) 
						{
							f_closedir(&root_dir);
							f_closedir(&assign_dir);

							init_assigned_scan();
							return (next_assigned_sample());
						}
					}

					// Skip the original sample's folder (we already scanned it)
					if (str_split(undo_sample.filename,'/', filepath, filename))
							filepath[str_len(filepath)-1]='\0'; //remove trailing slash
					else	filepath[0]='\0'; //filename contained no slash: use root dir

					if (str_cmp(filepath, cur_assign_bank_path)) continue; 

					// Check if the folder can be opened
					res = f_opendir(&assign_dir, cur_assign_bank_path);
					if (res!=FR_OK) {f_closedir(&assign_dir); continue;}

					break; //we found a folder, so exit the inner while loop

				}

				// In case the SD Card is removed, we don't want to search forever, so just bailout
				if (max_folders_bailout==0) return(0);

				// We just found a folder,
				// so now we need to go back to the beginning of the outer while loop
				// and look for a file
				continue;
			}
		}

		// Ignore files with paths that are too long
		if ((str_len(filename) + str_len(cur_assign_bank_path) + 1) >= _MAX_LFN) continue; 

		//.wav file was found:
		//Add the path the beginning of the filename
		
		i = str_len(cur_assign_bank_path);
		str_cpy(filepath, cur_assign_bank_path);
		filepath[i]='/';
		str_cpy(&(filepath[i+1]), filename);

		//See if the file already is assigned to a sample slot in any bank

		//Shortcut: if the file is the original file, then don't bother searching for it
		if (str_cmp(filepath, undo_sample.filename))
			is_file_already_assigned = 1;
		else 
		{
			//Cycle through all enabled banks
			//See if the filepath matches any sample in any bank

			is_file_already_assigned = 0;

			bank=next_enabled_bank(MAX_NUM_BANKS);
			if (find_filename_in_all_banks(bank, filepath) != 0xFF)
				is_file_already_assigned = 1;

			// orig_bank=bank;
			// do{
			// 	if (find_filename_in_bank(bank, filepath) != 0xFF)
			// 		{is_file_already_assigned = 1; break;}

			// 	bank=next_enabled_bank(bank);	
			// } while(bank!=orig_bank);
		}

		if (!is_file_already_assigned)
		{
			//Try opening the file and verifying the header is good
			res = f_open(&temp_file, filepath, FA_READ);

			if (res==FR_OK)
			{
				res = load_sample_header(&samples[i_param[0][BANK]][i_param[0][SAMPLE]], &temp_file);

				if (res==FR_OK)
				{
					// Sucess!! We found a valid file
					// Copy the name to the current sample slot
					str_cpy(samples[i_param[0][BANK]][i_param[0][SAMPLE]].filename, filepath);

					is_unassigned_file_found = 1;
				}
				f_close(&temp_file);

			}
		}
	}

	if (is_unassigned_file_found)
	{
		cur_assign_sample++;
		return(1); //sample found
	}

	else return(0); //no sample found
}


void init_assigned_scan(void)
{
	cur_assign_bank = next_enabled_bank(MAX_NUM_BANKS);
	cur_assign_sample = -1; //will wrap around to 0 when we first call next_assigned_sample()
	cur_assign_state = ASSIGN_USED;
}

// Find next assigned sample in current bank
// if no more assigned samples in current bank, go to next non-empty bank
// if no more non-empty bank set cur_assign_bank to 0xFFFFFFFF so next_unassigned_sample() gets called
// ... and unassigned samples get browsed
uint8_t next_assigned_sample(void)
{
	FRESULT res;

	while ( 1 )
	{
		//Go to the next sample
		cur_assign_sample++;
		if (cur_assign_sample>=NUM_SAMPLES_PER_BANK)
		{
			cur_assign_bank = next_enabled_bank_0xFF(cur_assign_bank);
			cur_assign_sample=0;

			if (cur_assign_bank == 0xFF) break;
		}	

		// Check if this is the original sample we're re-assigning
		// If so, then we want to load the undo_sample
		if (cur_assign_bank==i_param[0][BANK] && cur_assign_sample==i_param[0][SAMPLE])
		{
			restore_undo_state( i_param[0][BANK], i_param[0][SAMPLE] );
			return(1); //sample found
		}

		// See if this sample is valid, and then load in the current slot
		if (is_wav(samples[cur_assign_bank][cur_assign_sample].filename))
		{
			copy_sample( i_param[0][BANK], i_param[0][SAMPLE], cur_assign_bank, cur_assign_sample);
			return(1); //sample found
		}
	}

	// At this point, we've gone through all the assigned samples in all the banks
	// Now switch to unassigned samples
	res = init_unassigned_scan(&undo_sample);
	if (res!=FR_OK) return 0;//failed

	return (next_unassigned_sample());
}

//backs-up sample[][] data to an undo buffer
void save_undo_state(uint8_t bank, uint8_t samplenum)
{
	str_cpy(undo_sample.filename,  samples[bank][samplenum].filename);
	undo_sample.blockAlign 		= samples[bank][samplenum].blockAlign;
	undo_sample.numChannels 	= samples[bank][samplenum].numChannels;
	undo_sample.sampleByteSize 	= samples[bank][samplenum].sampleByteSize;
	undo_sample.sampleRate 		= samples[bank][samplenum].sampleRate;
	undo_sample.sampleSize 		= samples[bank][samplenum].sampleSize;
	undo_sample.startOfData 	= samples[bank][samplenum].startOfData;
	undo_sample.PCM 			= samples[bank][samplenum].PCM;

	undo_sample.inst_size 		= samples[bank][samplenum].inst_size  ;//& 0xFFFFFFF8;
	undo_sample.inst_start 		= samples[bank][samplenum].inst_start  ;//& 0xFFFFFFF8;
	undo_sample.inst_end 		= samples[bank][samplenum].inst_end  ;//& 0xFFFFFFF8;

	undo_banknum			= bank;
	undo_samplenum 			= samplenum;

	flags[UndoSampleExists] = 1;
}

//Restores sample[][] to the undo-state buffer
//
uint8_t restore_undo_state(uint8_t bank, uint8_t samplenum)
{
	if (bank==undo_banknum && samplenum==undo_samplenum)
	{
		str_cpy(samples[undo_banknum][undo_samplenum].filename,  undo_sample.filename);
		samples[undo_banknum][undo_samplenum].blockAlign 		= undo_sample.blockAlign;
		samples[undo_banknum][undo_samplenum].numChannels 		= undo_sample.numChannels;
		samples[undo_banknum][undo_samplenum].sampleByteSize 	= undo_sample.sampleByteSize;
		samples[undo_banknum][undo_samplenum].sampleRate 		= undo_sample.sampleRate;
		samples[undo_banknum][undo_samplenum].sampleSize 		= undo_sample.sampleSize;
		samples[undo_banknum][undo_samplenum].startOfData 		= undo_sample.startOfData;
		samples[undo_banknum][undo_samplenum].PCM 				= undo_sample.PCM;

		samples[undo_banknum][undo_samplenum].inst_size 		= undo_sample.inst_size  ;//& 0xFFFFFFF8;
		samples[undo_banknum][undo_samplenum].inst_start 		= undo_sample.inst_start  ;//& 0xFFFFFFF8;
		samples[undo_banknum][undo_samplenum].inst_end 			= undo_sample.inst_end  ;//& 0xFFFFFFF8;

		flags[ForceFileReload1] = 1;
		flags[Play1Trig]		= 1;
		flags[UndoSampleExists] = 0;

		return(1); //ok
	} else return(0); //not in the right sample/bank to preform an undo
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




