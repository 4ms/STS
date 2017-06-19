#include "globals.h"
#include "params.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "file_util.h"
#include "sts_fs_index.h"
#include "sts_filesystem.h"
#include "bank.h"


Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

uint8_t bank_status[MAX_NUM_BANKS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];

// extern enum g_Errors g_error;
// extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
// extern uint8_t 	flags[NUM_FLAGS];
// enum PlayStates play_state				[NUM_PLAY_CHAN];



uint8_t write_sampleindex_file(void)
{

	FIL 		temp_file;
	FRESULT 	res;
	uint32_t 	sz, bw;
	uint8_t 	i, j;
	char 		b_color[10];
	char 		path[_MAX_LFN+1];
	char 		bank_path[_MAX_LFN+1];
	char 		*filename_ptr;
	char 		t_filename_ptr[_MAX_LFN+1];

	// CREATE INDEX FILE - TXT
	// previous index files are replaced
	// f_open, f_sync and f_rpintf are functions from 
	// ...the module application interface
	// ...for the Generic FAT file system module  (ff.h)
	
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
	for (i=0; i<MAX_NUM_BANKS; i++)
	{

		// Print bank Color to index file
		bank_to_color(i, b_color);
 		f_printf(&temp_file, "--------------------\n%s\n--------------------\n", b_color);

		// For each sample in bank
		for (j=0; j<NUM_SAMPLES_PER_BANK; j++)
		{
 		
			// split path and filename			
			filename_ptr = t_filename_ptr;
			filename_ptr = str_rstr(samples[i][j].filename, '/', path);
			
			// Skip empty slots
			if (filename_ptr[0]!='\0')
			{

				// Print bank path to index file
				if(j==0)
				{
					str_cpy(bank_path, path);
					f_printf(&temp_file, "path: %s\n\n", path);
				}
				
				// Print sample name to index file
				if (str_cmp(path, bank_path)) f_printf(&temp_file, "%s\n",filename_ptr);
				else f_printf(&temp_file, "%s\n", samples[i][j].filename);

				// write unedditable sample info to index file
				switch (samples[i][j].numChannels)
				{
					case 1:
						f_printf(&temp_file, "sample info: %dHz, %d-bit, mono,   %d samples\n", samples[i][j].sampleRate, samples[i][j].sampleByteSize*8, samples[i][j].sampleSize);
					break;
					case 2:
						f_printf(&temp_file, "sample info: %dHz, %d-bit, stereo, %d samples\n", samples[i][j].sampleRate, samples[i][j].sampleByteSize*8, samples[i][j].sampleSize);
					break;
				}

				// write edditable sample info to index file
		 		f_printf(&temp_file, "- Sample slot: %d\n- play start: %d\n- play size: %d\n- play gain(%%): %d\n\n", j+1, samples[i][j].inst_start, samples[i][j].inst_size, (int)(100 * samples[i][j].inst_gain));
			}
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

	// // Variables to load from file
	// // ... and to initialize if missing 
	// // ... or out of bounds
	// samples[i][j].inst_start
	// samples[i][j].inst_size
	// (int)(100 * samples[i][j].inst_gain
	// global_mode[STEREO_MODE]

	// // Varialbles to check and update if incorrect
	// samples[i][j].filename

	// // Variables to systematically update
	// samples[i][j].sampleRate
	// samples[i][j].sampleByteSize*8
	// samples[i][j].sampleSize


	FIL 		temp_file, temp_wav_file;
	FRESULT 	res;
	char 		read_buffer[_MAX_LFN+1], folder_path[_MAX_LFN+1], sample_path[_MAX_LFN+1];
	uint8_t		cur_bank=0, cur_sample=0, arm_bank=0, arm_path=0, arm_data=0, loaded_header=0, read_name=0;
	uint32_t	num_buff=UINT32_MAX;
	// char 		*token;
	// char 		t_token[_MAX_LFN+1];
	char 		token[_MAX_LFN+1];
	uint8_t		fopen_flag	  = 0;
	uint8_t		rewrite_index = 0;
	uint8_t 	force_reload  = 2;

	// read_buffer[0]='\0'; 
	// folder_path[0]='\0'; 
	// sample_path[0]='\0';

	// Open sample index file
	res = f_open(&temp_file,"sample_index.txt", FA_READ);

	// rise flags if index can't be opened/created
	if (res != FR_OK) return(1); //file not found

	// ToDo: Check is f_sync is necessary here
	// write cached information of the file  to the volume	
	f_sync(&temp_file);
	
	//
	// token = t_token;

	// until we reach the eof
	while (!f_eof(&temp_file))
	{
		// Read next line
		f_gets(read_buffer, _MAX_LFN+1, &temp_file);

		// Remove /n from buffer
		read_buffer[str_len(read_buffer)-1]=0;

		// tokenize at spaces
		// if((read_name!=1) && (arm_data==0)) str_tok(read_buffer,' ', token);
		if((read_name!=1) && (arm_data==0)) str_tok(read_buffer,' ', token);
		else str_cpy(token, read_buffer);

		// While token isn't empty
		while(token[0]!='\0')
		{
			// Load bank data
			// ToDo: see if computing repeated str_cmp upstream helps saving time
			if (!loaded_header)
			{
				if 		( str_cmp(token,"--------------------") && !arm_bank) {arm_bank++; 					 			str_tok(read_buffer,' ', token);}
				else if	(!str_cmp(token,"--------------------") &&  arm_bank) {cur_bank=color_to_bank(token); 			str_tok(read_buffer,' ', token);}
				else if ( str_cmp(token,"--------------------") &&  arm_bank) {arm_bank=0; 					 			str_tok(read_buffer,' ', token);}

				// read folder path
				else if (str_cmp(token, "path:")) 
				{
					str_cpy(folder_path, read_buffer);
					// ToDo: check for missing / at end of path, or extra / at begining
					arm_path=0;
					loaded_header = 1;
					cur_sample = 0;
					token[0]='\0';
					read_name = 1; 
				}
			}

			// Load file name
			else if (!str_cmp(token,"--------------------")&&(read_name==1)) 
			{

				// update file path with file name
				str_cat(sample_path ,folder_path, token);

				token[0] = '\0'; 
				read_name++;
			}

			// Load .wav header data information and play data
			else if ((!str_cmp(token,"--------------------"))&&(read_name>1))
			{
				
				// if there is a number in the current line
				num_buff = str_xt_int(read_buffer);
				if (num_buff != UINT32_MAX)
				{
					
					// arm data loading
					// if the line startes with - and data loading isn't armed
					if 		((read_buffer[0]!='-')&&(!arm_data))	{arm_data=1; token[0] = '\0';}
					
					// load header data from .wav file
					// if the data loading has just been armed
					else if  (arm_data==1) 			
					{
						// Update sample bank number
						cur_sample=num_buff-1;
						
						// open sample file
						// FixMe: Temporarily disabled fopen_checked()
						// fopen_flag = fopen_checked(&temp_wav_file, sample_path);
						f_open(&temp_wav_file,sample_path, FA_READ);
						fopen_flag=0;

						// if file was opened
						if (fopen_flag<2)
						{

							// update samples structure with file path
							str_cpy(samples[cur_bank][cur_sample].filename, sample_path);

							// if file was opened, but not from given path
							if (fopen_flag==1)
							{
								// request to rewrite index (from samples struct) 
								// ... at the end of index read
								rewrite_index =1;
							}
							
							// At least a sample was loaded
							force_reload = 0;

							// load sample information from .wav header	
							load_sample_header(&samples[cur_bank][cur_sample], &temp_wav_file); // res=

							// close wav file
							f_close(&temp_wav_file);

							arm_data++; token[0] = '\0';
						}  

						// if file cannot be found on the SD card
						else if (fopen_flag==2)
						{
							// write empty filename to samples struct element
							// ... so this sample is skipped at the next index write
							samples[cur_bank][cur_sample].filename[0]='\0';

							// request to rewrite index (from samples struct) 
							// ... at the end of index read
							rewrite_index =1;

							// skip loading sample play information
							arm_data=0; token[0] = '\0'; read_name = 1; break;						
						}
					}

					// load sample play information from index file
					// if data headed has been loaded

					// ToDo: make arm-data numbers #define so code is easier to read
					// ToDo: Put limit on play start based on sample size - rewrite index if updated
					else if (arm_data==2) 			{samples[cur_bank][cur_sample].inst_start=num_buff; 									arm_data++; token[0] = '\0';}

					// ToDo: Put limit on play size  based on sample size - rewrite index if updated
					else if (arm_data==3) 			{samples[cur_bank][cur_sample].inst_size=num_buff; 										arm_data++; token[0] = '\0';}

					// ToDo: Put limit on gain value 0 to 100 - rewrite index if updated					
					else if (arm_data==4) 			{samples[cur_bank][cur_sample].inst_gain=num_buff/100.0f; 								arm_data=0; token[0] = '\0'; read_name = 1; break;}
				}
			}

			// Move to next bank
			else if (str_cmp(token,"--------------------"))
			{
				read_name 		= 0; 
				loaded_header	= 0;
				read_buffer[0]  = '\0';
			}			
		}
	}

	// close sample index file
	f_close(&temp_file);

	//  rewrite sample index as needed
	if(rewrite_index)
	{
		// write_sampleindex_file();
	}

	// Assign samples in SD card to (previously empty) sample index
	return(force_reload);	// OK if at least a sample was loaded
}