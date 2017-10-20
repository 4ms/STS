/* assignment_mode.c */
#include "globals.h"
#include "params.h"

#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "str_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "audio_util.h"
#include "edit_mode.h"
#include "bank.h"
#include "sample_file.h"
#include "sts_fs_index.h"
#include "adc.h"
#include "button_knob_combo.h"




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


int16_t 				editmode_latched_potadc[NUM_POT_ADCS];
extern int16_t 			bracketed_potadc[NUM_POT_ADCS];

extern enum 			g_Errors g_error;
extern uint8_t			i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 			flags[NUM_FLAGS];
extern uint8_t 			global_mode[NUM_GLOBAL_MODES];

extern Sample 			samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];
extern enum 			PlayStates play_state[NUM_PLAY_CHAN];

extern ButtonKnobCombo 	g_button_knob_combo[NUM_BUTTON_KNOB_COMBO_BUTTONS][NUM_BUTTON_KNOB_COMBO_KNOBS];

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
	uint8_t first_filled_slot, i;
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
			// Change bank if necessary
			// or if we're not at the beginning of the bank,
			// just go to the beginning of the bank

			// If we're at the beginning of the unassigned scan,
			// go to the last enabled bank 
			if (cur_assign_bank==0xFF && cur_assign_sample==0)
			{
				f_closedir(&root_dir);
				f_closedir(&assign_dir);
				cur_assign_bank = prev_enabled_bank_0xFF(cur_assign_bank);
			}

			// If we're in a normal bank,
			// The the beginning of the bank is defined as the first filled slot (not slot 0)
			// So, check if we're on the first filled slot, and go back a bank
			else
			if (cur_assign_bank<0xFF)
			{
				first_filled_slot = cur_assign_sample;
				for (i=0; i<NUM_SAMPLES_PER_BANK; i++)
					{if (is_wav(samples[cur_assign_bank][i].filename)){first_filled_slot=i;	break;}}

				if (cur_assign_sample <= first_filled_slot)
					cur_assign_bank = prev_enabled_bank_0xFF(cur_assign_bank);
			}

			// Go to the beginning of the bank we're in

			// Unassigned 'bank'
			if (cur_assign_bank == 0xFF)
			{
				res = init_unassigned_scan(&undo_sample, undo_samplenum, undo_banknum);

				if (res!=FR_OK) {exit_assignment_mode(); return;} //unable to enter assignment mode

				found_sample = next_unassigned_sample();
			}

			//Assigned bank
			else
			{
				cur_assign_sample = -1; //will wrap around to 0 when we first call next_assigned_sample()
				cur_assign_state = ASSIGN_USED;

				found_sample = next_assigned_sample();
			}
		}


		if (found_sample)
		{
			flags[UndoSampleDiffers] = 1;

			flags[ForceFileReload1] = 1;
			flags[Play1Trig]		= 1;

			flags[PlaySample1Changed_valid] = 1;
			flags[PlaySample1Changed_empty] = 0;

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

	// Return 1 immediately if we've already entered assignment_mode
	if (global_mode[ASSIGN_MODE]) return 1;

	//Enter assignment mode
	global_mode[ASSIGN_MODE] = 1;

	//Initialize the scan of unassigned samples
	res = init_unassigned_scan(&(samples[i_param[0][BANK]][i_param[0][SAMPLE]]), i_param[0][SAMPLE], i_param[0][BANK]);

	if (res!=FR_OK) {exit_assignment_mode(); return(0);} //unable to enter assignment mode

	//Make a copy of the current sample info, so we can undo our changes later
	//FixMe: Can we remove this, since we already saved the undo state when we entered EDIT_MODE?
	save_undo_state(i_param[0][BANK], i_param[0][SAMPLE]);

	return 1; //success
}

//
//This is typically called if we change sample or bank
//
void exit_assignment_mode(void)
{		
	global_mode[ASSIGN_MODE]	= 0;
	f_closedir(&root_dir);
	f_closedir(&assign_dir);

	flags[UndoSampleExists]		= 0;
	flags[UndoSampleDiffers]	= 0;
}


FRESULT init_unassigned_scan(Sample *s_sample, uint8_t samplenum, uint8_t banknum)
{
	char tmp[_MAX_LFN];

	//Unassigned samples represented by bank of 0xFF
	cur_assign_bank = 0xFF;
	cur_assign_sample = -1;

	//If the slot is empty, look for a filled slot in this bank
	samplenum=0;
	while (!is_wav(s_sample->filename) && samplenum<NUM_SAMPLES_PER_BANK)
	{
		s_sample = &(samples[banknum][samplenum++]);
	}

	//Use the filename to determine the folder we should start scanning in
	if (is_wav(s_sample->filename))
	{
		//Get the folder path from the samples's filename
		if (str_split(s_sample->filename, '/', cur_assign_bank_path, tmp))
			cur_assign_bank_path[str_len(cur_assign_bank_path)-1]='\0'; //remove trailing slash
		else
			cur_assign_bank_path[0]='\0'; //filename contained no slash: use root dir

		cur_assign_state = ASSIGN_IN_FOLDER;
	}
	else
	{
		//The entire bank of s_sample is empty:
		//See if the folder with the default color name exists
		bank_to_color(banknum, cur_assign_bank_path);

		if (f_opendir(&assign_dir, cur_assign_bank_path) == FR_OK)
		{
			cur_assign_state = ASSIGN_IN_FOLDER;
			return(FR_OK);
		} 
		else
		//If there's no default color name folder, then start the assignment scan in the root dir
		{
			cur_assign_state = ASSIGN_UNUSED_IN_ROOT;
			cur_assign_bank_path[0]='\0'; //root
		}

	}

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
	uint8_t	bank;
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
			if (cur_assign_state==ASSIGN_IN_FOLDER)
			{
				//We hit the end of the original sample's folder: 
				//Go through the entire filesystem

				//Close the assign_dir
				f_closedir(&assign_dir);

				// If our bank path isn't already root, then try scanning root
				// Otherwise skip root and scan the folders in root
				if (cur_assign_bank_path[0]!='\0')
					cur_assign_state = ASSIGN_UNUSED_IN_ROOT;
				else
					cur_assign_state = ASSIGN_UNUSED_IN_FS;
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

		is_file_already_assigned = 0;

		//Skip this file if it's already assigned somewhere
		//...except if we're looking in our folder, then we actually want to go through the whole folder, assigned or not
		if (cur_assign_state!=ASSIGN_IN_FOLDER)
		{
			bank=next_enabled_bank(MAX_NUM_BANKS);
			if (find_filename_in_all_banks(bank, filepath) != 0xFF)
				is_file_already_assigned = 1;
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
	res = init_unassigned_scan(&undo_sample, undo_samplenum, undo_banknum);
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

	undo_sample.inst_size 		= samples[bank][samplenum].inst_size;
	undo_sample.inst_start 		= samples[bank][samplenum].inst_start;
	undo_sample.inst_end 		= samples[bank][samplenum].inst_end;
	undo_sample.inst_gain 		= samples[bank][samplenum].inst_gain;

	undo_banknum				= bank;
	undo_samplenum 				= samplenum;

	flags[UndoSampleExists] 	= 1;
	flags[UndoSampleDiffers] 	= 0;
}

//Restores sample[][] to the undo-state buffer
//
uint8_t restore_undo_state(uint8_t bank, uint8_t samplenum)
{
	if (bank==undo_banknum && samplenum==undo_samplenum)
	{
		str_cpy(samples[undo_banknum][undo_samplenum].filename,   undo_sample.filename);
		samples[undo_banknum][undo_samplenum].blockAlign 		= undo_sample.blockAlign;
		samples[undo_banknum][undo_samplenum].numChannels 		= undo_sample.numChannels;
		samples[undo_banknum][undo_samplenum].sampleByteSize 	= undo_sample.sampleByteSize;
		samples[undo_banknum][undo_samplenum].sampleRate 		= undo_sample.sampleRate;
		samples[undo_banknum][undo_samplenum].sampleSize 		= undo_sample.sampleSize;
		samples[undo_banknum][undo_samplenum].startOfData 		= undo_sample.startOfData;
		samples[undo_banknum][undo_samplenum].PCM 				= undo_sample.PCM;

		samples[undo_banknum][undo_samplenum].inst_size 		= undo_sample.inst_size;
		samples[undo_banknum][undo_samplenum].inst_start 		= undo_sample.inst_start;
		samples[undo_banknum][undo_samplenum].inst_end 			= undo_sample.inst_end;
		samples[undo_banknum][undo_samplenum].inst_gain 		= undo_sample.inst_gain;

		flags[ForceFileReload1] 	= 1;
		flags[Play1Trig]			= 1;
		flags[UndoSampleExists] 	= 0;
		flags[UndoSampleDiffers]	= 0;

		return(1); //undo restored successfully
	} else return(0); //not in the right sample/bank to preform an undo
}





void enter_edit_mode(void)
{
	//If the current bank is different from the undo_banknum and we're in assign mode,
	//exit assignment mode
	 if (global_mode[ASSIGN_MODE] && i_param[0][BANK] != undo_banknum)
	 	exit_assignment_mode();

	// Latch pot values, so we can do alternative functions with the pots without distupting the original value

	if (g_button_knob_combo[bkc_Edit][bkc_Sample2].combo_state 	== COMBO_INACTIVE)
	{
		g_button_knob_combo[bkc_Edit][bkc_Sample2].combo_state 	= COMBO_ACTIVE;
		g_button_knob_combo[bkc_Edit][bkc_Sample2].latched_value = bracketed_potadc[SAMPLE2_POT];
	}

	if (g_button_knob_combo[bkc_Edit][bkc_Length2].combo_state 	== COMBO_INACTIVE)
	{
		g_button_knob_combo[bkc_Edit][bkc_Length2].combo_state 	= COMBO_ACTIVE;
		g_button_knob_combo[bkc_Edit][bkc_Length2].latched_value = bracketed_potadc[LENGTH2_POT];
	}

	if (g_button_knob_combo[bkc_Edit][bkc_StartPos2].combo_state == COMBO_INACTIVE)
	{
		g_button_knob_combo[bkc_Edit][bkc_StartPos2].combo_state = COMBO_ACTIVE;
		g_button_knob_combo[bkc_Edit][bkc_StartPos2].latched_value = bracketed_potadc[START2_POT];
	}

	if (g_button_knob_combo[bkc_Edit][bkc_RecSample].combo_state == COMBO_INACTIVE)
	{
		g_button_knob_combo[bkc_Edit][bkc_RecSample].latched_value = bracketed_potadc[RECSAMPLE_POT];
	}

	global_mode[EDIT_MODE] = 1;
	scrubbed_in_edit = 0;

	cached_looping 	= i_param[0][LOOPING];
	cached_rev 		= i_param[0][REV];

	if (play_state[0] == SILENT || play_state[0] == PLAY_FADEDOWN)
		cached_play_state = 0;
	else
		cached_play_state = 1;


	//Save an undo state unless we've already saved one for this bank/sample
	
	if (!flags[UndoSampleExists] || undo_banknum != i_param[0][BANK] || undo_samplenum != i_param[0][SAMPLE])
		save_undo_state(i_param[0][BANK], i_param[0][SAMPLE]);
}

void exit_edit_mode(void)
{

	if (global_mode[EDIT_MODE])
	{
		//Continue latching the pot values
		//This remains latched until pot is moved
		g_button_knob_combo[bkc_Edit][bkc_Sample2].combo_state 		= COMBO_LATCHED;
		g_button_knob_combo[bkc_Edit][bkc_Length2].combo_state 		= COMBO_LATCHED;
		g_button_knob_combo[bkc_Edit][bkc_StartPos2].combo_state 	= COMBO_LATCHED;

		//RecSample pot is not latched, its value returns to the knob position immediately
		g_button_knob_combo[bkc_Edit][bkc_RecSample].combo_state 	= COMBO_INACTIVE;

		global_mode[EDIT_MODE] = 0;

		i_param[0][LOOPING] = cached_looping;
		i_param[0][REV]		= cached_rev;

		if (scrubbed_in_edit)
		{
			if (cached_play_state)	flags[Play1Trig] = 1;
			else if (play_state[0] == PREBUFFERING) 	play_state[0] = SILENT;
			else if (play_state[0] != SILENT){
				play_state[0] = PLAY_FADEDOWN;
			}
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

	samples[dst_bank][dst_sample].inst_size 		= samples[src_bank][src_sample].inst_size;
	samples[dst_bank][dst_sample].inst_start 		= samples[src_bank][src_sample].inst_start;
	samples[dst_bank][dst_sample].inst_end 			= samples[src_bank][src_sample].inst_end;
	samples[dst_bank][dst_sample].inst_gain 		= samples[src_bank][src_sample].inst_gain;
}




void set_sample_gain(Sample *s_sample, float gain)
{
	if (gain >= 5.1) 		gain = 5.1f;
	else if (gain <= 0.1)	gain = 0.1f;

	s_sample->inst_gain = gain;

	flags[UndoSampleDiffers] = 1;
}


#define TRIM_MIN_SIZE 4412
#define TRIM_FINE_ADJ_AMOUNT 1

void nudge_trim_start(Sample *s_sample, int32_t fine)
{
	int32_t trimdelta;

	float sample_size_secs;
	uint32_t samples_per_adc;

	scrubbed_in_edit = 1;
	flags[UndoSampleDiffers] = 1;

	sample_size_secs = (float)s_sample->sampleSize / (float)(s_sample->sampleRate * s_sample->blockAlign); // Bytes / (samples/sec * bytes/sample)

	if (sample_size_secs > TRIM_FINE_ADJ_AMOUNT)
		samples_per_adc = (TRIM_FINE_ADJ_AMOUNT * s_sample->sampleRate * s_sample->blockAlign) >> 12;
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

	scrubbed_in_edit = 1;
	flags[UndoSampleDiffers] = 1;

	sample_size_secs = (float)s_sample->sampleSize / (float)(s_sample->sampleRate * s_sample->blockAlign); // Bytes / (samples/sec * bytes/sample)

	if (sample_size_secs > TRIM_FINE_ADJ_AMOUNT)
		samples_per_adc = (TRIM_FINE_ADJ_AMOUNT * s_sample->sampleRate * s_sample->blockAlign) >> 12;
	else if (s_sample->sampleSize >= 32768)
		samples_per_adc = s_sample->sampleSize >> 14;
	else
		samples_per_adc = 1;

	trimdelta = samples_per_adc * fine * fine;
	if (fine < 0) trimdelta *= -1;

	//clip ->inst_size+trimdelta to TRIM_MIN_SIZE..inst_size
	//Is there a more efficient way of doing this?
	//We don't want to convert ->sampleSize or ->instSize to an int, it must stay unsigned
	if (trimdelta<0)
	{
		trimdelta = -1 * trimdelta;
		if (s_sample->inst_size > (trimdelta + TRIM_MIN_SIZE))
			s_sample->inst_size -= trimdelta;
		else
			s_sample->inst_size = TRIM_MIN_SIZE;
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




