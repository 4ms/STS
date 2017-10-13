#include "globals.h"
#include "audio_util.h"


uint32_t align_addr(uint32_t addr, uint32_t blockAlign)
{
	volatile uint32_t t;

	if (blockAlign == 4)			addr &= 0xFFFFFFFC;
	else if (blockAlign == 2)		addr &= 0xFFFFFFF8; //was E? but it clicks if we align to 2 not 4, even if our file claims blockAlign = 2
	else if (blockAlign == 8)		addr &= 0xFFFFFFF8;
	else if (blockAlign == 6)	{	t = addr / 6UL;
									addr = t * 6UL;
								}
	else if (blockAlign == 3)	{
									t = addr / 3UL;
									addr = t * 3UL;
								}
	else if (blockAlign == 1) 		return addr;
	else 							return 0; //unknown block Align

	return addr;
}

