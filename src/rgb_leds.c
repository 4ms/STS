/*
 * rgb_leds.c
 */

#include <dig_pins.h>
#include "rgb_leds.h"
#include "globals.h"


uint16_t Button_LED[NUM_RGBBUTTONS][3];

const uint32_t LED_PALETTE[84][3]={
		{0, 0, 761}, {0, 770, 766}, {0, 766, 16}, {700, 700, 700}, {763, 154, 0}, {766, 0, 112},
		{0, 0, 761}, {0, 780, 766}, {768, 767, 764}, {493, 768, 2}, {763, 154, 0}, {580, 65, 112},
		{767, 0, 0}, {767, 28, 386}, {0, 0, 764}, {0, 320, 387}, {767, 768, 1}, {767, 774, 765},
		{0, 0, 761}, {106, 0, 508}, {762, 769, 764}, {0, 767, 1}, {706, 697, 1}, {765, 179, 1},
		{0,0,900}, {200,200,816}, {800,800,800},	{900,400,800},	{900,500,202}, {800,200,0},
		{0,0,766}, {766,150,0}, {0,50,766}, {766,100,0}, {0,150,766}, {766,50,0},
		{0,0,1000}, {0,100,766}, {0,200,666}, {0,300,500}, {0,350,500}, {0,350,400},

		{895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023},
		{1023, 1, 1023}, {1023, 0, 602}, {1021, 0, 217}, {1023, 0, 118}, {1023, 0, 105}, {1023, 4, 65},
		{1023, 1, 0}, {1023, 33, 0}, {1023, 139, 0}, {1023, 275, 0}, {1023, 446, 0}, {1023, 698, 0},
		{0, 1023, 0}, {0, 1022, 0}, {1, 1020, 4}, {0, 1023, 164}, {1, 1023, 86}, {0, 1023, 94},
		{612, 37, 0}, {614, 0, 203}, {126, 0, 898}, {0, 1023, 461}, {894, 1023, 0}, {941, 996, 1000},
		{1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0},
		{1023, 1023, 0}, {1022, 1021, 1}, {1021, 1022, 32}, {1023, 1023, 110}, {1023, 1023, 164}, {1023, 1023, 222}
};

/*
 * Initializes the Button_LED array
 * The elements are organized into  RGBLED components, and then red, green, and blue elements:
 * LED_Element_Brightness <==> Button_LED [RGB_Button_Number] [0=red, 1=green, 2=blue]
 */
void init_buttonLEDs(void)
{
	uint8_t i;
	for (i=0;i<NUM_RGBBUTTONS;i++)
	{
		Button_LED[i][0]=0;
		Button_LED[i][1]=0;
		Button_LED[i][2]=0;
	}
}

/*
 * Updates the display of one RGB LED given by the number ButtonLED_number
 */
void update_one_ButtonLED(uint8_t ButtonLED_number)
{
	LEDDriver_setRGBLED(ButtonLED_number, ( Button_LED[ButtonLED_number][0] <<20) | ( Button_LED[ButtonLED_number][1] <<10) | Button_LED[ButtonLED_number][2] );
}


/*
 * Updates the display of all the RGB LEDs
 */
void update_all_ButtonLEDs(void)
{
	uint8_t i;

	for (i=0;i<NUM_RGBBUTTONS;i++)
	{
		LEDDriver_setRGBLED(i, ( Button_LED[i][0] <<20) | ( Button_LED[i][1] <<10) | Button_LED[i][2] );
	}

}

void test_all_buttonLEDs(void)
{
	uint8_t i, j;

	for (j=0;j<8;j++)
	{
		LEDDriver_setRGBLED(j,0 );
	}


	for (j=0;j<8;j++)
	{
		i=j+9;
		LEDDriver_setRGBLED(j, ( LED_PALETTE[i][0] <<20) | ( LED_PALETTE[i][1] <<10) | LED_PALETTE[i][2] );
	}

}

