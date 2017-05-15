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
	FULL,
	GREENER,
	CYANER,
	NUM_LED_PALETTE

};

const uint32_t LED_PALETTE[NUM_LED_PALETTE][3]=
{
		{0,		0, 		0},		//OFF
		{1000,	500,	700},	//WHITE
		{1000,	0,	50},	//RED
		{0,		600,	60},		//GREEN
		{0,		0,		1000},	//BLUE
		{600,	500,	0},		//YELLOW
		{0,		560,	1000},	//CYAN
		{1000,	200,	0},		//ORANGE
		{1000,	0,		1000},	//VIOLET
		{1000,	0,		500},	//PINK
		{1023, 1023, 1023}, //FULL
		{550, 700, 0}, //GREENER
		{200, 800, 1000} //CYANER

};

