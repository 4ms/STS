#ifndef LED_PALETTE_H
#define LED_PALETTE_H

enum Palette {
	OFF,
	
	PINK,
	RED,
	ORANGE,
	YELLOW,
	GREEN,
	CYAN,
	BLUE,
	VIOLET,
	LAVENDAR,
	WHITE,

	AQUA,
	GREENER,
	CYANER,
	NUM_LED_PALETTE

};

const uint32_t LED_PALETTE[NUM_LED_PALETTE][3]=
{
		{0,		0, 		0},		//OFF

		{930,	240,	240},	//PINK
		{1000,	0,		0},		//RED
		{1000,	200,	0},		//ORANGE
		{600,	500,	0},		//YELLOW
		{0,		600,	60},	//GREEN
		{0,		560,	1000},	//CYAN
		{0,		0,		1000},	//BLUE
		{1000,	0,		1000},	//VIOLET
		{150,     0,    450}, 	//LAVENDAR
		{1000,	500,	700},	//WHITE

		{0, 	280, 	150},  	//AQUA
		{550, 	700, 	0}, 	//GREENER
		{200, 	800, 	1000} 	//CYANER
};

#endif
