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

        if (res != FR_OK || fno.fname[0] == 0)  return(1); //no more files found

        if (fno.fname[0] == '.') continue;                  //ignore files starting with a .

        i = str_len(fno.fname);
        if (i==0xFFFFFFFF)                      return (2); //file name invalid

        if (fno.fname[i-4] == ext[0] &&
              (  (fno.fname[i-3] == ext[1] && fno.fname[i-2] == ext[2] && fno.fname[i-1] == ext[3])
              || (fno.fname[i-3] == EXT[1] && fno.fname[i-2] == EXT[2] && fno.fname[i-1] == EXT[3])  )
            )
        {
            str_cpy(fname, fno.fname);
            return(FR_OK);
        }
    }

    return (3); ///error
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

//Returns the tail of a string, following the splitting char
//Truncates string at the splitting char, and copies that into path
char *str_rstr(char *string, char find, char *path)
{

  char *cp;
  char *rp;
  rp = 0;
  str_cpy(path, string);

  for (cp = string; *cp!=0; cp++)
  {
    if (*cp == find) rp = cp+1;
  }
  if (rp==0) return 0; //or maybe string?
  else {
    path[str_len(path)-str_len(rp)]=0;
    return (rp);
  }
}

//Returns the head of a string up until the first 'find' char
//Copies the tail following the 'find' char into string
// str_tok(Path/To/A/File,'/') -->> returns Path, and string becomes "To/A/File"
char *str_tok(char *string, char find)
{

  char    *cp;
  uint8_t flag=0;
  char    *tokn;
  char    t_tokn[_MAX_LFN+1];
  
  if (string[0]=='\0'){return(0);}
  else{
    tokn = t_tokn;
    str_cpy(tokn, string);

    for (cp = string; *cp!=0; cp++)
    {
      if (*cp == find) {
        flag=1;
        break;
      }
    }

    if(!flag){string[0]=0; return (tokn);}
    else {
      tokn[str_len(string)-str_len(cp)]=0;
      str_cpy(string, cp+1);
      return (tokn);  
    }
  }
}


//Returns a (positive) integer from a string
//Returns 0xFFFFFFFF if no number found
uint32_t str_xt_int(char *string)
{

  char      *cp;
  uint32_t  n = 4294967295; //max value of uin32_t
  uint8_t   found_number = 0;

  for (cp = string; *cp!=0; cp++)
  {
    if ((*cp >= '0') && (*cp <= '9'))
    {
      if (!found_number)
      {
        n=0; found_number=1;
      }
      n = n*10 + *cp-'0';
    } 
    else 
    {
      if(found_number)break;
    }
  }
  return(n);
}

//Copy string dest <= src
void str_cpy(char *dest, char *src)
{
  while(*src!=0)
    *dest++ = *src++;

  *dest=0;
}

//Concatenate string dest <= srca + srcb
void str_cat(char *dest, char *srca, char *srcb)
{ 
  while(*srca!=0)
    *dest++ = *srca++;
  while(*srcb!=0)
    *dest++ = *srcb++;
  *dest=0;
}

//Comare strings a and b
//Return 1 if the same, 0 if not
uint8_t str_cmp(char *a, char *b)
{
	while(*a!=0)
		if (*b++ != *a++) return(0);

	if (*a!=*b) return(0); //check b also has 0 in the same spot as a

	return(1);
}

//Returns first position of needle in haystack
//Returns 0xFFFFFFFF if not found
//
uint32_t str_pos(char needle, char *haystack)
{
  uint32_t i=0;
  while (*haystack++)
  {
    if (*haystack == needle) return(i);
    i++;
  }
  return (0xFFFFFFFF); //not found
}

//Convert integer x to a string, padding with 0's to make it d digits
//Returns the length
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

