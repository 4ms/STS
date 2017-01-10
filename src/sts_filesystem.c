#include "sts_filesystem.h"
#include "file_util.h"


uint8_t bank_to_color(uint8_t bank, char *color)
{
	switch(bank)
	{
	case 0:
		str_cpy(color, "White");
		return(5);
		break;

	case 1:
		str_cpy(color, "Red");
		return(3);
		break;

	case 2:
		str_cpy(color, "Green");
		return(5);
		break;

	case 3:
		str_cpy(color, "Blue");
		return(4);
		break;

	case 4:
		str_cpy(color, "Yellow");
		return(5);
		break;

	case 5:
		str_cpy(color, "Cyan");
		return(4);
		break;

	case 6:
		str_cpy(color, "Orange");
		return(6);
		break;

	case 7:
		str_cpy(color, "Violet");
		return(6);
		break;

	default:
		color[0]=0;
		return(0);
	}

}
