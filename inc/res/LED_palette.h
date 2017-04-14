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
	NUM_LED_PALETTE

};

const uint32_t LED_PALETTE[NUM_LED_PALETTE][3]=
{
		{0,		0, 		0},		//OFF
		{700,	700,	500},	//WHITE
		{1000,	0,	50},	//RED
		{90,		600,	0},		//GREEN
		{0,		0,		1000},	//BLUE
		{600,	500,	0},		//YELLOW
		{0,		560,	1000},	//CYAN
		{1000,	200,	0},		//ORANGE
		{1000,	0,		1000},	//VIOLET
		{1000,	0,		500}	//PINK

};

