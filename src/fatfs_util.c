
/*
 * fatfs_util.c - add-ons for FATFS
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#include "globals.h"
#include "fatfs_util.h"

extern FATFS FatFs;
extern enum g_Errors g_error;


FRESULT reload_sdcard(void)
{
	FRESULT res;

	res = f_mount(&FatFs, "", 1);
	if (res != FR_OK)
	{
		//can't mount
		g_error |= SDCARD_CANT_MOUNT;
		res = f_mount(&FatFs, "", 0);
		return(FR_DISK_ERR);
	}
	return (FR_OK);
}

//
// Create a fast-lookup table (linkmap)
//
#define SZ_TBL 256
CCMDATA DWORD chan_clmt[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK][SZ_TBL];

FRESULT create_linkmap(FIL *fil, uint8_t chan, uint8_t samplenum)
{
	FRESULT res;

	fil->cltbl = chan_clmt[chan][samplenum];
	chan_clmt[chan][samplenum][0] = SZ_TBL;

	res = f_lseek(fil, CREATE_LINKMAP);

	return (res);
}
