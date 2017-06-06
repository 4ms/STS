#ifndef LED_PALETTE_H
#define LED_PALETTE_H

enum Palette {
	OFF,
	WHITE,
	RED,
	GREEN,
	BLUE,
	YELLOW,
	CYAN,
	ORANGE,
	VIOLET,
	PINK,
	LAVENDAR,

	AQUA,
	GREENER,
	CYANER,
	NUM_LED_PALETTE

};

const uint32_t LED_PALETTE[NUM_LED_PALETTE][3]=
{
		{0,		0, 		0},		//OFF

		{1000,	500,	700},	//WHITE
		{1000,	0,		0},		//RED
		{0,		600,	60},	//GREEN
		{0,		0,		1000},	//BLUE
		{600,	500,	0},		//YELLOW
		{0,		560,	1000},	//CYAN
		{1000,	200,	0},		//ORANGE
		{1000,	0,		1000},	//VIOLET
		{930,	240,	240},	//PINK
		{150,     0,    450}, 	//LAVENDAR

		{0, 	280, 	150},  	//AQUA
		{550, 	700, 	0}, 	//GREENER
		{200, 	800, 	1000} 	//CYANER
};

#endif
