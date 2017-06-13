#include "globals.h"
#include "params.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sample_file.h"
#include "bank.h"


uint8_t bank_status[MAX_NUM_BANKS];

extern enum g_Errors g_error;





uint8_t bank_to_color_string(uint8_t bank, char *color)
{
	switch(bank)
		{
		case 0:
			str_cpy(color, "Pink");
			return(4);
			break;

		case 1:
			str_cpy(color, "Red");
			return(3);
			break;

		case 2:
			str_cpy(color, "Orange");
			return(6);
			break;

		case 3:
			str_cpy(color, "Yellow");
			return(6);
			break;

		case 4:
			str_cpy(color, "Green");
			return(5);
			break;

		case 5:
			str_cpy(color, "Cyan");
			return(4);
			break;

		case 6:
			str_cpy(color, "Blue");
			return(4);
			break;

		case 7:
			str_cpy(color, "Violet");
			return(6);
			break;

		case 8:
			str_cpy(color, "Lavendar"); 
			return(8);
			break;

		case 9:
			str_cpy(color, "White");
			return(5);
			break;

		default:
			color[0]=0;
			return(0);
		}

}

uint8_t bank_to_color(uint8_t bank, char *color)
{
	uint8_t digit1, digit2, len;

	if (bank>9 && bank<100)
	{
		//convert a 2-digit bank number to a color and number

		//First digit: 49 ---> 4
		digit1 = bank / 10;

		//Second digit: 49 ---> (49 - 4*10) = 9
		digit2 = bank - (digit1*10); 

		//Second digit as a color string: "White"
		len = bank_to_color(digit2, color);

		//Omit the "0" for the first bank of a color, so "Red-0" is just "Red"
		//Otherwise append a dash and number to the color name
		if (digit1 > 0)
		{
			//Move pointer to the end of the string (overwrite the \0)
			color += len;

			//Append a dash: "White-"
			*color++ = '-';
			len++;

			//Append first digit as a string: "White-5"
			intToStr(digit1, color, 1);
			len += 2;
		}

		return (len);
	}
	else 
		return bank_to_color_string(bank, color);
}


uint8_t next_enabled_bank(uint8_t bank) //if bank==0xFF, we find the first enabled bank
{
	uint8_t orig_bank=bank;

	do{
		bank++;
		if (bank >= MAX_NUM_BANKS)	bank = 0;

	} while(!bank_status[bank] && (bank!=orig_bank));

	return (bank);
}


void check_enabled_banks(void)
{
	uint32_t sample_num, bank;

	for (bank=0;bank<MAX_NUM_BANKS;bank++)
	{
		bank_status[bank] = 0; //does not exist
		for(sample_num=0; sample_num<NUM_SAMPLES_PER_BANK; sample_num++)
		{
			if (samples[bank][sample_num].filename[0] != 0)
			{
				bank_status[bank] = 1;
				break;
			}
		}
	}
}

uint8_t is_bank_enabled(uint8_t bank)
{
	return bank_status[bank] ? 1:0;
}

void enable_bank(uint8_t bank)
{
	bank_status[bank] = 1;
}
void disable_bank(uint8_t bank)
{
	bank_status[bank] = 0;
}
