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
			LEDDriver_setRGBLED(i, ( ButLED_color[i][0] <<20) | ( ButLED_color[i][1] <<10) | ButLED_color[i][2] );
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

uint16_t ButLED_statetable[NUM_RGBBUTTONS][5]={
		{OFF, BLUE, BLUE, BLUE, WHITE}, 	/*RecBankButtonLED*/
		{OFF, RED, BLUE, VIOLET, WHITE}, 	/*RecButtonLED*/
		{OFF, CYAN, CYAN, CYAN, WHITE}, 	/*Reverse1ButtonLED*/
		{OFF, GREEN, YELLOW, BLUE, WHITE}, 	/*Play1ButtonLED*/
		{OFF, WHITE, WHITE, WHITE, WHITE}, 	/*Bank1ButtonLED*/
		{OFF, WHITE, WHITE, WHITE, WHITE}, 	/*Bank2ButtonLED*/
		{OFF, GREEN, YELLOW, BLUE, WHITE}, 	/*Play2ButtonLED*/
		{OFF, CYAN, CYAN, CYAN, WHITE} 	/*Reverse2ButtonLED*/

};

void update_ButtonLED_states(void)
{

	if (global_mode[MONITOR_AUDIO])		ButLED_state[RecButtonLED] |= 0b10;
	else								ButLED_state[RecButtonLED] &= ~0b10;

	if (rec_state==REC_OFF
	|| rec_state==CLOSING_FILE
	|| rec_state==REC_PAUSED)			ButLED_state[RecButtonLED] &= ~0b01;
	else								ButLED_state[RecButtonLED] |= 0b01;



	if (flags[PlaySample1Changed_light])
	{
		ButLED_state[Play1ButtonLED] = 4;
		flags[PlaySample1Changed_light]--;
	}
	else
	{
		if (play_state[0]==SILENT
		|| play_state[0]==RETRIG_FADEDOWN
		|| play_state[0]==PLAY_FADEDOWN)	ButLED_state[Play1ButtonLED] = 0;
		else								ButLED_state[Play1ButtonLED] = 1;

		if (i_param[0][LOOPING])			ButLED_state[Play1ButtonLED] += 2;
	}

	if (flags[PlaySample2Changed_light])
	{
		ButLED_state[Play2ButtonLED] = 4;
		flags[PlaySample2Changed_light]--;
	}
	else
	{
		if (play_state[1]==SILENT
		|| play_state[1]==RETRIG_FADEDOWN
		|| play_state[1]==PLAY_FADEDOWN)	ButLED_state[Play2ButtonLED] = 0;
		else								ButLED_state[Play2ButtonLED] = 1;


		if (i_param[1][LOOPING])			ButLED_state[Play2ButtonLED] += 2;
	}


	ButLED_state[Reverse1ButtonLED] = i_param[0][REV];
	ButLED_state[Reverse2ButtonLED] = i_param[1][REV];


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
	uint32_t tm=sys_tmr & 0x3FFF;


	update_ButtonLED_states();

	for (ButLEDnum=0;ButLEDnum<NUM_RGBBUTTONS;ButLEDnum++)
	{
		if (ButLEDnum == Bank1ButtonLED || ButLEDnum == Bank2ButtonLED || ButLEDnum == RecBankButtonLED)
		{
			if (ButLEDnum == Bank1ButtonLED) chan = 0;
			else if (ButLEDnum == Bank2ButtonLED) chan = 1;
			else chan = 2;

			if (i_param[chan][BANK] < (NUM_LED_PALETTE-1))
				set_ButtonLED_byPalette(ButLEDnum, i_param[chan][BANK]+1 );

			else if (i_param[chan][BANK] < ((NUM_LED_PALETTE-1)*2))
			{
				if (tm < 0x1000)
					set_ButtonLED_byPalette(ButLEDnum, OFF );
				else
					set_ButtonLED_byPalette(ButLEDnum, i_param[chan][BANK] + 2 - NUM_LED_PALETTE );
			}
			else
			{
				if ((tm < 0x0400) || (tm < 0x0C00 && tm > 0x0800))
					set_ButtonLED_byPalette(ButLEDnum, OFF );
				else
					set_ButtonLED_byPalette(ButLEDnum, i_param[chan][BANK] + 3 - NUM_LED_PALETTE*2 );

			}

		}

		else
			set_ButtonLED_byPalette(ButLEDnum, ButLED_statetable[ButLEDnum][ButLED_state[ButLEDnum]] );
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
