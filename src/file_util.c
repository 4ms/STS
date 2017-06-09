#include <string.h>
#include "globals.h"
#include "file_util.h"
#include "ff.h"

//Returns the next directory in the parent_dir
//
//To reset to the first directory, pass dir->obj.fs=0
//Following that, pass the same dir argument each time it's called, and
//the string path of the next directory in the parent_dir will be returned.
//If no more directories exist, it will return 0
//
FRESULT get_next_dir(DIR *dir, char *parent_path, char *next_dir_path)
{
    FRESULT res=FR_OK;
    FILINFO fno;
    uint8_t i;

    if (dir->obj.fs==0){
        // Open the directory
        res = f_opendir(dir, parent_path);
    }
    if (res == FR_OK) {
        for (;;) {
            // Read a directory item
            res = f_readdir(dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;

            //It's a directory that doesn't start with a dot
            if ((fno.fattrib & AM_DIR) && (fno.fname[0] != '.')) {

                str_cpy(next_dir_path, parent_path);

                //Add a slash to the end of parent_path if it doesn't have one
                i = str_len(next_dir_path);
                // if (next_dir_path[i-1] != '/')
                //     next_dir_path[i++] = '/';

                //Append the directory name to the end of parent_path
                str_cpy(&(next_dir_path[i]), fno.fname);
                return (FR_OK);
            }
        }
        return FR_NO_FILE;
    }
    return FR_NO_PATH;
}

//FRESULT scan_files (
//    char* path        /* Start node to be scanned (***also used as work area***) */
//)
//{
//    FRESULT res;
//    DIR dir;
//    UINT i=0;
//    static FILINFO fno;
//
//
//    res = f_opendir(&dir, path);							/* Open the directory */
//    if (res == FR_OK) {
//        for (;;) {
//            res = f_readdir(&dir, &fno);					/* Read a directory item */
//            if (res != FR_OK || fno.fname[0] == 0) break;	/* Break on error or end of dir */
//
//
//
//
//            if (fno.fattrib & AM_DIR) {						/* It is a directory */
//              i = str_len(path);
//              path[i]='/';
//              str_cpy(&(path[i+1]), fno.fname);
//
//              //  sprintf(&path[i], "/%s", fno.fname);
//
//                res = scan_files(path);						/* Enter the directory */
//                if (res != FR_OK) break;
//                path[i] = 0;
//            } else {										/* It is a file. */
//              //  printf("%s/%s\n", path, fno.fname);
//            }
//        }
//        f_closedir(&dir);
//    }
//    return res;
//}

uint8_t str_startswith(const char *string, const char *prefix)
{
   // if (str_len(string) < str_len(prefix)) return 0;

    while (*prefix)
    {
        if (*prefix++ != *string++)
            return 0;
    }
    return 1;
}

uint32_t str_len(char* str)
{
	uint32_t i=0;

	while(*str!=0)
	{
		str++;
		i++;
		if (i==255) return(0xFFFFFFFF);
	}
	return(i);
}

void str_cpy(char *dest, char *src)
{
	while(*src!=0)
		*dest++ = *src++;

	*dest=0;
}

uint8_t str_cmp(char *a, char *b)
{
	while(*a!=0)
		if (*b++ != *a++) return(0);

	if (*a!=*b) return(0); //check b also has 0 in the same spot as a

	return(1);
}

uint32_t intToStr(uint32_t x, char *str, uint32_t d)
{
	uint32_t i = 0;
	uint32_t len;
	char temp;

    while (x)
    {
        str[i++] = (x%10) + '0';
        x = x/10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    len=i;

    x = i-1;
    i = 0;
    while (i<x)
    {
        temp = str[i];
        str[i] = str[x];
        str[x] = temp;
        i++; x--;
    }

    str[len] = '\0';
    return len;
}

