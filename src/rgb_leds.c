/*
 * rgb_leds.c
 */
#include "globals.h"
#include "dig_pins.h"
#include "params.h"
#include "adc.h"
#include "rgb_leds.h"
#include "pca9685_driver.h"
#include "sampler.h"
#include "wav_recording.h"
#include "calibration.h"
#include "button_knob_combo.h"
#include "res/LED_palette.h"
#include "edit_mode.h"
#include "system_mode.h"

#define BIG_PLAY_BUTTONS
#define FROSTED_BUTTONS

//#define DEBUG_SETBANK1RGB


const uint32_t LED_PALETTE[NUM_LED_PALETTE][3]=
{
		{0,		0, 		0},		//OFF

		{930,	950,	900},	//WHITE
		{1000,	0,		0},		//RED
		{1000,	200,	0},		//ORANGE
		{700,	650,	0},		//YELLOW
		{0,		600,	60},	//GREEN
		{0,		560,	1000},	//CYAN
		{0,		0,		1000},	//BLUE
		{1000,	0,		1000},	//MAGENTA
		{410,   0,      1000}, 	//LAVENDER
		{150,	150,	200},	//PEARL

		{0, 	280, 	150},  	//AQUA
		{550, 	700, 	0}, 	//GREENER
		{200, 	800, 	1000}, 	//CYANER
		{100, 	100, 	100}, 	//DIM_WHITE
		{50, 	0, 		0}, 	//DIM_RED
		{150, 	0, 		0}	 	//MED_RED


};



uint16_t 						ButLED_color[NUM_RGBBUTTONS][3];
uint16_t 						cached_ButLED_color[NUM_RGBBUTTONS][3];
uint8_t	 						ButLED_state[NUM_RGBBUTTONS];

extern uint8_t 					i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern enum PlayStates 			play_state[NUM_PLAY_CHAN];
extern enum RecStates			rec_state;

extern uint8_t 					flags[NUM_FLAGS];

extern uint8_t					global_mode[NUM_GLOBAL_MODES];

extern volatile uint32_t 		sys_tmr;

extern int16_t 					i_smoothed_potadc[NUM_POT_ADCS];

extern uint32_t 				play_led_flicker_ctr[NUM_PLAY_CHAN];

extern ButtonKnobCombo 			g_button_knob_combo[NUM_BUTTON_KNOB_COMBO_BUTTONS][NUM_BUTTON_KNOB_COMBO_KNOBS];

extern uint8_t 					cur_assign_bank;
extern enum AssignmentStates 	cur_assign_state;

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
			if (i ==  Play1ButtonLED || i == Play2ButtonLED)
				LEDDriver_setRGBLED_RGB(i, ButLED_color[i][0]*4, ButLED_color[i][2]*4, ButLED_color[i][1]*4); // swapped [1] and [2]
			else
				LEDDriver_setRGBLED_RGB(i, ButLED_color[i][0]*4, ButLED_color[i][1]*4, ButLED_color[i][2]*4);
			cached_ButLED_color[i][0]=ButLED_color[i][0];
			cached_ButLED_color[i][1]=ButLED_color[i][1];
			cached_ButLED_color[i][2]=ButLED_color[i][2];
		}
	}

}

void all_buttonLEDs_off(void)
{
	uint8_t j;
	for (j=0;j<8;j++)
	{
		LEDDriver_setRGBLED(j,0 );
	}
}

void fade_all_buttonLEDs(void)
{
	uint8_t i, j;
	float t=0.0f;

	LEDDRIVER_OUTPUTENABLE_ON;

	for (j=0;j<8;j++)
	{
		LEDDriver_setRGBLED(j,0 );
	}

	for(i=0;i<(NUM_LED_PALETTE-1);i++)
	{
		for (t=0.0f;t<=1.0f;t+=0.005f)
		{
			for (j=0;j<8;j++)
			{
				set_ButtonLED_byPaletteFade(j, i, i+1, t);
			}

			display_all_ButtonLEDs();
		}
	}
}

void chase_all_buttonLEDs(uint32_t del)
{
	uint8_t i, j;

	LEDDRIVER_OUTPUTENABLE_ON;

	for (j=0;j<(NUM_RGBBUTTONS*3);j++)
	{
		delay_ms(del);
		for (i=0; i<(NUM_RGBBUTTONS*3); i++)
			LEDDriver_set_one_LED(i, i==j ? 2000: 0);
	}
}

uint32_t display_bank_blink(uint8_t ButLEDnum, uint8_t bank_to_display, uint32_t tmr)
{

	if (bank_to_display <= 9) //solid color, no blinks
	{
		set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1 );
	}
	else 
	if (bank_to_display <= 19) //one blink
	{
		if (	tmr < 0x1000) set_ButtonLED_byPalette(ButLEDnum, OFF);
		else 				set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-10 );
	}
	else
	if (bank_to_display <= 29) //two blinks
	{
		if 		(tmr < 0x1000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x3000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-20 );
		else if (tmr < 0x4000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else 				  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-20 );
	}
	else
	if (bank_to_display <= 39) //three blinks
	{
		if 		(tmr < 0x1000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x3000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-30 );
		else if (tmr < 0x4000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x6000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-30 );
		else if (tmr < 0x7000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else 				  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-30 );
	}
	else
	if (bank_to_display <= 49) //four blinks
	{
		if 		(tmr < 0x1000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x3000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-40 );
		else if (tmr < 0x4000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x6000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-40 );
		else if (tmr < 0x7000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x9000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-40 );
		else if (tmr < 0xA000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else 				  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-40 );
	}
	else
	if (bank_to_display <= 59) //five blinks
	{
		if 		(tmr < 0x1000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x3000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-50 );
		else if (tmr < 0x4000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x6000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-50 );
		else if (tmr < 0x7000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0x9000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-50 );
		else if (tmr < 0xA000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else if (tmr < 0xC000)  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-50 );
		else if (tmr < 0xD000)  set_ButtonLED_byPalette(ButLEDnum, OFF);
		else 				  set_ButtonLED_byPalette(ButLEDnum, bank_to_display+1-50 );
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
	uint8_t bank_to_display;
	//uint32_t tm=sys_tmr & 0x3FFF; //14-bit counter
	//uint32_t tm_7 = sys_tmr & 0x7F; //7-bit counter
	//uint32_t tm_12 = sys_tmr & 0xFFF; //12-bit counter
	uint32_t tm_13 = sys_tmr & 0x1FFF; //13-bit counter
	uint32_t tm_14 = sys_tmr & 0x3FFF; //14-bit counter
//	uint32_t tm_15 = sys_tmr & 0x7FFF; //15-bit counter
	uint32_t tm_16 = sys_tmr & 0xFFFF; //16-bit counter
	//float tri_16;
	//float tri_15;
	float tri_14;
	float tri_13;

	static uint32_t st_phase=0;

	float t_tri;
	uint32_t t;

	// if (tm_16>0x8000)
	// 	tri_16 = ((float)(tm_16 - 0x8000)) / 32768.0f;
	// else
	// 	tri_16 = ((float)(0x8000 - tm_16)) / 32768.0f;

	// if (tm_15>0x4000)
	// 	tri_15 = ((float)(tm_15 - 0x4000)) / 16384.0f;
	// else
	// 	tri_15 = ((float)(0x4000 - tm_15)) / 16384.0f;

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

		//
		// Animations of all lights:
		//

		// Writing index
		if (flags[RewriteIndex])
		{	
			if (ButLEDnum!=Play1ButtonLED && ButLEDnum!=Play2ButtonLED)
				set_ButtonLED_byPalette(ButLEDnum, flags[RewriteIndex]);
			else
				set_ButtonLED_byPalette(ButLEDnum, OFF);

		}
		else

 		// Successfully wrote index: flicker top row of buttons green
		if (flags[RewriteIndexSucess] && (ButLEDnum == Bank1ButtonLED || ButLEDnum == Bank2ButtonLED || ButLEDnum == Play1ButtonLED || ButLEDnum == Play2ButtonLED))
		{
			set_ButtonLED_byPalette(ButLEDnum, GREEN);
			flags[RewriteIndexSucess]--;
		}
		else

		// Failed writing index: flicker all buttons red
		if (flags[RewriteIndexFail])
		{
			set_ButtonLED_byPalette(ButLEDnum, RED);
			flags[RewriteIndexFail]--;
		}
		else

		if (flags[RevertAll])
		{
			set_ButtonLED_byPalette(ButLEDnum, GREEN);
			flags[RevertAll]--;
		}
		else

		// Success re-loading bank 1: flash all ch1 buttons green
		if (flags[RevertBank1] && (ButLEDnum == Bank1ButtonLED || ButLEDnum == Play1ButtonLED || ButLEDnum == Reverse1ButtonLED || ButLEDnum == RecButtonLED))
		{
			set_ButtonLED_byPalette(ButLEDnum, GREEN);
			flags[RevertBank1]--;
		}
		else
		if (flags[RevertBank2] && (ButLEDnum == Bank2ButtonLED || ButLEDnum == Play2ButtonLED || ButLEDnum == Reverse2ButtonLED || ButLEDnum == RecBankButtonLED))
		{
			set_ButtonLED_byPalette(ButLEDnum, GREEN);
			flags[RevertBank1]--;
		}
		else
		
		if (flags[ChangedTrigDelay])
		{
			//initialize phase
			if (flags[ChangedTrigDelay] == 1){
				// st_phase = 0xFFFFFFFF - sys_tmr + 1; //t will start at 0
				st_phase = sys_tmr;
				flags[ChangedTrigDelay]++;
			}

			// t = (sys_tmr + st_phase) & 0x7FF;
			t = sys_tmr - st_phase;

			if (flags[ChangedTrigDelay] == 2){
				set_ButtonLED_byPalette(RecButtonLED, WHITE);
				set_ButtonLED_byPalette(RecBankButtonLED, OFF);
				flags[ChangedTrigDelay]++;
			}
			else
			if ( (flags[ChangedTrigDelay] == 3) && (t > calc_trig_delay(global_mode[TRIG_DELAY]) * 4) )
			{
				set_ButtonLED_byPalette(RecButtonLED, WHITE);
				set_ButtonLED_byPalette(RecBankButtonLED, GREEN);
				flags[ChangedTrigDelay]++;
				st_phase = sys_tmr;
			}
			else
			if ( (flags[ChangedTrigDelay] == 4) && (t > calc_trig_delay(global_mode[TRIG_DELAY]) * 4) )
			{
				set_ButtonLED_byPalette(RecButtonLED, OFF);
				set_ButtonLED_byPalette(RecBankButtonLED, OFF);
				flags[ChangedTrigDelay]++;
				st_phase = sys_tmr;
			}
			else
			if ( (flags[ChangedTrigDelay] == 5) && (t > 10000) )
			{
				flags[ChangedTrigDelay]=0;
			}

		}
		else

		if (flags[SkipProcessButtons]==2)
		{
			set_ButtonLED_byPalette(ButLEDnum, DIM_WHITE);
		}
		else

		//
		// Normal functions:
		//

		//BANK lights
		if (ButLEDnum == Bank1ButtonLED || ButLEDnum == Bank2ButtonLED || ButLEDnum == RecBankButtonLED)
		{
			if 		(ButLEDnum == Bank1ButtonLED) 	chan = 0;
			else if (ButLEDnum == Bank2ButtonLED) 	chan = 1;
			else if (ButLEDnum == RecBankButtonLED)	chan = 2;

#ifdef DEBUG_SETBANK1RGB
			if (chan==0 && global_mode[EDIT_MODE]) set_ButtonLED_byRGB(ButLEDnum, i_smoothed_potadc[0]/4, i_smoothed_potadc[1]/4, i_smoothed_potadc[2]/4);
			else 
#endif
			if (chan==2 && global_mode[EDIT_MODE]) //REC Bank button LED off when in Edit Mode
			{
				set_ButtonLED_byPalette(ButLEDnum, OFF);
			}
			else
			if (flags[PlayBankHover1Changed + chan])
			{
				flags[PlayBankHover1Changed + chan]--;
				set_ButtonLED_byPalette(ButLEDnum, CYANER);
			}
			else
			{
				if (chan==2) //REC button
				{
					//Display bank hover value
					if (g_button_knob_combo[bkc_RecBank][bkc_RecSample].combo_state == COMBO_ACTIVE)
						bank_to_display = g_button_knob_combo[bkc_RecBank][bkc_RecSample].hover_value;
					else
						bank_to_display = i_param[2][BANK];
				}
				else //chan must be 0 or 1:
				if (g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1].combo_state == COMBO_ACTIVE)
					bank_to_display = g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample1].hover_value;
				else
				if (g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample2].combo_state == COMBO_ACTIVE)
					bank_to_display = g_button_knob_combo[bkc_Bank1 + chan][bkc_Sample2].hover_value;
				else
					bank_to_display = i_param[chan][BANK];

				display_bank_blink(ButLEDnum, bank_to_display, tm_16);

			}
		}

		// PLAY lights
		else if (ButLEDnum == Play1ButtonLED || ButLEDnum == Play2ButtonLED)
		{
			chan = (ButLEDnum == Play1ButtonLED) ? 0 : 1;

			if (chan && global_mode[EDIT_MODE])
			{
				set_ButtonLED_byPaletteFade(ButLEDnum, WHITE, MAGENTA, tri_14);
			}
			else
			if (flags[PlaySample1Changed_valid + chan]>1)
			{
				set_ButtonLED_byPalette(ButLEDnum, WHITE );
				flags[PlaySample1Changed_valid + chan]--;
			}
			else
			if (flags[PlaySample1Changed_empty + chan]>1)
			{
				set_ButtonLED_byPalette(ButLEDnum, RED );
				flags[PlaySample1Changed_empty + chan]--;
			}

			else
			if (flags[StereoModeTurningOn])
			{
				//reset st_phase
				if (flags[StereoModeTurningOn] == 1) {
					st_phase = 0xFFFFFFFF - sys_tmr + 1; //t will start at 0
					flags[StereoModeTurningOn]++;
				}

				//calc fade
				t = (sys_tmr + st_phase) & 0x3FFF;
				if (t >0x2000)			t_tri = ((float)(t - 0x2000)) / 8192.0f;
				else					t_tri = ((float)(0x2000 - t)) / 8192.0f;

				set_ButtonLED_byPaletteFade(ButLEDnum, GREENER, OFF, t_tri);

				if ((t < 0x1000) && (flags[StereoModeTurningOn] & 1)==1) 					flags[StereoModeTurningOn]++;
				if ((t > 0x2000) && (t < 0x3000) && (flags[StereoModeTurningOn] & 1)==0) 	flags[StereoModeTurningOn]++;

				if (flags[StereoModeTurningOn] > 5) flags[StereoModeTurningOn] = 0;

			}
			else if (flags[StereoModeTurningOff])
			{
				//reset st_phase
				if (flags[StereoModeTurningOff] == 1) {
					st_phase = 0xFFFFFFFF - sys_tmr;
					flags[StereoModeTurningOff]++;
				}

				t = (sys_tmr + st_phase) & 0x3FFF;

				if (flags[StereoModeTurningOff] == 2){
					set_ButtonLED_byPalette(Play1ButtonLED, OFF);
					set_ButtonLED_byPalette(Play2ButtonLED, OFF);
				}
				else if (flags[StereoModeTurningOff] < 5){
					if ((t&0x1FFF) < 0x1000)	set_ButtonLED_byPalette(Play1ButtonLED, GREEN);
					else						set_ButtonLED_byPalette(Play1ButtonLED, OFF);

					set_ButtonLED_byPalette(Play2ButtonLED, OFF);
				}
				else if (flags[StereoModeTurningOff] < 7){
					if ((t&0x1FFF) < 0x1000)	set_ButtonLED_byPalette(Play2ButtonLED, GREEN);
					else						set_ButtonLED_byPalette(Play2ButtonLED, OFF);

					set_ButtonLED_byPalette(Play1ButtonLED, OFF);
				}

				if ((t < 0x1000) && (flags[StereoModeTurningOff] & 1)==1) 					flags[StereoModeTurningOff]++;
				if ((t > 0x2000) && (t < 0x3000) && (flags[StereoModeTurningOff] & 1)==0) 	flags[StereoModeTurningOff]++;

				if (flags[StereoModeTurningOff] > 7) flags[StereoModeTurningOff] = 0;

			}
			else
			{
				if (play_state[chan]==SILENT
				|| play_state[chan]==RETRIG_FADEDOWN
				|| play_state[chan]==PLAY_FADEDOWN
				|| play_led_flicker_ctr[chan]
				)
				//Not playing, or re-starting a sample
				{
					//Sample slot empty
					if (flags[PlaySample1Changed_empty + chan])
					{
						if (i_param[chan][LOOPING]) 	set_ButtonLED_byPalette(ButLEDnum, MED_RED );
						else							set_ButtonLED_byPalette(ButLEDnum, DIM_RED );
					}
					//Sample found
					else
					{
						set_ButtonLED_byPalette(ButLEDnum, OFF );
						// if (i_param[chan][LOOPING]) 	set_ButtonLED_byPalette(ButLEDnum, OFF );
						// else							set_ButtonLED_byPalette(ButLEDnum, DIM_WHITE );
					}
				}
				//Playing
				else
				{
					if (global_mode[STEREO_MODE])
					{
						if (i_param[chan][LOOPING]) 	set_ButtonLED_byPalette(ButLEDnum, CYAN );
						else							set_ButtonLED_byPalette(ButLEDnum, GREENER );
					}
					else {
						if (i_param[chan][LOOPING]) 	set_ButtonLED_byPalette(ButLEDnum, BLUE );
						else							set_ButtonLED_byPalette(ButLEDnum, GREEN );
					}
				}
			}

		}

		//Rec button Light
		else if (ButLEDnum == RecButtonLED)
		{
			if (global_mode[EDIT_MODE])
			{
				set_ButtonLED_byPalette(RecButtonLED, OFF);
			}
			else if (flags[RecSampleChanged_light])
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
					) {
						if (global_mode[MONITOR_RECORDING])	set_ButtonLED_byPalette(RecButtonLED, (tm_13 < 0x0800)? RED : OFF); //Off/paused/closing = flash red
						else 								set_ButtonLED_byPalette(RecButtonLED, (tm_13 < 0x0800)? MAGENTA : OFF); //Off/paused/closing = flash red



					}else //recording
						if (global_mode[MONITOR_RECORDING])	set_ButtonLED_byPalette(RecButtonLED, RED); //rec + mon = solid red
						else 								set_ButtonLED_byPalette(RecButtonLED, MAGENTA); //rec + no-mon = solid violet

			}

		}

		//Reverse Lights
		else if (ButLEDnum == Reverse1ButtonLED || ButLEDnum == Reverse2ButtonLED)
		{
			chan = (ButLEDnum == Reverse1ButtonLED) ? 0 : 1;

			if (global_mode[EDIT_MODE])
			{
				if (chan==0)
				{
					//When we press Edit+Rev1,
					//Flicker the Rev1 light White for forward motion
					//And Red for backward motion
					//But-- if we're on a Red or White bank, then flicker it off
					if (flags[AssignedNextSample])
					{
						set_ButtonLED_byPalette(ButLEDnum, ((cur_assign_bank%10)==(WHITE-1)) ? OFF: WHITE);
						flags[AssignedNextSample]--;
					}
					else
					if (flags[AssignedPrevBank])
					{
						set_ButtonLED_byPalette(ButLEDnum, ((cur_assign_bank%10) == (RED-1))? OFF: RED);
						flags[AssignedPrevBank]--;
					}
					else

					// Reverse light during assignment: shows bank we're assigning from
					//
					if (global_mode[ASSIGN_MODE])
					{
						if (cur_assign_bank<MAX_NUM_BANKS)
							// When we're looking for already assigned samples
							// display the current bank we're scanning on the Rev1 light
							display_bank_blink(ButLEDnum, cur_assign_bank, tm_16);
						else
							// When we're scanning unassigned samples:
							// Flicker the bank base color rapidly if scanning unassigned samples in the folder
						if (cur_assign_state==ASSIGN_IN_FOLDER)
							set_ButtonLED_byPaletteFade(ButLEDnum, OFF, (i_param[0][BANK] % 10)+1, tri_13);

							// Flicker dim red/white if we're scanning unsassigned samples outside of the sample's folder
						else
						if (cur_assign_state==ASSIGN_UNUSED_IN_FS || cur_assign_state==ASSIGN_UNUSED_IN_ROOT)
							set_ButtonLED_byPaletteFade(ButLEDnum, DIM_RED, DIM_WHITE, tri_13);
					}
					else
					{
						//Edit Mode, but not Assignment mode --> bank's base color
						set_ButtonLED_byPaletteFade(ButLEDnum, OFF, (i_param[0][BANK] % 10)+1, tri_14);
					}
				}
				else //Edit mode and chan==1
				{
					//Edit Mode: Rev2 flashes red/yellow if there's an undo available
					if (flags[UndoSampleDiffers])
						set_ButtonLED_byPaletteFade(ButLEDnum, YELLOW, RED, tri_14);
					else
						set_ButtonLED_byPalette(ButLEDnum, DIM_WHITE);
				}
			}
			else //Not edit mode
			{
				if (flags[PercEnvModeChanged]){
					if (global_mode[PERC_ENVELOPE])
						set_ButtonLED_byPaletteFade(ButLEDnum, YELLOW, OFF, (float)flags[PercEnvModeChanged] / 100.0);
					else
					{
						if (flags[PercEnvModeChanged]>70)
							set_ButtonLED_byPalette(ButLEDnum, YELLOW);
						else
							set_ButtonLED_byPalette(ButLEDnum, OFF);
					}
					flags[PercEnvModeChanged]--;
				}
				else
				if (flags[FadeEnvModeChanged]){
					if (global_mode[FADEUPDOWN_ENVELOPE])
						set_ButtonLED_byPaletteFade(ButLEDnum, RED, OFF, (float)flags[FadeEnvModeChanged] / 100.0);
					else
					{
						if (flags[FadeEnvModeChanged]>70)
							set_ButtonLED_byPalette(ButLEDnum, RED);
						else
							set_ButtonLED_byPalette(ButLEDnum, OFF);
					}
					flags[FadeEnvModeChanged]--;
				}
				else
				if (i_param[chan][REV]) set_ButtonLED_byPalette(ButLEDnum, CYAN);
				else					set_ButtonLED_byPalette(ButLEDnum, OFF);
			}

		}
	}

}




void ButtonLED_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM10, TIM_IT_Update) != RESET)
	{
		if (global_mode[CALIBRATE])
			update_calibration_button_leds();
		else
		if (global_mode[SYSTEM_MODE])
			update_system_mode_button_leds();
		else
			update_ButtonLEDs();

		display_all_ButtonLEDs();

		TIM_ClearITPendingBit(TIM10, TIM_IT_Update);
	}
}
