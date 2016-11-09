/*
 * rgb_leds.c
 */

#include <dig_pins.h>
#include "rgb_leds.h"
#include "globals.h"
#include "LED_palette.h"

uint16_t ButLED_color[NUM_RGBBUTTONS][3];
uint16_t cached_ButLED_color[NUM_RGBBUTTONS][3];
uint8_t ButLED_state[NUM_RGBBUTTONS];

extern uint32_t g_error;


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

//		cached_ButLED_color[i][0]=0xFF;
//		cached_ButLED_color[i][1]=0xFF;
//		cached_ButLED_color[i][2]=0xFF;
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

uint16_t ButLED_statetable[NUM_RGBBUTTONS][2]={
		{OFF, BLUE}, 	/*RecBankButtonLED*/
		{OFF, RED}, 	/*RecButtonLED*/
		{OFF, CYAN}, 	/*Reverse1ButtonLED*/
		{OFF, WHITE}, 	/*Bank1ButtonLED*/
		{OFF, GREEN}, 	/*Play1ButtonLED*/
		{OFF, GREEN}, 	/*Play2ButtonLED*/
		{OFF, WHITE}, 	/*Bank2ButtonLED*/
		{OFF, CYAN} 	/*Reverse2ButtonLED*/

};

/*
 * update_ButtonLEDs()
 *
 * Calculates the color of each button LED and tells the LED Driver chip to update the display
 *
 */
void update_ButtonLEDs(void)
{
	uint8_t ButLEDnum;

	if (!g_error){
		for (ButLEDnum=0;ButLEDnum<NUM_RGBBUTTONS;ButLEDnum++)
		{
			set_ButtonLED_byPalette(ButLEDnum, ButLED_statetable[ButLEDnum][ButLED_state[ButLEDnum]] );
		}
	}
/*
	if (ButLED_state[Reverse1ButtonLED]==1)
		set_ButtonLED_byPalette(Reverse1ButtonLED,YELLOW);
	else
		set_ButtonLED_byPalette(Reverse1ButtonLED,OFF);

	if (ButLED_state[Reverse2ButtonLED]==1)
		set_ButtonLED_byPalette(Reverse2ButtonLED,YELLOW);
	else
		set_ButtonLED_byPalette(Reverse2ButtonLED,OFF);

	if (ButLED_state[Play1ButtonLED]==1)
		set_ButtonLED_byPalette(Play1ButtonLED,GREEN);
	else
		set_ButtonLED_byPalette(Play1ButtonLED,OFF);

	if (ButLED_state[Play2ButtonLED]==1)
		set_ButtonLED_byPalette(Play2ButtonLED,GREEN);
	else
		set_ButtonLED_byPalette(Play2ButtonLED,OFF);

	if (ButLED_state[Bank1ButtonLED]==1)
		set_ButtonLED_byPalette(Bank1ButtonLED,WHITE);
	else
		set_ButtonLED_byPalette(Bank1ButtonLED,OFF);

	if (ButLED_state[Bank2ButtonLED]==1)
		set_ButtonLED_byPalette(Bank2ButtonLED,WHITE);
	else
		set_ButtonLED_byPalette(Bank2ButtonLED,OFF);

	if (ButLED_state[RecButtonLED]==1)
		set_ButtonLED_byPalette(RecButtonLED,RED);
	else
		set_ButtonLED_byPalette(RecButtonLED,OFF);

	if (ButLED_state[RecBankButtonLED]==1)
		set_ButtonLED_byPalette(RecBankButtonLED,BLUE);
	else
		set_ButtonLED_byPalette(RecBankButtonLED,OFF);
*/

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
