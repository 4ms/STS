#include "globals.h"
#include "params.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sample_file.h"
#include "bank.h"


uint8_t bank_status[MAX_NUM_BANKS];

extern enum g_Errors g_error;


extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

//
//Returns the sample number inside the given bank, whose filename matches the given filename
//If not found, returns 0xFF
//
uint8_t find_filename_in_bank(uint8_t bank, char *filename)
{
	uint8_t sample;

	if (bank>=MAX_NUM_BANKS) return 0xFF; //invalid bank
	if (!is_bank_enabled(bank)) return 0xFF; //bank not enabled

	for (sample=0; sample<NUM_SAMPLES_PER_BANK; sample++)
	{
		if (str_cmp(samples[bank][sample].filename, filename)) return sample;
	}

	return(0xFF); //not found
}


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
			str_cpy(color, "Lavender"); 
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

		//Second digit as a color string i.e. "White" etc
		len = bank_to_color_string(digit2, color);

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

//FixMe: Update to blink-color system
uint8_t color_to_bank(char *color)
{
	uint8_t i;
	char* 	b_color;
	char  	t_b_color[_MAX_LFN+1];

	b_color = t_b_color;

	// for every bank number
	for (i=0; i<MAX_NUM_BANKS; i++)
	{
		// convert bank number to color
		bank_to_color(i, b_color);

		// compare current bank color with input bank color
		if (str_cmp(color, b_color))
		{
			// ... and return bank number if there's a match
			return(i);
		}
	}		
	return(200);

	// if 		(str_cmp(color, "White")) 		return(0);
	// else if (str_cmp(color, "Red")) 		return(1);
	// else if (str_cmp(color, "Green")) 		return(2);
	// else if (str_cmp(color, "Blue")) 		return(3);
	// else if (str_cmp(color, "Yellow"))		return(4);
	// else if (str_cmp(color, "Cyan")) 		return(5);
	// else if (str_cmp(color, "Orange")) 		return(6);
	// else if (str_cmp(color, "Violet")) 		return(7);
	// else if (str_cmp(color, "White-SAVE")) 	return(8);
	// else if (str_cmp(color, "Red-SAVE"))	return(9);
	// else if (str_cmp(color, "Green-SAVE"))	return(10);
	// else if (str_cmp(color, "Blue-SAVE")) 	return(11);
	// else if (str_cmp(color, "Yellow-SAVE")) return(12);
	// else if (str_cmp(color, "Cyan-SAVE")) 	return(13);
	// else if (str_cmp(color, "Orange-SAVE")) return(14);
	// else if (str_cmp(color, "Violet-SAVE")) return(15);
	// return(200);
}

uint8_t next_bank(uint8_t bank)
{
	bank++;
	if (bank >= MAX_NUM_BANKS)	return(0);
	return (bank);
}


uint8_t next_enabled_bank(uint8_t bank) //if bank==0xFF, we find the first enabled bank
{
	uint8_t orig_bank=bank;

	do{
		bank++;
		if (bank >= MAX_NUM_BANKS)	bank = 0;
		if (bank==orig_bank) return(0); //no banks are enabled -->> bail out and return the first bank
	} while(!bank_status[bank]);

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
	if (bank>=MAX_NUM_BANKS) return 0;

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

//
//Given a number, returns the units digit
//This digit refers to the bank color, regardless of blink #
//
uint8_t get_bank_color_digit(uint8_t bank)
{
//	uint8_t digit1;

	if (bank<10) return bank;
	
	while (bank>=10) bank-=10;
	return (bank);
}

//
//Given a number, returns the tens digit
//This digit refers to the bank blink #, regardless of color
//
uint8_t get_bank_blink_digit(uint8_t bank)
{
	return (bank / 10);
}
