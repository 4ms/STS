#include <string.h>
#include "globals.h"
#include "ff.h"
#include "file_util.h"

FRESULT scan_files (
    char* path        /* Start node to be scanned (***also used as work area***) */
)
{
    FRESULT res;
    DIR dir;
    UINT i=0;
    static FILINFO fno;


    res = f_opendir(&dir, path);							/* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);					/* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;	/* Break on error or end of dir */




            if (fno.fattrib & AM_DIR) {						/* It is a directory */
              i = str_len(path);
              path[i]='/';
              str_cpy(&(path[i+1]), fno.fname);

              //  sprintf(&path[i], "/%s", fno.fname);

                res = scan_files(path);						/* Enter the directory */
                if (res != FR_OK) break;
                path[i] = 0;
            } else {										/* It is a file. */
              //  printf("%s/%s\n", path, fno.fname);
            }
        }
        f_closedir(&dir);
    }
    return res;
}

uint32_t str_len(char* str)
{
	uint32_t i=0;

	while(*str!=0)
	{
		str++;
		i++;
		if (i==_MAX_LFN) return(0xFFFFFFFF);
	}
	return(i);
}

void str_cpy(char *dest, char *src)
{
	while(*src!=0)
		*dest++ = *src++;

	*dest=0;
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
