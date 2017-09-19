
#include "globals.h"
#include "ff.h"
#include "file_util.h"
#include "str_util.h"
#include "sts_filesystem.h"
#include "sts_fs_renaming_queue.h"

#define DO_RENAMING 0
extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

//
// clear_renaming_queue()
//
// Create and truncate the queue file
//
FRESULT clear_renaming_queue(void)
{
	FIL queue_file;
	FRESULT res;
	char	filepath[_MAX_LFN];

	if (!DO_RENAMING) return(FR_OK);

	res = check_sys_dir();
	if (res==FR_OK)
	{
		str_cat(filepath, SYS_DIR_SLASH, RENAME_TMP_FILE);
		res = f_open(&queue_file, filepath, FA_WRITE | FA_CREATE_ALWAYS); 
		if (res==FR_OK) 
			res=f_close(&queue_file);
	}

	return(res);
}

//
// append_rename_queue()
//
// Appends to a renaming queue file
//
FRESULT append_rename_queue(uint8_t bank, char *orig_name, char *new_name)
{
	FRESULT res;
	FIL queue_file;
	char	filepath[_MAX_LFN];

	if (!DO_RENAMING) return(FR_OK);

	res = check_sys_dir();
	if (res==FR_OK)
	{
		str_cat(filepath, SYS_DIR_SLASH, RENAME_TMP_FILE);
		res = f_open(&queue_file, filepath, FA_OPEN_APPEND | FA_WRITE); 
		if (res!=FR_OK) return(res);

		f_sync(&queue_file);

	 	f_printf(&queue_file, "\nBank: ");
		f_sync(&queue_file);

	 	f_printf(&queue_file, "%d\n", bank);
		f_sync(&queue_file);

	 	f_printf(&queue_file, "Original name: %s\n", orig_name);
		f_sync(&queue_file);

	 	f_printf(&queue_file, "New name: %s\n", new_name);
		f_sync(&queue_file);

		res = f_close(&queue_file);
	}
	return(res);
}

//
//Execute the renaming procedure
//rename all the queued items in the renaming queue file
//And delete the file when done
//
FRESULT process_renaming_queue(void)
{
	FRESULT 	res;
	FIL 		queue_file;
	FIL 		log_file;
	uint8_t 	skip_log=0;
	uint8_t		logged_something=0;
	char 		read_buffer[255];//max line length

	char 		orig_name[_MAX_LFN+1];
	char 		new_name[_MAX_LFN+1];
	uint32_t	bank;

	if (!DO_RENAMING) return(FR_OK);

	res = check_sys_dir();
	if (res==FR_OK)
	{

		//Open the queue file in read-only mode
		str_cat(orig_name, SYS_DIR_SLASH, RENAME_TMP_FILE);
		res = f_open(&queue_file, orig_name, FA_READ); 
		if (res!=FR_OK) return(res);

		//Open the log file in append mode (create it if it doesn't exist)
		str_cat(orig_name, SYS_DIR_SLASH, RENAME_LOG_FILE);
		res = f_open(&log_file, orig_name, FA_OPEN_APPEND | FA_WRITE); 
		if (res!=FR_OK) skip_log = 1; //if we can't create the queue log file, just skip logging

		//Read from the renaming queue file
		//Get an orig_name, new_name, and bank
		
		bank = MAX_NUM_BANKS+1;
		orig_name[0]=0;
		new_name[0]=0;

		// until we reach the eof
		while (!f_eof(&queue_file))
		{
			// Read next line
			f_gets(read_buffer, 255, &queue_file);
			if (read_buffer[0]==0) break; //error or end of file

			// Remove /n from buffer
			if (read_buffer[str_len(read_buffer)-1]=='\n')
				read_buffer[str_len(read_buffer)-1]=0;

			//Skip blank lines
			if (read_buffer[0]==0) continue;

			//Look for a new entry
			if (str_startswith_nocase(read_buffer, "Bank: "))
			{
				//Load the bank number
				bank = str_xt_int(&(read_buffer[6]));

				//clear the orig and new names
				//because Bank: always starts a new entry
				orig_name[0] = 0;
				new_name[0] = 0;
			}
			else
			//Look for an original name line (only if we've already found a valid bank number)
			if (bank<MAX_NUM_BANKS && str_startswith_nocase(read_buffer, "Original name: "))
			{
				str_cpy(orig_name, &(read_buffer[15]));

				//clear the new name
				//because New name: must always follow Original name:
				new_name[0] = 0;
			}
			else
			//Look for a new name line (only if we've already found a valid bank number AND an original name)
			if (bank<MAX_NUM_BANKS && orig_name[0] && str_startswith_nocase(read_buffer, "New name: "))
			{
				str_cpy(new_name, &(read_buffer[10]));

				if (new_name[0])
				{
					res = f_rename(orig_name, new_name);
					if (res!=FR_OK) continue; //something went wrong in renaming the folder, so we abort and continue processing the file

					//Reload the bank using the new folder name
					//ToDo: this is inefficient, we could just update every samples[bank][].filename
					load_bank_from_disk((samples[bank]), new_name);

					if (!skip_log)
					{
						logged_something = 1;

						//Log the transaction:
						f_printf(&log_file, "\nRenamed folder for bank %d\n", bank);
						f_sync(&log_file);

						f_printf(&log_file, "Old folder name: %s\n", orig_name);
						f_sync(&log_file);

						f_printf(&log_file, "New folder name: %s\n\n", new_name);
						f_sync(&log_file);
					}

					bank=MAX_NUM_BANKS+1;
					new_name[0]=0;
					orig_name[0]=0;
				}
			}

		}


		res = f_close(&log_file);

		//Delete the log file if we didn't log anything
		if (res==FR_OK && !logged_something)
		{
			str_cat(orig_name, SYS_DIR_SLASH, RENAME_LOG_FILE);
			res = f_unlink(orig_name);
		}

		res = f_close(&queue_file);

		//Delete the tmp file
		if (res==FR_OK)
		{
			str_cat(orig_name, SYS_DIR_SLASH, RENAME_TMP_FILE);
			res = f_unlink(orig_name);
		}
	}
	return(res);
}
