/*
 * rgb_leds.c
 */
#include "globals.h"
#include "dig_pins.h"
#include "params.h"
#include "rgb_leds.h"
#include "LED_palette.h"
#include "pca9685_driver.h"
#include "sampler.h"
#include "wav_recording.h"

uint16_t ButLED_color[NUM_RGBBUTTONS][3];
uint16_t cached_ButLED_color[NUM_RGBBUTTONS][3];
uint8_t ButLED_state[NUM_RGBBUTTONS];

extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern enum PlayStates play_state[NUM_PLAY_CHAN];
extern enum RecStates	rec_state;

extern uint8_t flags[NUM_FLAGS];

extern uint8_t	global_mode[NUM_GLOBAL_MODES];

extern volatile uint32_t sys_tmr;


/*
 * init_buttonLEDs()
 *
 * Initializes the ButLED_color array
 * The elements are organized into  RGBLED components, and then red, green, and blue elements:
 * LED_Element_Brightness <==> ButLED_color [RGB_Button_Number] [0=red, 1=green, 2=blue]
 */
void init_buttonLEDs(void)
{
	uint8_t i;
	for (i=0;i<NUM_RGBBUTTONS;i++)
	{
		ButLED_color[i][0]=0;
		ButLED_color[i][1]=0;
		ButLED_color[i][2]=0;
		ButLED_state[i] = 0;

		cached_ButLED_color[i][0]=0xFF;
		cached_ButLED_color[i][1]=0xFF;
		cached_ButLED_color[i][2]=0xFF;
	}

}

void set_ButtonLED_byRGB(uint8_t ButtonLED_number, uint16_t red,  uint16_t green,  uint16_t blue)
{
	ButLED_color[ButtonLED_number][0]=red;
	ButLED_color[ButtonLED_number][1]=green;
	ButLED_color[ButtonLED_number][2]=blue;

}

void set_ButtonLED_byPalette(uint8_t ButtonLED_number, uint16_t paletteIndex)
{
	ButLED_color[ButtonLED_number][0] = LED_PALETTE[paletteIndex][0];
	ButLED_color[ButtonLED_number][1] = LED_PALETTE[paletteIndex][1];
	ButLED_color[ButtonLED_number][2] = LED_PALETTE[paletteIndex][2];
}


void set_ButtonLED_byPaletteFade(uint8_t ButtonLED_number, uint16_t paletteIndexA, uint16_t paletteIndexB, float fade)
{
	ButLED_color[ButtonLED_number][0] = ((float)LED_PALETTE[paletteIndexA][0] * (1.0f-fade)) + ((float)LED_PALETTE[paletteIndexB][0] * fade);
	ButLED_color[ButtonLED_number][1] = ((float)LED_PALETTE[paletteIndexA][1] * (1.0f-fade)) + ((float)LED_PALETTE[paletteIndexB][1] * fade);
	ButLED_color[ButtonLED_number][2] = ((float)LED_PALETTE[paletteIndexA][2] * (1.0f-fade)) + ((float)LED_PALETTE[paletteIndexB][2] * fade);
}


/*
 * display_one_ButtonLED(buttonLED_number)
 *
 * Tells the LED Driver chip to set the RGB color of one LED given by the number ButtonLED_number
 */
void display_one_ButtonLED(uint8_t ButtonLED_number)
{
	if ((cached_ButLED_color[ButtonLED_number][0] != ButLED_color[ButtonLED_number][0]) || (cached_ButLED_color[ButtonLED_number][1] != ButLED_color[ButtonLED_number][1]) || (cached_ButLED_color[ButtonLED_number][2] != ButLED_color[ButtonLED_number][2]))
	{
		LEDDriver_setRGBLED(ButtonLED_number, ( ButLED_color[ButtonLED_number][0] <<20) | ( ButLED_color[ButtonLED_number][1] <<10) | ButLED_color[ButtonLED_number][2] );

		cached_ButLED_color[ButtonLED_number][0]=ButLED_color[ButtonLED_number][0];
		cached_ButLED_color[ButtonLED_number][1]=ButLED_color[ButtonLED_number][1];
		cached_ButLED_color[ButtonLED_number][2]=ButLED_color[ButtonLED_number][2];
	}

}

//#define BIG_PLAY_BUTTONS

/*
 * display_all_ButtonLEDs()
 *
 * Tells the LED Driver chip to set the RGB color of all LEDs that have changed value
 */
void display_all_ButtonLEDs(void)
{
	uint8_t i;

	for (i=0;i<NUM_RGBBUTTONS;i++)
	{
		//Update each LED that has a different color
		if ((cached_ButLED_color[i][0] != ButLED_color[i][0]) || (cached_ButLED_color[i][1] != ButLED_color[i][1]) || (cached_ButLED_color[i][2] != ButLED_color[i][2]))
		{
#ifdef BIG_PLAY_BUTTONS
			if (i ==  Play1ButtonLED || i == Play2ButtonLED)
				LEDDriver_setRGBLED_12bit(i, ( ButLED_color[i][0] <<20) | ( ButLED_color[i][2] <<10) | ButLED_color[i][1] );
			else
				LEDDriver_setRGBLED(i, ( ButLED_color[i][0] <<20) | ( ButLED_color[i][1] <<10) | ButLED_color[i][2] );

#else
			LEDDriver_setRGBLED(i, ( ButLED_color[i][0] <<20) | ( ButLED_color[i][1] <<10) | ButLED_color[i][2] );
#endif
			cached_ButLED_color[i][0]=ButLED_color[i][0];
			cached_ButLED_color[i][1]=ButLED_color[i][1];
			cached_ButLED_color[i][2]=ButLED_color[i][2];
		}
	}

}


/*
 * test_all_buttonLEDs()
 *
 * Test all the buttons
 *
 */
void test_all_buttonLEDs(void)
{
	uint8_t i, j;
	float t=0.0f;
	uint32_t r,g,b;

	for (j=0;j<8;j++)
	{
		LEDDriver_setRGBLED(j,0 );
	}

	for(i=0;i<80;i++)
	{
		for (t=0.0;t<=1.0;t+=0.005)
		{
			for (j=0;j<8;j++)
			{
				set_ButtonLED_byPaletteFade(j, i, i+1, t);
			}

			display_all_ButtonLEDs();
		}
	}
}



/*
 * update_ButtonLEDs()
 *
 * Calculates the color of each button LED and tells the LED Driver chip to update the display
 *
 */
void update_ButtonLEDs(void)
{
	uint8_t ButLEDnum;
	uint8_t chan;
	//uint32_t tm=sys_tmr & 0x3FFF; //14-bit counter
	uint32_t tm_7 = sys_tmr & 0x7F; //7-bit counter
	uint32_t tm_12 = sys_tmr & 0xFFF; //12-bit counter
	uint32_t tm_13 = sys_tmr & 0x1FFF; //13-bit counter
	uint32_t tm_14 = sys_tmr & 0x3FFF; //14-bit counter
	uint32_t tm_15 = sys_tmr & 0x7FFF; //15-bit counter
	uint32_t tm_16 = sys_tmr & 0xFFFF; //16-bit counter
	float tri_16;
	float tri_15;
	float tri_14;
	float tri_13;


//	if (tm_16>0x8000)
//		tri_16 = ((float)(tm_16 - 0x8000)) / 32768.0f;
//	else
//		tri_16 = ((float)(0x8000 - tm_16)) / 32768.0f;
//
//		if (tm_15>0x4000)
//			tri_15 = ((float)(tm_15 - 0x4000)) / 16384.0f;
//		else
//			tri_15 = ((float)(0x4000 - tm_15)) / 16384.0f;

		if (tm_14>0x2000)
			tri_14 = ((float)(tm_14 - 0x2000)) / 8192.0f;
		else
			tri_14 = ((float)(0x2000 - tm_14)) / 8192.0f;

		if (tm_13>0x1000)
			tri_13 = ((float)(tm_13 - 0x1000)) / 4096.0f;
		else
			tri_13 = ((float)(0x1000 - tm_13)) / 4096.0f;

	for (ButLEDnum=0;ButLEDnum<NUM_RGBBUTTONS;ButLEDnum++)
	{

		//BANK lights
		if (ButLEDnum == Bank1ButtonLED || ButLEDnum == Bank2ButtonLED || ButLEDnum == RecBankButtonLED)
		{
			if (ButLEDnum == Bank1ButtonLED) chan = 0;
			else if (ButLEDnum == Bank2ButtonLED) chan = 1;
			else chan = 2;

			if (chan==2 && global_mode[EDIT_MODE])
			{
				set_ButtonLED_byPalette(ButLEDnum, OFF);
			}
			else if (i_param[chan][BANK] < MAX_NUM_REC_BANKS)
				set_ButtonLED_byPalette(ButLEDnum, i_param[chan][BANK]+1 );

			else if (i_param[chan][BANK] < (MAX_NUM_REC_BANKS*2))
			{
				if (tm_14 < 0x1000)
					set_ButtonLED_byPalette(ButLEDnum, OFF );
				else
					set_ButtonLED_byPalette(ButLEDnum, i_param[chan][BANK] + 1 - MAX_NUM_REC_BANKS );
			}
			else
			{
				if ((tm_14 < 0x0400) || (tm_14 < 0x0C00 && tm_14 > 0x0800))
					set_ButtonLED_byPalette(ButLEDnum, OFF );
				else
					set_ButtonLED_byPalette(ButLEDnum, i_param[chan][BANK] + 1 - MAX_NUM_REC_BANKS*2 );

			}

		}

		// PLAY lights
		else if (ButLEDnum == Play1ButtonLED || ButLEDnum == Play2ButtonLED)
		{
			chan = (ButLEDnum == Play1ButtonLED) ? 0 : 1;

			if (chan && global_mode[EDIT_MODE])
			{
				set_ButtonLED_byPaletteFade(ButLEDnum, WHITE, VIOLET, tri_14);
			}
			else if (flags[PlaySample1Changed_valid + chan])
			{
				set_ButtonLED_byPalette(ButLEDnum, WHITE );
				flags[PlaySample1Changed_valid + chan]--;
			}
			else if (flags[PlaySample1Changed_empty + chan])
			{
				set_ButtonLED_byPalette(ButLEDnum, RED );
				flags[PlaySample1Changed_empty + chan]--;
			}

			else if (flags[AssignModeRefused+chan])
			{
				if (tm_12 <100) flags[AssignModeRefused+chan]--;

				if (tm_12 < 0x800)	set_ButtonLED_byPalette(ButLEDnum, RED);
				else				set_ButtonLED_byPalette(ButLEDnum, YELLOW);
			}
			else
			{
				if (play_state[chan]==SILENT
				|| play_state[chan]==RETRIG_FADEDOWN
				|| play_state[chan]==PLAY_FADEDOWN)
					if (i_param[chan][LOOPING]) 	set_ButtonLED_byPalette(ButLEDnum, YELLOW );
					else							set_ButtonLED_byPalette(ButLEDnum, OFF );

				//Playing
				else
					if (i_param[chan][LOOPING]) 	set_ButtonLED_byPalette(ButLEDnum, CYAN );
					else							set_ButtonLED_byPalette(ButLEDnum, GREEN );

			}


		}
		else if (ButLEDnum == RecButtonLED)
		{
			if (global_mode[EDIT_MODE])
			{
				set_ButtonLED_byPalette(RecButtonLED, OFF);
			}
			if (flags[RecSampleChanged_light])
			{
				set_ButtonLED_byPalette(RecButtonLED, WHITE);
				flags[RecSampleChanged_light]--;
			}
			else
			{
				//Solid Red = recording + monitoring
				//Solid Violet = recording, not monitoring
				//Flashing Red = recording enabled, not recording, but monitoring
				//Off = not enabled
				if (!global_mode[ENABLE_RECORDING])		set_ButtonLED_byPalette(RecButtonLED, OFF);
				else

					if (rec_state==REC_OFF
					|| rec_state==CLOSING_FILE
					|| rec_state==REC_PAUSED) {
						if (global_mode[MONITOR_RECORDING])	set_ButtonLED_byPalette(RecButtonLED, (tm_13 < 0x0800)? RED : OFF); //Off/paused/closing = flash red
						else 								set_ButtonLED_byPalette(RecButtonLED, (tm_13 < 0x0800)? VIOLET : OFF); //Off/paused/closing = flash red



					}else //recording
						if (global_mode[MONITOR_RECORDING])	set_ButtonLED_byPalette(RecButtonLED, RED); //rec + mon = solid red
						else 								set_ButtonLED_byPalette(RecButtonLED, VIOLET); //rec + no-mon = solid violet

			}

		}

		//Reverse1ButtonLED, Reverse2ButtonLED
		else if (ButLEDnum == Reverse1ButtonLED || ButLEDnum == Reverse2ButtonLED)
		{
			chan = (ButLEDnum == Reverse1ButtonLED) ? 0 : 1;

			if (global_mode[EDIT_MODE])
			{
				if (chan==0)
					if (flags[AssigningEmptySample])
						set_ButtonLED_byPalette(ButLEDnum, tm_13>0x1000 ? RED : OFF);//set_ButtonLED_byPaletteFade(ButLEDnum, RED, OFF, tri_13);
					else
						set_ButtonLED_byPaletteFade(ButLEDnum, OFF, GREEN, tri_14);
				else
						set_ButtonLED_byPaletteFade(ButLEDnum, OFF, RED, tri_14);

			}
			else if (flags[AssignModeRefused+chan])
			{
				if (tm_12 < 0x800)	set_ButtonLED_byPalette(ButLEDnum, RED);
				else 				set_ButtonLED_byPalette(ButLEDnum, YELLOW);

			}
			else
				if (i_param[chan][REV]) set_ButtonLED_byPalette(ButLEDnum, CYAN);
				else					set_ButtonLED_byPalette(ButLEDnum, OFF);


		}
	}



	display_all_ButtonLEDs();
}




void ButtonLED_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM10, TIM_IT_Update) != RESET)
	{
		update_ButtonLEDs();

		TIM_ClearITPendingBit(TIM10, TIM_IT_Update);
	}
}
