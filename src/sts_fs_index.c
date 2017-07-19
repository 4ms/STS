#include "globals.h"
#include "params.h"
#include "timekeeper.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "sts_fs_index.h"
#include "sts_filesystem.h"
#include "bank.h"
#include "res/LED_palette.h"


Sample 						samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

uint8_t 					bank_status[MAX_NUM_BANKS];

extern uint8_t 				global_mode[NUM_GLOBAL_MODES];
extern uint8_t 				flags[NUM_FLAGS];

// extern enum g_Errors g_error;
// extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
// enum PlayStates play_state				[NUM_PLAY_CHAN];



FRESULT write_sampleindex_file(void)
{
	FIL 		temp_file;
	FRESULT 	res, res_sysdir;
	//uint32_t 	sz, bw;
	uint8_t 	i, j;
	char 		b_color[11];
	char 		bank_path[_MAX_LFN+1];
	char 		filename_ptr[_MAX_LFN+1];
	char 		path[_MAX_LFN+1], bootbakpath[_MAX_LFN+1];

	// CREATE INDEX FILE
	// previous index files are replaced
	// f_open, f_sync and f_rpintf are functions from 
	// ...the module application interface
	// ...for the Generic FAT file system module  (ff.h)

	// Check for system dir
	res_sysdir = check_sys_dir();
	if (res_sysdir!=FR_OK)	return(res_sysdir);

	// WRITE SAMPLES DATA TO INDEX FILE
	else {

		// file index path
		str_cat(path, SYS_DIR_SLASH,SAMPLE_INDEX_FILE);

		// backup index at boot
		if (flags[BootBak])
		{
			str_cat(bootbakpath, SYS_DIR_SLASH, SAMPLE_BOOTBAK_FILE);
			f_rename(path,bootbakpath);
			flags[BootBak]=0;
		}

		// (re)create sample.index file
		// ... and its pointer &temp_file
		res = f_open(&temp_file, path, FA_WRITE | FA_CREATE_ALWAYS); 
		if (res != FR_OK) return(res); 


		// For each bank
		for (i=0; i<MAX_NUM_BANKS; i++)
		{

			// Print bank Color to index file
			bank_to_color(i, b_color);
	 		f_printf(&temp_file, "--------------------\n%s\n--------------------\n", b_color);
			f_sync(&temp_file);

			// For each sample in bank
			for (j=0; j<NUM_SAMPLES_PER_BANK; j++)
			{
	 		
				// split path and filename			
				str_split(samples[i][j].filename, '/', path, filename_ptr);
				
				// Skip empty slots
				if (filename_ptr[0]!='\0')
				{

					// Print bank path to index file
					if(j==0)
					{
						str_cpy(bank_path, path);
						f_printf(&temp_file, "path: %s\n\n", path);
						f_sync(&temp_file);
					}
					
					// Print sample name to index file
					if (str_cmp(path, bank_path)) f_printf(&temp_file, "%s\n",filename_ptr);
					else f_printf(&temp_file, "%s\n", samples[i][j].filename);

					f_sync(&temp_file);

					// write unedditable sample info to index file
					switch (samples[i][j].numChannels)
					{
						case 1:
							f_printf(&temp_file, "sample info: %dHz, %d-bit, mono,   %d samples\n", samples[i][j].sampleRate, samples[i][j].sampleByteSize*8, samples[i][j].sampleSize);
							f_sync(&temp_file);
						break;
						case 2:
							f_printf(&temp_file, "sample info: %dHz, %d-bit, stereo, %d samples\n", samples[i][j].sampleRate, samples[i][j].sampleByteSize*8, samples[i][j].sampleSize);
							f_sync(&temp_file);
						break;
					}

					// write edditable sample info to index file
			 		f_printf(&temp_file, "- Sample slot: %d\n- play start: %d\n- play size: %d\n- play gain(%%): %d\n\n", j+1, samples[i][j].inst_start, samples[i][j].inst_size, (int)(100 * samples[i][j].inst_gain));
					f_sync(&temp_file);
				}
			}
	 		f_printf(&temp_file, "\n");
	 		f_sync(&temp_file);
		}

		// Write global info to file
		f_printf(&temp_file, "Timestamp: %d\n", get_fattime());

		f_printf(&temp_file, EOF_TAG);

			// timestamp  ((uint32_t)(2016 - 1980) 	<< 25)
					// 	| ((uint32_t)month 			<< 21)
					// 	| ((uint32_t)days 			<< 16)
					// 	| ((uint32_t)hours 			<< 11)
					// 	| ((uint32_t)mins 			<< 5)
					// 	| ((uint32_t)secs 			>> 1);

		// FIXME: update flags
		// rise flags as needed
		//if 		(res != FR_OK)	{f_close(&temp_file); return(2);} //file write failed
		//else if (bw < sz)		{f_close(&temp_file); return(3);} //not all data written


		// CLOSE INDEX FILE
		f_close(&temp_file);

		// WRITE SAMPLE LIST HTML FILE
		// write_samplelist();

		return(FR_OK);

	} 
}

uint8_t index_write_wrapper(void){	

	FRESULT res;
	uint8_t	html_res;

	// WRITE INDEX FILE (buttons are red)
	flags[RewriteIndex]=RED;
	res = write_sampleindex_file();
	if (res!=FR_OK) return(1);

	// WRITE SAMPLE LIST HTML FILE (buttons are orange)
	flags[RewriteIndex]=ORANGE;
	html_res = write_samplelist();
	return (html_res);
}


// WRITE SAMPLE LIST HTML
// previous files are replaced
uint8_t write_samplelist(void)
{

	FIL 		temp_file;
	FRESULT 	res;
	uint8_t 	i, j;
	uint32_t	minutes, seconds;
	uint8_t		bank_is_empty;
	char 		b_color[11]; //Lavender-5\0

	// create file
	res = f_open(&temp_file, SAMPLELIST_FILE , FA_WRITE | FA_CREATE_ALWAYS); 
	if (res != FR_OK) return(1); 
	f_sync(&temp_file);

	// WRITE 'SAMPLES' INFO TO SAMPLE LIST
	f_printf(&temp_file, "<!DOCTYPE html>\n<html>\n<body style=\"padding-left: 100px; background-color:#F8F9FD;\">\n<br><h1>SAMPLE LIST</h1><br>\n");
	f_sync(&temp_file);

	// For each bank
	for (i=0; i<MAX_NUM_BANKS; i++){

		// check if bank is empty
		bank_is_empty=1; j=0;
 		while(j<NUM_SAMPLES_PER_BANK){
 			if (samples[i][j].filename[0]!=0) {bank_is_empty=0; break;}
 			j++;
 		}

 		// if bank isn't empty
 		if(!bank_is_empty){

			// Print bank name to file
			bank_to_color(i, b_color);
			if (i>0) {f_printf(&temp_file, "<br>\n");f_sync(&temp_file);}
	 		f_printf(&temp_file, "<h2>%s</h2>\n<table>\n", b_color);
			f_sync(&temp_file);

			// Print sample name to sample list, for each sample in bank
			for (j=0; j<NUM_SAMPLES_PER_BANK; j++)
			{
				seconds = (samples[i][j].sampleSize/samples[i][j].blockAlign) / samples[i][j].sampleRate;
				minutes = seconds / 60;
				seconds-= minutes * 60;
				if (!minutes && !seconds) seconds =1;
				if(samples[i][j].filename[0]!=0)f_printf(&temp_file, "<tr>\n<td>%d]  %s</td>\n<td>........... [%02d:%02d]</td>\n</tr>\n", j+1, samples[i][j].filename, minutes, seconds);
				f_sync(&temp_file);
			} 
			
			f_printf(&temp_file, "</table><br>\n");
			f_sync(&temp_file);
		}
	}
	f_printf(&temp_file, "</body>\n</html>");
	f_sync(&temp_file);	

	// CLOSE FILE
	f_close(&temp_file);
	return(0);
}


FRESULT backup_sampleindex_file(void)
{
	// ToDo: could just duplicate data and open resulting pointer
	// ... if dup is not available
	// FILE *fp2 = fdopen (dup (fileno (fp)), "r");

	FIL 		indexfile, backupindex;
	FRESULT		res_index, res_bak;
	char 		read_buff[512];
	uint32_t 	bytes_read, bytes_written;
	char		idx_full_path[_MAX_LFN+1];
	char		bak_full_path[_MAX_LFN+1];

	// Open index
	str_cat(idx_full_path, SYS_DIR_SLASH, SAMPLE_INDEX_FILE);
	res_index  = f_open(&indexfile, idx_full_path, FA_READ);
	if(res_index!=FR_OK) {f_close(&indexfile);   return (res_index);}

	// Open Backup file
	str_cat(bak_full_path, SYS_DIR_SLASH, SAMPLE_BAK_FILE);	
	res_bak = f_open(&backupindex, bak_full_path, FA_WRITE | FA_CREATE_ALWAYS);
	if(res_bak!=FR_OK) {f_close(&indexfile);f_close(&backupindex); return (res_bak);}

	while(1)
	{
		// Read index file
		f_read(&indexfile, read_buff, 512, &bytes_read);

		// Write into backup file
		f_write(&backupindex, read_buff, bytes_read, &bytes_written);
		f_sync(&backupindex);

		if (bytes_written!=bytes_read) //error
		{
			f_close(&indexfile);
			f_close(&backupindex);
			return (FR_INT_ERR); // ToDo: there should be a way to report this error more accurately
		}
	
		if (f_eof(&indexfile))
		{
			f_close(&indexfile);
			f_close(&backupindex);
			return (FR_OK); 
		}
	}
}

//Returns 0 if invalid file (file can't be opened/read, or no EOF_TAG at end of file)
//Returns 1 if we read EOF_TAG at the end of the file
uint8_t check_sampleindex_valid(char *indexfilename)
{
	FIL 		temp_file;
	FRESULT 	res;
	char		full_path[_MAX_LFN+1];
	char 		readdata[_MAX_LFN+1];
	uint8_t		l;
	uint32_t	br;

	str_cat(full_path, SYS_DIR_SLASH, SAMPLE_INDEX_FILE);
	res = f_open(&temp_file, full_path, FA_READ);

	if (res != FR_OK) 						{return 0;}							//file can't open

	//Verify it's a complete file, with EOF_TAG at the end
	l = str_len(EOF_TAG) + EOF_PAD;

	res = f_lseek(&temp_file, f_size(&temp_file)-l);
	if (res != FR_OK) 						{f_close(&temp_file); return 0;}	//can't seek to end: file/disk error
	
	res = f_read(&temp_file, readdata, l, &br);
	if (res != FR_OK) 						{f_close(&temp_file); return 0;}	//can't read from file: file/disk error

	if (br!=l) 								{f_close(&temp_file); return 0;}	//didn't read proper number of bytes: disk/read error

	// Check for EOF_TAG within last read buffer
	if (!str_found(readdata, EOF_TAG)) {f_close(&temp_file); return 0;}	//no EOF_TAG found at end of file: index is incomplete
	
	//Index file is ok
	f_close(&temp_file);
	return 1;

}


//Loads a sample index file
//Sets samples[][] for all banks, or just one bank (specified with banks=bank# or banks=MAX_NUM_BANKS --> all banks)
//
//Can request the normal index file, or the backup file (use_backup=USE_INDEX_FILE or USE_BACKUP_FILE)
//Note: if we request backup file, but it's invalid/missing, then we exit with an error
//On the other hand, if we request the index file but it's invalid/missing, then we use the backup file instead
//
uint8_t load_sampleindex_file(uint8_t use_backup, uint8_t banks)
{

	// // Variables to load from file
	// // ... and to initialize if missing 
	// // ... or out of bounds
	// samples[i][j].inst_start
	// samples[i][j].inst_size
	// (int)(100 * samples[i][j].inst_gain

	// // Varialbles to check and update if incorrect
	// samples[i][j].filename

	// // Variables to systematically update
	// samples[i][j].sampleRate
	// samples[i][j].sampleByteSize*8
	// samples[i][j].sampleSize


	FIL 		temp_file, temp_wav_file;
	FRESULT 	res;
	uint8_t		head_load;
	char 		read_buffer[_MAX_LFN+1], folder_path[_MAX_LFN+2], file_name[_MAX_LFN+1], full_path[_MAX_LFN+1];
	uint8_t		cur_bank=0, cur_sample=0, arm_bank=0, load_data=0, loaded_header=0, read_name=0;
	uint32_t	num_buff=UINT32_MAX;
	char 		token[_MAX_LFN+1];
	//uint8_t		fopen_flag	  = 0;
	uint8_t		l;
	// uint8_t		rewrite_index = 0;
	uint8_t 	force_reload  = 2;
	uint8_t		skip_cur_bank = 0;


	//If we requested the normal non-backup file but it's not a valid file,
	//use the backup file instead
	if (use_backup==USE_INDEX_FILE)
	{
		if (!check_sampleindex_valid(SAMPLE_INDEX_FILE))
			use_backup=USE_BACKUP_FILE;
	}

	//If we requested the backup file (or if we requested the normal file, and it was invalid)
	//Then see if the backup file is valid
	//If not, then we exit with an error
	if (use_backup==USE_BACKUP_FILE)
	{
		if (!check_sampleindex_valid(SAMPLE_BAK_FILE))
			return(1); //file not found
	}

	if (use_backup)
		str_cat(full_path, SYS_DIR_SLASH, SAMPLE_BAK_FILE);

	else
		str_cat(full_path, SYS_DIR_SLASH, SAMPLE_INDEX_FILE);


	res = f_open(&temp_file,full_path, FA_READ);

	// raise flag if index can't be opened/created
	if (res != FR_OK) return(1); //file not found
	
	// until we reach the eof
	while (!f_eof(&temp_file))
	{
		// Read next line
		f_gets(read_buffer, _MAX_LFN+1, &temp_file);

		// Check if it's the end of the file
		if (str_cmp(read_buffer, EOF_TAG))	break;

		// Unix only uses Line feed for newline: \n
		// Windows uses carriage return + line feed for newline: \r\n

		// Remove \n from buffer (mac)
		read_buffer[str_len(read_buffer)-1]=0;

		// Remove \r from end of buffer as needed (PC)
		if(read_buffer[str_len(read_buffer)-1] == '\r')	read_buffer[str_len(read_buffer)-1]=0;

		// tokenize at space if we're not trying to read_name 
		// ... which is both [reading name] and [reading play  data] cases
		if((read_name<1) && (load_data==0)) str_tok(read_buffer,' ', token);
		else str_cpy(token, read_buffer);

		// While token isn't empty
		while(token[0]!='\0')
		{
			// Load bank data
			// ToDo: see if computing repeated str_cmp upstream helps saving time
			if ((!loaded_header) && (!skip_cur_bank))
			{
				if 		( str_cmp(token,"--------------------") && !arm_bank) {arm_bank++; str_tok(read_buffer,' ', token);}
				
				// Bank number
				else if	(!str_cmp(token,"--------------------") &&  arm_bank) 
					{
						cur_bank=color_to_bank(token); 		
						
						// request bank to be skipped if it is not to be loaded
						if (banks != MAX_NUM_BANKS)
						{
							if (banks !=  cur_bank) skip_cur_bank = 1;
						}

						str_tok(read_buffer,' ', token);
					}

				else if ( str_cmp(token,"--------------------") &&  arm_bank) {arm_bank=0; str_tok(read_buffer,' ', token);}

				// read folder path
				else if (str_cmp(token, "path:")) 
				{
					str_cpy(folder_path, read_buffer);
					// ToDo: check for missing / at end of path, or extra / at begining
					loaded_header = 1;
					cur_sample = 0;
					token[0]='\0';
					read_name = 1; 
				}
			}

			// Load file name
			else if ((!str_cmp(token,"--------------------")&&(read_name==1)) && (!skip_cur_bank))
			{

				// save file name
				str_cpy(file_name, token);

				// move on to reading data only if file name is valid	
				if (file_name[0]!='-') {read_name++;}

				token[0] = '\0'; 
			}

			// Load .wav header data information and play data
			else if (((!str_cmp(token,"--------------------"))&&(read_name>1)) && (!skip_cur_bank))
			{
				
				// if there is a number in the current line
				num_buff = str_xt_int(read_buffer);
				if (num_buff != UINT32_MAX)
				{
					
					// arm data loading
					// arm data loading if line starts with '-'
					// skips header info and extra lines otherwise
					// if 		((read_buffer[0]=='-')&&(!load_data))	{load_data=1;}
					
					// Process data if line starts with '-'
					// skip lines that don't start with '-'					
					if (read_buffer[0]=='-') {
						if (!load_data) {load_data=1;}
						// load header data from .wav file
						// if the data loading has just been armed
						if  (load_data == SAMPLE_SLOT) 			
						{
							// Update sample bank number
							cur_sample=num_buff-1;

							// ToDo: Add bank number check here
							// check requested bank number against what's already used and update accordingly

							res = FR_INT_ERR; //not FR_OK

							//If filename contains a slash, then try the filename as written
							//--- if it doesn't contain a slash, then it's not a path, so don't assume it's in root
							if (str_pos('/', file_name) != 0xFFFFFFFF)
							{
								str_cpy(full_path, file_name);
								res = f_open(&temp_wav_file, full_path, FA_READ);
							}
							if (res!=FR_OK)
							{
								//add a slash to folder_path if it doesn't have one, and file_name doesn't start with one
								l = str_len(folder_path);
								if (folder_path[l-1] !='/' && file_name[0]!='/'){
									folder_path[l] = '/';
									folder_path[l+1] = '\0';
								}

								//try to open folder_path/file_name
								str_cat(full_path, folder_path, file_name);
								res = f_open(&temp_wav_file, full_path, FA_READ);
							}

							if (res==FR_OK)//file found
							{
								//Sanity check: cur_bank and cur_sample must be in range, or we risk memory corruption
								if (cur_bank<MAX_NUM_BANKS && cur_sample<NUM_SAMPLES_PER_BANK)
								{
									str_cpy(samples[cur_bank][cur_sample].filename, full_path); //use whatever file_name was opened
									samples[cur_bank][cur_sample].file_found = 1;

									// At least a sample was loaded
									force_reload = 0;

									// load sample information from .wav header	

									head_load = load_sample_header(&samples[cur_bank][cur_sample], &temp_wav_file); 
									// close wav file
									f_close(&temp_wav_file);
								}
								else //exit if cur_bank and/or cur_sample are out of range
									{f_close(&temp_wav_file); load_data=0; token[0] = '\0'; read_name = 1; break;}


								// if header information couldn't load, treat file as if it couldn'tbe found
								if (head_load!=FR_OK)
								{
									// write empty filename to samples struct element
									// ... so this sample is skipped at the next index write
									// FixMe:?? Set file_found==1 because the file was found, it just was corrupted
									// Or set it to 0?
									// samples[cur_bank][cur_sample].filename[0]='\0';

									// FixMe(H) if statement redundant with same statement l502?
									if (cur_bank<MAX_NUM_BANKS && cur_sample<NUM_SAMPLES_PER_BANK)
										samples[cur_bank][cur_sample].file_found = 0;
									
									// skip loading sample play information, and read next file
									load_data=0; token[0] = '\0'; read_name = 1; break;						
								}		
								//FixMe: Should we have an 'else' here? Otherwise load_data=1 after not loading a header
								load_data++; token[0] = '\0';
							}

							else if (res!=FR_OK) //file not found
							{
								// Copy the file name into the sample struct element
								// This is used to find the missing file, or other files in its folder
								if (cur_bank<MAX_NUM_BANKS && cur_sample<NUM_SAMPLES_PER_BANK)
								{
									str_cpy(samples[cur_bank][cur_sample].filename, full_path);

									//Mark file as not found
									samples[cur_bank][cur_sample].file_found = 0;
								}

								// skip loading sample play information
								load_data=0; token[0] = '\0'; read_name = 1; break;						

							}
						
						}

						else if (load_data == PLAY_START)
						{
							if (num_buff > samples[cur_bank][cur_sample].sampleSize) {samples[cur_bank][cur_sample].inst_start= samples[cur_bank][cur_sample].sampleSize - 2;} // 2x samples min room after play start
							else {samples[cur_bank][cur_sample].inst_start = num_buff;}
							load_data++; token[0] = '\0';
						}

						else if (load_data == PLAY_SIZE)
						{
							if ( num_buff < (samples[cur_bank][cur_sample].inst_start+2) ) {samples[cur_bank][cur_sample].inst_size = samples[cur_bank][cur_sample].inst_start +2;} // 2x samples min play lenght
							else if ( num_buff > samples[cur_bank][cur_sample].sampleSize){samples[cur_bank][cur_sample].inst_size = samples[cur_bank][cur_sample].sampleSize;}
							else {samples[cur_bank][cur_sample].inst_size=num_buff;}
							load_data++; token[0] = '\0';
						}

						else if (load_data == PLAY_GAIN)
						{
							if(num_buff>100){num_buff = 100;}
							samples[cur_bank][cur_sample].inst_gain=num_buff/100.0f;
							load_data=0; token[0] = '\0'; read_name = 1; break;
						}
					}
					else	{ token[0] = '\0';} 
				}
			}

			// Move to next bank
			else if (str_cmp(token,"--------------------"))
			{
				skip_cur_bank   = 0;
				read_name 		= 0; 
				loaded_header	= 0;
				read_buffer[0]  = '\0';
			}		
			
			// if nothing is recognized
			// or bank is to be skipped
			// empty token so new line is read
			if  (	(!loaded_header &&
					!(  (str_cmp(token,"--------------------") && !arm_bank)	||
						(!str_cmp(token,"--------------------") &&  arm_bank)	||
						(str_cmp(token,"--------------------") &&  arm_bank)	||
						(str_cmp(token, "path:")) 
					 ))
					 ||  skip_cur_bank
			    )
			{
			// should be the same as commented section below since
			// && has precedence over ||
			/*
			if  (	!loaded_header &&
					!(  (str_cmp(token,"--------------------") && !arm_bank)	||
						(!str_cmp(token,"--------------------") &&  arm_bank)	||
						(str_cmp(token,"--------------------") &&  arm_bank)	||
						(str_cmp(token, "path:")) 
					 )
					 ||  skip_cur_bank
			    )
			{
			*/

				token[0]='\0';
			}	
		}
	}

	// close sample index file
	f_close(&temp_file);

	// //  rewrite sample index as needed
	// if(rewrite_index)
	// {
	// 	// write_sampleindex_file();
	// }

	// Assign samples in SD card to (previously empty) sample index
	return(force_reload);	// OK if at least a sample was loaded
}