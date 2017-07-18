#include "globals.h"
#include "params.h"
#include "dig_pins.h"
#include "sts_filesystem.h"
#include "file_util.h"
#include "sample_file.h"
#include "bank.h"


extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 	flags[NUM_FLAGS];
extern enum g_Errors g_error;

extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

uint8_t bank_status[MAX_NUM_BANKS];


void copy_bank(Sample *dst, Sample *src)
{
	uint8_t *p_dst;
	uint8_t *p_src;
	uint32_t i;

	p_dst = (uint8_t *)(dst);
	p_src = (uint8_t *)(src);
	for (i=0;i<(sizeof(Sample)*NUM_SAMPLES_PER_BANK);i++)
		*p_dst++ = *p_src++;


	// for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
	// {
	// 		str_cpy(dst[i].filename,  src[i].filename);
	// 		dst[i].blockAlign 		= src[i].blockAlign;
	// 		dst[i].numChannels 		= src[i].numChannels;
	// 		dst[i].sampleByteSize 	= src[i].sampleByteSize;
	// 		dst[i].sampleRate 		= src[i].sampleRate;
	// 		dst[i].sampleSize 		= src[i].sampleSize;
	// 		dst[i].startOfData 		= src[i].startOfData;
	// 		dst[i].PCM 				= src[i].PCM;

	// 		dst[i].inst_size 		= src[i].inst_size  ;//& 0xFFFFFFF8;
	// 		dst[i].inst_start 		= src[i].inst_start  ;//& 0xFFFFFFF8;
	// 		dst[i].inst_end 		= src[i].inst_end  ;//& 0xFFFFFFF8;

	// 		dst[i].file_found 		= src[i].file_found  ;

	// }

}


//
// Copy bank+40 to bank+50, bank+40 to bank+40, etc...
// The goal is to open up bank#=bank so we can place a new set of files in there
// FixMe: This must be changed manually if MAX_NUM_BANKS changes
uint8_t bump_down_banks(uint8_t bank)
{
	uint8_t t_bank;

	if (bank<10){
		copy_bank(samples[bank+50], samples[bank+40]);
		bank_status[bank+50] = bank_status[bank+40];
	}

	if (bank<20){
		copy_bank(samples[bank+40], samples[bank+30]);
		bank_status[bank+40] = bank_status[bank+30];
	}

	if (bank<30){
		copy_bank(samples[bank+30], samples[bank+20]);
		bank_status[bank+30] = bank_status[bank+20];
	}

	if (bank<40){
		copy_bank(samples[bank+20], samples[bank+10]);
		bank_status[bank+20] = bank_status[bank+10];
	}

	if (bank<50){
		copy_bank(samples[bank+10], samples[bank]);
		bank_status[bank+10] = bank_status[bank];
	}

	else{ //bank>=50 would get bumped out, so try to place it elsewhere
		t_bank = prev_disabled_bank(MAX_NUM_BANKS);
		copy_bank(samples[t_bank], samples[bank]);
		bank_status[t_bank] = bank_status[bank];
	}

}

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

//Searches all banks for a filename
//Returns bank# if filename exists in ANY sample in that bank
//Returns 0xFF if filename is not found anywhere
//bank is the first bank to look in, setting this to a likely candidate will save CPU time
//
uint8_t find_filename_in_all_banks(uint8_t bank, char *filename)
{
	uint8_t orig_bank;
	
	orig_bank=bank;
	do{
		if (find_filename_in_bank(bank, filename) != 0xFF)
			return(bank);

		bank=next_enabled_bank(bank);	
	} while(bank!=orig_bank);

	return(0xFF);//not found
}


uint8_t bank_to_color_string(uint8_t bank, char *color)
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
			str_cpy(color, "Magenta");
			return(7);
			break;

		case 8:
			str_cpy(color, "Lavender"); 
			return(8);
			break;

		case 9:
			str_cpy(color, "Pearl");
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
			len++;
		}

		return (len);
	}
	else 
		return bank_to_color_string(bank, color);
}

//FixMe: have this return an error flag, or perhaps MAX_NUM_BANKS, because we could conceivably have >200 banks someday
uint8_t color_to_bank(char *color)
{
	uint8_t i;
	char  	b_color[11];

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
}

uint8_t next_bank(uint8_t bank)
{
	bank++;
	if (bank >= MAX_NUM_BANKS)	return(0);
	return (bank);
}

//if bank==0xFF, we find the first enabled bank
uint8_t next_enabled_bank(uint8_t bank) 
{
	uint8_t orig_bank=bank;

	do{
		bank++;
		if (bank >= MAX_NUM_BANKS)	bank = 0;
		if (bank==orig_bank) return(0); //no banks are enabled -->> bail out and return the first bank
	} while(!bank_status[bank]);

	return (bank);
}

// same as next_enabled_bank but wraps around to 0xFF instead
uint8_t next_enabled_bank_0xFF(uint8_t bank) 
{
	do{
		bank++;
		if (bank >= MAX_NUM_BANKS)	return(0xFF);
	} while(!bank_status[bank]);

	return (bank);
}

uint8_t prev_enabled_bank_0xFF(uint8_t bank)
{
	if (bank == 0xFF) bank=MAX_NUM_BANKS;
	do{
		if (bank == 0)	return(0xFF);
		bank -=1;
	} while(!bank_status[bank]);

	return (bank);
}

//if bank==0xFF, we find the first disabled bank
uint8_t next_disabled_bank(uint8_t bank) 
{
	uint8_t orig_bank=bank;

	do{
		bank++;
		if (bank >= MAX_NUM_BANKS)	bank = 0;
		if (bank==orig_bank) return(0); //no banks are disabled -->> bail out and return the first bank
	} while(bank_status[bank]);

	return (bank);
}

uint8_t prev_disabled_bank(uint8_t bank) 
{
	uint8_t orig_bank=bank;

	if (bank >= MAX_NUM_BANKS) bank=MAX_NUM_BANKS;

	do{
		if (bank == 0)	bank = MAX_NUM_BANKS;
		bank--;
		if (bank==orig_bank) return(bank); //no banks are disabled -->> bail out and return the same bank given
	} while(!bank_status[bank]);

	return (bank);
}

// Go through all banks, and enable/disable each one:
// If the bank has at least one slot with a filename and file_found==1, then enable the bank 
// Otherwise disable it
void check_enabled_banks(void)
{
	uint32_t sample_num, bank;

	for (bank=0;bank<MAX_NUM_BANKS;bank++)
	{
		bank_status[bank] = 0; //does not exist
		for(sample_num=0; sample_num<NUM_SAMPLES_PER_BANK; sample_num++)
		{
			if (samples[bank][sample_num].filename[0] != 0 && samples[bank][sample_num].file_found==1)
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


void init_banks(void)
{
	uint32_t bank;
	uint8_t force_reload;


	//Force reloading of banks from disk with button press on boot
	if (REV1BUT && REV2BUT) 
		force_reload = 1;
	else
		force_reload = 0;


	load_all_banks(force_reload);

	bank = next_enabled_bank(MAX_NUM_BANKS-1); 				//Find the first enabled bank
	i_param[0][BANK] = bank; 								//Bank 1 starts on the first enabled bank
	i_param[1][BANK] = bank; 								//Bank 2 starts on the first enabled bank
	
	i_param[2][BANK] = next_disabled_bank(bank);			//REC bank starts on first disabled bank

	flags[PlaySample1Changed] = 1;
	flags[PlaySample2Changed] = 1;

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
