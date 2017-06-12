#include <string.h>
#include "globals.h"
#include "file_util.h"
#include "ffconf.h"

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


void str_cpy(char *dest, char *src)
{
  while(*src!=0)
    *dest++ = *src++;

  *dest=0;
}

void str_cat(char *dest, char *srca, char *srcb)
{ 
  while(*srca!=0)
    *dest++ = *srca++;
  while(*srcb!=0)
    *dest++ = *srcb++;
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

