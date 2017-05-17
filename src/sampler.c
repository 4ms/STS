/*
 * sampler.c

//Is checking polarity and wrapping changes necessary? Line 940
//Is reversing while PREBUFFERING working? Line 341
//Still kind of weird when running the same trigger into REV and PLAY


		//improvement idea #1: load all 19 samples in the current bank, opening their files. So we have:
		//FIL sample_files[19];
		//Then we when play, set the channel's file to the sample file (somehow jump around to play the same file in two channels)

		//#2:
		//On change bank, load all 19 sample headers and store info into samples[].
		//On play, open the file and jump right to startOfData.

		//#3:
		//On change bank, load all 19 sample headers into samples[]
		//Load the first 100ms or so of data into SDRAM.
		//We'd want to divide SDRAM into halves for Play and Rec,
		//and then divide Play into 19 slots = max 9.1s@48k/16b or 441k samples or 1724 blocks of 512-bytes. So we could pre-load up to 1724 blocks per sample
		//Set it up to load blocks in the background as we have time (e.g. if no sample is playing), filling the SDRAM space for the sample
		//Don't let a sample start playing until it has a minimum number of blocks pre-loaded
		//???? This doesn't allow us to play from a different start position!!!
		//

 */

/* Improvements:
 *
 * Use structs:
 * -params (combine i_param + f_param)
 * -adc_value: uint16_t raw_val, uint16_t i_smoothed_val, float smoothed_val;
 *
 *
ToDo: Allow (in system mode?) a selection of length param modes:
 1) Playback time is fixed, as if an EG->VCA is applied to output and LENGTH controls the decay time
			PITCH and REV do not change playback time

 2) Fixed sound clip:
	  (define points in sample file to not read past and set fully_buffered[]=1 when reaching those points)
    2a) Stop point(s) are set by LENGTH: so it stops at a certain point (# of samples in file) ahead of the startpos and/or behind startpos (limited by the beginning/end of the sample file of course)
    2b) Stop point #1 is set by LENGTH and Stop point #2 is the start point (identical to 2a if startpos is 0). So if you hit reverse while playing, it stops where you started, never goes before that.
			PITCH and REV change the playback time

 3) LENGTH sets total number of samples to play (total fwd and rev)
	  (keep track of # addr of play_buff[]->out are read when resampling, whether fwd or rev)
 			PITCH changes playback time, REV does not
		So at a constant PITCH, we can get a constant playback time even if we're toggling REV to get different effects

 4) ???
			PITCH does not change playback time, REV does??
 *
 */

#include "globals.h"
#include "audio_sdram.h"
#include "ff.h"
#include "sampler.h"
#include "audio_util.h"

#include "adc.h"
#include "params.h"
#include "timekeeper.h"
#include "compressor.h"
#include "dig_pins.h"
#include "rgb_leds.h"
#include "resample.h"

#include "wavefmt.h"
#include "circular_buffer.h"
#include "file_util.h"
#include "wav_recording.h"
#include "sts_filesystem.h"
#include "edit_mode.h"


//
// DEBUG
//
//volatile uint32_t tdebug[256];
//uint8_t t_i;
//uint32_t debug_i=0x80000000;
//uint32_t debug_tri_dir=0;
//debug:
//extern __IO uint16_t potadc_buffer[NUM_POT_ADCS];
//extern __IO uint16_t cvadc_buffer[NUM_CV_ADCS];
//extern int16_t i_smoothed_cvadc[NUM_CV_ADCS];
//extern int16_t i_smoothed_potadc[NUM_POT_ADCS];
//volatile uint32_t*  SDIOSTA;

extern uint32_t WATCH0;
extern uint32_t WATCH1;
extern uint32_t WATCH2;
extern uint32_t WATCH3;


//
// System-wide parameters, flags, modes, states
//
extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
//extern uint8_t 	settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

//extern float global_param[NUM_GLOBAL_PARAMS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern uint8_t 	flags[NUM_FLAGS];
extern enum g_Errors g_error;

extern uint8_t play_led_state[NUM_PLAY_CHAN];
//extern uint8_t clip_led_state[NUM_PLAY_CHAN];


uint8_t SAMPLINGBYTES=2;

uint32_t end_out_ctr[NUM_PLAY_CHAN]={0,0};
uint32_t play_led_flicker_ctr[NUM_PLAY_CHAN]={0,0};


//
// Memory
//
extern const uint32_t AUDIO_MEM_BASE[4];

//
// SDRAM buffer addresses for playing from sdcard
// SD Card:fil[]@sample_file_curpos --> SDARM @play_buff[]->in ... SDRAM @play_buff[]->out --> Codec
//
CircularBuffer splay_buff				[NUM_PLAY_CHAN];
CircularBuffer* play_buff				[NUM_PLAY_CHAN];

//make this a struct: play_state
uint8_t 		is_buffered_to_file_end	[NUM_PLAY_CHAN]; //is_buffered_to_end
enum PlayStates play_state				[NUM_PLAY_CHAN]; //activity
uint8_t			sample_num_now_playing	[NUM_PLAY_CHAN]; //sample_now_playing
uint8_t			sample_bank_now_playing	[NUM_PLAY_CHAN]; //bank_now_playing
uint32_t		cache_low				[NUM_PLAY_CHAN];
uint32_t		cache_high				[NUM_PLAY_CHAN];
uint32_t		cache_offset			[NUM_PLAY_CHAN];

//make this a struct
uint32_t 		sample_file_startpos	[NUM_PLAY_CHAN];
uint32_t		sample_file_endpos		[NUM_PLAY_CHAN];
uint32_t 		sample_file_curpos		[NUM_PLAY_CHAN];

uint32_t 		play_buff_bufferedamt	[NUM_PLAY_CHAN];
enum PlayLoadTriage play_load_triage;

//must have chan, banknum, and samplenum defined to use this macro!
#define goto_filepos(p)	f_lseek(&fil[chan], samples[banknum][samplenum].startOfData + (p));\
						if(fil[chan].fptr != samples[banknum][samplenum].startOfData + (p)) g_error|=LSEEK_FPTR_MISMATCH;

// static FRESULT goto_filepos(FIL *sfil, Sample *sample, uint32_t pos)
// {
// 	f_lseek(sfil, pos + sample->startOfData);

// }
// FRESULT goto_filepos(FIL *sfil, Sample *sample, uint32_t pos);


// #define DBG_SIZE 64
// uint32_t DBG_i=0;
// uint32_t DBG_buffi=0;
// uint32_t DBG_last_i = DBG_SIZE-1;
// uint32_t DBG_last_buffi = DBG_SIZE-1;
// uint32_t DBG_buffadd[DBG_SIZE];
// uint32_t DBG_sdadd[DBG_SIZE];
// uint32_t DBG_err[16];
// uint8_t DBG_err_i;

//
// Cross-fading:
//
uint32_t fade_queued_dest_read_addr[NUM_PLAY_CHAN];
uint32_t fade_dest_read_addr[NUM_PLAY_CHAN];
float read_fade_pos[NUM_PLAY_CHAN];

float decay_amp_i[NUM_PLAY_CHAN];
float decay_inc[NUM_PLAY_CHAN]={0,0};


//
// Filesystem
//
FIL fil[NUM_PLAY_CHAN];

//
// Sample info
//
extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];


void audio_buffer_init(void)
{
	uint32_t i;
	uint32_t bank;
	FRESULT res;


//	uint32_t sample_num, bank;
//	FIL temp_file;
//	FRESULT res;
//	DIR dir;
//	char path[10];
//	char tname[_MAX_LFN+1];
//	char path_tname[_MAX_LFN+1];


//	if (MODE_24BIT_JUMPER)
//		SAMPLINGBYTES=4;
//	else
		SAMPLINGBYTES=2;

	memory_clear(0);
	memory_clear(1);
	memory_clear(2);
	memory_clear(3);

	init_rec_buff();


	play_buff[0] = &(splay_buff[0]);
	play_buff[1] = &(splay_buff[1]);

	play_buff[0]->in 		= AUDIO_MEM_BASE[0];
	play_buff[0]->out		= AUDIO_MEM_BASE[0];
	play_buff[0]->min		= AUDIO_MEM_BASE[0];
	play_buff[0]->max		= AUDIO_MEM_BASE[0] + MEM_SIZE;
	play_buff[0]->size		= MEM_SIZE;
	play_buff[0]->wrapping	= 0;


	play_buff[1]->in 		= AUDIO_MEM_BASE[1];
	play_buff[1]->out		= AUDIO_MEM_BASE[1];
	play_buff[1]->min		= AUDIO_MEM_BASE[1];
	play_buff[1]->max		= AUDIO_MEM_BASE[1] + MEM_SIZE;
	play_buff[1]->size		= MEM_SIZE;
	play_buff[1]->wrapping	= 0;

	flags[PlayBuff1_Discontinuity] = 1;
	flags[PlayBuff2_Discontinuity] = 1;

	if (SAMPLINGBYTES==2){
		//min_vol = 10;
		init_compressor(1<<15, 0.75);
	}
	else{
		//min_vol = 10 << 16;
		init_compressor(1<<31, 0.75);
	}


	//Force loading of sample header on first play after boot
	sample_num_now_playing[0] = 0xFF;
	sample_num_now_playing[1] = 0xFF;
	sample_bank_now_playing[0] = 0xFF;
	sample_bank_now_playing[1] = 0xFF;


	//look for a sample.index file
	//if not found, or holding down both REV buttons, load all banks from disk
	//if found, read it into sample[]
	//  do some basic checks to verify integrity

	if (REV1BUT && REV2BUT)
		res = 1;
	else
		res = load_sampleindex_file();

	if (res == 0) //file was found
	{
		check_enabled_banks();
	}
	else
	{
		//load all the banks
		for (bank=0;bank<MAX_NUM_BANKS;bank++)
		{
			i = load_bank_from_disk(bank);

			if (i) enable_bank(bank);
			else disable_bank(bank);
		}

		res = write_sampleindex_file();
		if (res) {g_error |= CANNOT_WRITE_INDEX; check_errors();}
	}

	i = next_enabled_bank(2); //find the first enabled bank
	i_param[0][BANK] = i;
	i_param[1][BANK] = i;



}

//
void clear_is_buffered_to_file_end(uint8_t chan)
{
	is_buffered_to_file_end[chan] = 0;
	flags[ForceFileReload1+chan] = 1;
}

//
//Given: the starting address of the cache, and the address in the buffer to which it refers.
//Returns: a cache address equivalent to the buffer_point
//Assumes cache_start is the lowest value of the cache 
//
uint32_t map_buffer_to_cache(uint32_t buffer_point, uint32_t cache_start, uint32_t buffer_start, CircularBuffer *b)
{
	uint32_t p;

	//Find out how far ahead the buffer_point is from the buffer reference
	p = CB_distance_points(buffer_point, buffer_start, b->size, 0);

	//add that to the cache reference
	p += cache_start;

	return(p);
}

//
//Given: the starting address of the cache, and the address in the buffer to which it refers.
//Returns: a buffer address equivalent to cache_point
//Assumes cache_start is the lowest value of the cache (to overcome this, we would have to use int32_t or compare cache_point>cache_start)
//
uint32_t map_cache_to_buffer(uint32_t cache_point, uint32_t cache_start, uint32_t buffer_start, CircularBuffer *b)
{
	uint32_t p;
	//find the distance the cache_pos is ahead of the reference, and 
	p = buffer_start + (cache_point - cache_start);
	while (p > b->max)
		p -= b->size;

	return(p);
}

void toggle_reverse(uint8_t chan)
{
	uint8_t samplenum, banknum;
	uint32_t t;
	FRESULT res;
	enum PlayStates tplay_state;

	//DEBUG2_ON;

	if (play_state[chan] == PLAYING || play_state[chan]==PLAYING_PERC || play_state[chan] == PREBUFFERING || play_state[chan]==PLAY_FADEUP)
	{
		samplenum =sample_num_now_playing[chan];
		banknum = sample_bank_now_playing[chan];
	}
	else
	{
		samplenum = i_param[chan][SAMPLE];
		banknum = i_param[chan][BANK];
	}

	tplay_state = play_state[chan];
	play_state[chan] = SILENT;

	//If we are PREBUFFERING or PLAY_FADEUP, then that means we just started playing. 
	//It could be the case a common trigger fired into PLAY and REV, but the PLAY trig was detected first
	//So we actually want to make it play from the end of the sample rather than reverse direction from the current spot
	//if (play_state[chan] == PREBUFFERING || play_state[chan]==PLAY_FADEUP)

	//Check the cache position of play_buff[chan]->out, to see if it's a certain distance from sample_file_startpos[chan]
	//If so, reverse direction (by keeping ->out the same)
	//If not, play from the other end (by moving ->out to the opposite end) 
	t = map_buffer_to_cache(play_buff[chan]->out, cache_low[chan], cache_offset[chan], play_buff[chan]);

	if (!i_param[chan][REV]){ //currently going forward, so find distance ->out is ahead of startpos
		if (t > sample_file_startpos[chan]) t = t - sample_file_startpos[chan]; else t=0;
	}else{
		if (t < sample_file_startpos[chan]) t = sample_file_startpos[chan] - t; else t=0;
	}

	if (t <= READ_BLOCK_SIZE * 2)
	{
		if (sample_file_endpos[chan] <= cache_high[chan])
			play_buff[chan]->out = map_cache_to_buffer(sample_file_endpos[chan], cache_low[chan], cache_offset[chan], play_buff[chan]);
		else
		{
			DEBUG2_ON;
			if (i_param[chan][REV])	i_param[chan][REV] = 0;
			else 					i_param[chan][REV] = 1;

			start_playing(chan);
			return;
		}
	}

	// Swap sample_file_curpos with cache_high or _low
	// and move ->in to the equivlant address in play_buff
	// This gets us ready to read new data to the opposite end of the cache.

	if (i_param[chan][REV])
	{
		i_param[chan][REV] = 0;
		sample_file_curpos[chan] = cache_high[chan];
		play_buff[chan]->in = map_cache_to_buffer(cache_high[chan], cache_low[chan], cache_offset[chan], play_buff[chan]);
	}
	else
	{
		i_param[chan][REV] = 1;
		sample_file_curpos[chan] = cache_low[chan];
		play_buff[chan]->in = cache_offset[chan]; //cache_offset is the map of cache_low
	}



	//swap the endpos with the startpos
	t							= sample_file_endpos[chan];
	sample_file_endpos[chan]	= sample_file_startpos[chan];
	sample_file_startpos[chan]	= t;

	res = goto_filepos(sample_file_curpos[chan]); //uses samplenum and banknum to find startOfData

	if (res!=FR_OK)
		g_error |= FILE_SEEK_FAIL;

	 play_state[chan] = tplay_state;

	DEBUG2_OFF;
}




//
// start_playing()
// Starts playing on a channel at the current param positions
//

#define SZ_TBL 32
DWORD chan_clmt[NUM_PLAY_CHAN][SZ_TBL];

void start_playing(uint8_t chan)
{
	uint8_t samplenum, banknum;
	FRESULT res;

	DEBUG1_ON;

	samplenum = i_param[chan][SAMPLE];
	banknum = i_param[chan][BANK];

	if (samples[banknum][samplenum].filename[0] == 0)
		return;

	flags[PlayBuff1_Discontinuity+chan] = 1;

	check_change_bank(chan);

	//Check if sample/bank changed, or file is flagged for reload
	//if so, close the current file and open the new sample file
	if (flags[ForceFileReload1+chan] || (sample_num_now_playing[chan] != samplenum) || (sample_bank_now_playing[chan] != banknum) || fil[chan].obj.fs==0)
	{
		flags[ForceFileReload1+chan] = 0;

		f_close(&fil[chan]);

		 // 384us up to 1.86ms or more? at -Ofast
		res = f_open(&fil[chan], samples[banknum][samplenum].filename, FA_READ);
		f_sync(&fil[chan]);

		if (res != FR_OK)
		{
			g_error |= FILE_OPEN_FAIL;
			sample_num_now_playing[chan] = 0xFF;
			sample_bank_now_playing[chan] = 0xFF;

			f_close(&fil[chan]);
			return;
		}

		//takes over 3.3ms for a big file
		//Could save time by pre-loading all the clmt when we load the bank
		//Then just do fil[chan].cltbl = preloaded_clmt[samplenum];
		//instead of clmt[0]=SZ_TBL and f_lseek(...CREATE_LINKMAP)
		fil[chan].cltbl = chan_clmt[chan];
		chan_clmt[chan][0] = SZ_TBL;
		res = f_lseek(&fil[chan], CREATE_LINKMAP);

		if (res != FR_OK)
		{
			g_error |= FILE_CANNOT_CREATE_CLTBL;
			sample_num_now_playing[chan] = 0xFF;
			sample_bank_now_playing[chan] = 0xFF;

			f_close(&fil[chan]);
			return;
		}

		//Check the file is really as long as the sampleSize says it is
		if (f_size(&fil[chan]) < (samples[banknum][samplenum].startOfData + samples[banknum][samplenum].sampleSize))
		{
			samples[banknum][samplenum].sampleSize = f_size(&fil[chan]) - samples[banknum][samplenum].startOfData;

			if (samples[banknum][samplenum].inst_end > samples[banknum][samplenum].sampleSize)
				samples[banknum][samplenum].inst_end = samples[banknum][samplenum].sampleSize;

			if ((samples[banknum][samplenum].inst_start + samples[banknum][samplenum].inst_size) > samples[banknum][samplenum].sampleSize)
				samples[banknum][samplenum].inst_size = samples[banknum][samplenum].sampleSize - samples[banknum][samplenum].inst_start;
		}

		sample_num_now_playing[chan] = samplenum;
		sample_bank_now_playing[chan] = i_param[chan][BANK];

		cache_low[chan] = 0;
		cache_high[chan] = 0;
		cache_offset[chan] = play_buff[chan]->min;
WATCH0=cache_low[0]; WATCH1=cache_high[0];
	}

	//Determine starting and ending addresses
	if (i_param[chan][REV])
	{
		sample_file_endpos[chan] = calc_start_point(f_param[chan][START], &samples[banknum][samplenum]);
		sample_file_startpos[chan] = calc_stop_points(f_param[chan][LENGTH], &samples[banknum][samplenum], sample_file_endpos[chan]);
	}
	else
	{
		sample_file_startpos[chan] = calc_start_point(f_param[chan][START], &samples[banknum][samplenum]);
		sample_file_endpos[chan] = calc_stop_points(f_param[chan][LENGTH], &samples[banknum][samplenum], sample_file_startpos[chan]);
	}

	// Set up parameters to begin playing

	//see if starting position is already cached
	if ((cache_high[chan]>cache_low[chan]) && (cache_low[chan] <= sample_file_startpos[chan]) && (sample_file_startpos[chan] <= cache_high[chan]))
	{
		play_buff[chan]->out = map_cache_to_buffer(sample_file_startpos[chan], cache_low[chan], cache_offset[chan], play_buff[chan]);

		if (play_buff[chan]->out < play_buff[chan]->in)	play_buff[chan]->wrapping = 0;
		else											play_buff[chan]->wrapping = 1;

		if (f_param[chan][LENGTH] < 0.5 && i_param[chan][REV])	play_state[chan] = PLAYING_PERC;
		else													play_state[chan] = PLAY_FADEUP;


	}
	else //...otherwise, start buffering from scratch
	{
		CB_init(play_buff[chan], i_param[chan][REV]);

		res = goto_filepos(sample_file_startpos[chan]);
		if (res != FR_OK) g_error |= FILE_SEEK_FAIL;

		if (g_error & LSEEK_FPTR_MISMATCH)
		{
			sample_file_startpos[chan] = f_tell(&fil[chan]) - samples[banknum][samplenum].startOfData;
		}

		sample_file_curpos[chan] 		= sample_file_startpos[chan];

		cache_low[chan] 				= sample_file_startpos[chan];
		cache_high[chan] 				= sample_file_startpos[chan];
		cache_offset[chan] 				= play_buff[chan]->min;

		is_buffered_to_file_end[chan] = 0;

		play_state[chan]=PREBUFFERING;
 WATCH0=cache_low[0]; WATCH1=cache_high[0];
	}

	if (i_param[chan][REV])	decay_amp_i[chan]=0.0;
	else					decay_amp_i[chan]=1.0;
	decay_inc[chan]=0.0;

	play_led_state[chan]=1;

	if (global_mode[MONITOR_RECORDING])	global_mode[MONITOR_RECORDING] = 0;

	DEBUG1_OFF;
}


//
// Determines whether to start/restart/stop playing
//
void toggle_playing(uint8_t chan)
{

	//Start playing
	if (play_state[chan]==SILENT || play_state[chan]==PLAY_FADEDOWN || play_state[chan]==RETRIG_FADEDOWN || play_state[chan]==PREBUFFERING || play_state[chan]==PLAY_FADEUP)
	{	//previously, PREBUFFERING and PLAY_FADEUP were ignored??

		start_playing(chan);
	}

	//Re-start if we have a short length
	else if (play_state[chan]==PLAYING_PERC || (play_state[chan]==PLAYING && f_param[chan][LENGTH] <= 0.98))
	{
		play_state[chan]=RETRIG_FADEDOWN;
		play_led_state[chan]=0;

	}

	//Stop it if we're playing a full sample
	else if (play_state[chan]==PLAYING/* && f_param[chan][LENGTH] > 0.98*/)
	{
		play_state[chan]=PLAY_FADEDOWN;
		play_led_state[chan]=0;
	}

}



uint32_t calc_start_point(float start_param, Sample *sample)
{
	uint32_t align;
	uint32_t zeropt;
	uint32_t inst_size;

	if (sample->blockAlign == 4)		align = 0xFFFFFFFC;
	else if (sample->blockAlign == 2)	align = 0xFFFFFFF8; //was E? but it clicks if we align to 2 not 4, even if our file claims blockAlign = 2
	else if (sample->blockAlign == 8)	align = 0xFFFFFFF8;

	zeropt = sample->inst_start;
	inst_size = sample->inst_end - sample->inst_start;

	//Determine the starting address
	if (start_param < 0.002)			return(align & zeropt);
	else if (start_param > 0.998)		return(align & (zeropt + inst_size - (READ_BLOCK_SIZE*2)) ); //just play the last 32 blocks (~64k samples)

	else								return(align & (zeropt  +  ( (uint32_t)(start_param * (float)inst_size) )) );


}


uint32_t calc_stop_points(float length, Sample *sample, uint32_t startpos)
{
	uint32_t dist, fwd_stop_point;

	dist = calc_play_length(length, sample);

	fwd_stop_point = startpos + dist;
	if (fwd_stop_point > sample->inst_end)
		fwd_stop_point = sample->inst_end;

	if (sample->blockAlign == 4)		fwd_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 2)	fwd_stop_point &= 0xFFFFFFF8;
	else if (sample->blockAlign == 8)	fwd_stop_point &= 0xFFFFFFF8;

	return(fwd_stop_point);

}

uint32_t calc_play_length(float length, Sample *sample)
{
	uint32_t dist;
	uint32_t inst_size;

	inst_size = sample->inst_end - sample->inst_start; //as opposed to taking sample->inst_size because that won't be clipped to the end of a sample file

	if (length > 0.98)
	 	dist = inst_size * (length-0.9) * 10; //80% to 100%

	else 
	if (length>=0.5) //>=0.5 to <0.95
	{
		if (inst_size > (5 * sample->sampleRate * sample->blockAlign) ) //sample is > 5 seconds long
			dist = (length - (0.5-0.25/8.0)) * 8.0 * sample->sampleRate  * sample->blockAlign; //0.25 .. 4.25 seconds
		else
			dist = (length - (0.5-0.25/8.0)) * 1.6 * inst_size; //sampletime/5.0 .. sampletime seconds
	}

	else //length < 0.5
	{
		dist = sample->sampleRate * sample->blockAlign * length; //very small to 0.5 seconds
	}

	return(dist);

}


void check_change_bank(uint8_t chan)
{
	if (flags[PlayBank1Changed + chan*2])
	{
		flags[PlayBank1Changed + chan*2]=0;

		//load_bank_from_disk(i_param[chan][BANK], chan);

		is_buffered_to_file_end[chan] = 0;

		flags[PlaySample1Changed + chan*2]=1;

	}

}

void check_change_sample(void)
{

	if (flags[PlaySample1Changed])
	{
		flags[PlaySample1Changed]=0;
		//can_restart[0] = 0;
		//make cache_* = 0

		if (samples[ i_param[0][BANK] ][ i_param[0][SAMPLE] ].filename[0] == 0) //no file: fadedown or remain silent
		{
			if (play_state[0] != SILENT && play_state[0]!=PREBUFFERING)
				play_state[0] = PLAY_FADEDOWN;
			else
				play_state[0] = SILENT;

			if (global_mode[EDIT_MODE])
				flags[AssigningEmptySample] = 1;

		}
		else
		{
			if (play_state[0] == SILENT && i_param[0][LOOPING])
				flags[Play1But]=1;

			if (play_state[0] != SILENT && play_state[0]!=PREBUFFERING)
				play_state[0] = PLAY_FADEDOWN;

			if (global_mode[EDIT_MODE])
			{
				find_current_sample_in_assign(&samples[ i_param[0][BANK] ][ i_param[0][SAMPLE] ]);
				flags[AssigningEmptySample] = 0;
			}
		}


	}

	if (flags[PlaySample2Changed]){
		flags[PlaySample2Changed]=0;
		//can_restart[1] = 0;

		if (samples[ i_param[1][BANK] ][ i_param[1][SAMPLE] ].filename[0] == 0 )
		{
			if (play_state[1] != SILENT && play_state[1]!=PREBUFFERING)
				play_state[1] = PLAY_FADEDOWN;
			else
				play_state[1] = SILENT;
		}
		else
		{
			if (play_state[1] == SILENT && i_param[1][LOOPING])
				flags[Play2But]=1;

			if (play_state[1] != SILENT && play_state[1]!=PREBUFFERING)
				play_state[1] = PLAY_FADEDOWN;

		}
	}
}

// void check_trim_bounds(void)
// {
// 	uint8_t samplenum, banknum;

// 	samplenum = sample_num_now_playing[0];
// 	banknum = sample_bank_now_playing[0];

// 	if (sample_file_curpos[0] > samples[banknum][samplenum].inst_end)
// 	{
// 		if (i_param[0][LOOPING])
// 			flags[Play1But]=1;
// 		else
// 			is_buffered_to_file_end[0] = 1;

// 		//sample_file_curpos[chan] = samples[banknum][samplenum].inst_end;
// 		//goto_filepos(sample_file_curpos[chan]);
// 	}
	


// }

int16_t tmp_buff_i16[READ_BLOCK_SIZE>>1]; 

void read_storage_to_buffer(void)
{
	uint8_t chan=0;
	uint32_t err;

	FRESULT res;
	uint32_t br;
	uint32_t rd;
	//uint16_t i;
	//int32_t a,b,c;

	//convenience variables
	uint8_t samplenum, banknum;
	FSIZE_t t_fptr;
	uint32_t pre_buff_size;
	uint32_t active_buff_size;

	check_change_sample();



	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{
		if (play_state[chan] == SILENT)
		{
			check_change_bank(chan);
		}

		if (play_state[chan] != SILENT && play_state[chan] != PLAY_FADEDOWN && play_state[chan] != RETRIG_FADEDOWN)
		{
			samplenum = sample_num_now_playing[chan];
			banknum = sample_bank_now_playing[chan];

			//We should buffer more if:
			//     play_buff[]->out is approaching play_buff[]->in (buffer_lead)
			//     OR the circular buffer is not full (cache_left > READ_BLOCK_SIZE)
			// Unless the entire file has been read from startpos to endpos (is_buffered_to_file_end)
			//don't read more if it will make play_buff[chan]->in wrap past the cache_offset[chan].... unless we have to because buffer_lead < PRE_BUFF_SIZE

			play_buff_bufferedamt[chan] = CB_distance(play_buff[chan], i_param[chan][REV]);

			//cache_left = play_buff[chan]->size - (cache_high[chan] - cache_low[chan]);
			//if (cache_left>play_buff[chan]->size) cache_left = 0;

			if ( !(g_error & (FILE_READ_FAIL_1 << chan)))
			{
				if ( (!i_param[chan][REV] && (sample_file_curpos[chan] < samples[banknum][samplenum].inst_end))
				 	|| (i_param[chan][REV] && (sample_file_curpos[chan] > samples[banknum][samplenum].inst_start)) )
					is_buffered_to_file_end[chan] = 0;
			}

			pre_buff_size = PRE_BUFF_SIZE;// * (f_param[chan][PITCH]);
			active_buff_size = ACTIVE_BUFF_SIZE;// * (f_param[chan][PITCH]);

			if (!is_buffered_to_file_end[chan] && 
				(
					(play_state[chan]==PREBUFFERING && (play_buff_bufferedamt[chan] < pre_buff_size)) ||
					(play_state[chan]!=PREBUFFERING && (play_buff_bufferedamt[chan] < active_buff_size))
				)) //FixMe: should be PRE_BUFF_SIZE * blockAlign
			{

				if (sample_file_curpos[chan] > samples[banknum][ samplenum ].sampleSize) //we read too much data somehow //When does this happen? sample_file_curpos has not changed recently...
					g_error |= FILE_WAVEFORMATERR;							//Breakpoint


				else if (sample_file_curpos[chan] > samples[banknum][samplenum].inst_end)
				{
					//if (i_param[chan][LOOPING])
					//	flags[Play1But]=1;
					//else
						is_buffered_to_file_end[chan] = 1;

					//sample_file_curpos[chan] = samples[banknum][samplenum].inst_end;
					//goto_filepos(sample_file_curpos[chan]);
				}

				else
				{
					//
					// Forward reading:
					//
					if (i_param[chan][REV]==0)
					{
						rd = samples[banknum][ samplenum ].inst_end -  sample_file_curpos[chan];

						if (rd > READ_BLOCK_SIZE) rd = READ_BLOCK_SIZE;

					//	DEBUG1_ON;
						res = f_read(&fil[chan], (uint8_t *)tmp_buff_i16, rd, &br);
						if (res != FR_OK) 
						{
							g_error |= FILE_READ_FAIL_1 << chan; 
							is_buffered_to_file_end[chan] = 1;
						}
					//	f_sync(&fil[chan]);
					//	DEBUG1_OFF;

						// DEBUG0_OFF;	
						// for(i=2;i<(rd>>1);i++)
						// {
						// 	a = tmp_buff_i16[i];
						// 	b = tmp_buff_i16[i-2];

						// 	if (a > b)
						// 		c = a - b;
						// 	else
						// 		c = b - a;
						// 	if (c > 0x2000)
						// 		DEBUG0_ON;
							
						// }

						if (br < rd)
						{
							g_error |= FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read
							is_buffered_to_file_end[chan] = 1;
						}

						//sample_file_curpos[chan] += br;
						sample_file_curpos[chan] = f_tell(&fil[chan]) - samples[banknum][ samplenum ].startOfData;

						if (sample_file_curpos[chan] >= samples[banknum][ samplenum ].inst_end)
						{
							//sample_file_curpos[chan] = samples[banknum][ samplenum ].inst_end;
							//Breakpoint
							is_buffered_to_file_end[chan] = 1;
						}

					}

					//
					// Reverse reading:
					//
					else
					{
						if (sample_file_curpos[chan] > samples[banknum][samplenum].inst_start)
							rd = sample_file_curpos[chan] - samples[banknum][samplenum].inst_start;
						else
							rd = 0;

						if (rd >= READ_BLOCK_SIZE)
						{

							//Jump back a block
							rd = READ_BLOCK_SIZE;

							t_fptr=f_tell(&fil[chan]);
							res = f_lseek(&fil[chan], t_fptr - READ_BLOCK_SIZE);
							if (res || (f_tell(&fil[chan])!=(t_fptr - READ_BLOCK_SIZE)))
								g_error |= LSEEK_FPTR_MISMATCH;
							
							sample_file_curpos[chan] = f_tell(&fil[chan]) - samples[banknum][ samplenum ].startOfData;

						}
						else
						{
							//Jump to the beginning
							sample_file_curpos[chan] = samples[banknum][samplenum].inst_start;
							res = goto_filepos(sample_file_curpos[chan]);
							if (res!=FR_OK)
								g_error |= FILE_SEEK_FAIL;

							//f_lseek(&fil[chan], samples[banknum][samplenum].startOfData + samples[banknum][samplenum].inst_start);
							is_buffered_to_file_end[chan] = 1;
						}


					//	DEBUG1_ON;
						//Read one block forward
						t_fptr=f_tell(&fil[chan]);
						res = f_read(&fil[chan], tmp_buff_i16, rd, &br);
						if (res != FR_OK) 
							g_error |= FILE_READ_FAIL_1 << chan;
					//	DEBUG1_OFF;

						if (br < rd)		g_error |= FILE_UNEXPECTEDEOF;
						//Jump backwards to where we started reading
						//fil[chan].fptr = t_fptr;
						//res = f_lseek(&fil[chan], f_tell(&fil[chan]) - br);
						res = f_lseek(&fil[chan], t_fptr);
						if (res != FR_OK)	g_error |= FILE_SEEK_FAIL;
						if (f_tell(&fil[chan])!=t_fptr)
											g_error |= LSEEK_FPTR_MISMATCH;

					}


					//Write temporary buffer to play_buff[]->in
					if (res != FR_OK) 		g_error |= FILE_READ_FAIL_1 << chan;
					else
					{
	

						//Todo: avoid using a tbuff16 and copying it, rewrite f_read so that it writes directly to play_buff[] in SDRAM
						err = memory_write16_cb(play_buff[chan], tmp_buff_i16, rd>>1, 0);

						//Update the cache addresses
						if (i_param[chan][REV])
						{
							//Ignore head crossing error if we are reversing and ended up with in==out (that's normal for the first reading)
							if (err && (play_buff[chan]->in == play_buff[chan]->out)) 
								err = 0;

							//Jump back two blocks if we're reversing
							CB_offset_in_address(play_buff[chan], READ_BLOCK_SIZE*2, 1);

							if (cache_high[chan] - cache_low[chan] >= play_buff[chan]->size)
							{
								
								if (cache_high[chan] > (cache_low[chan] - sample_file_curpos[chan]))
									cache_high[chan] -= (cache_low[chan] - sample_file_curpos[chan]); //should be equivlant to br (bytes read)
								else
									cache_high[chan] = 0;
							}
							cache_offset[chan] = play_buff[chan]->in;
							cache_low[chan] = sample_file_curpos[chan]; 

						} 
						else {

							//if (play_buff[chan]->filled)
							if (cache_high[chan] - cache_low[chan] >= play_buff[chan]->size)
							{
								cache_offset[chan] = play_buff[chan]->in;
								cache_low[chan] += (sample_file_curpos[chan] - cache_high[chan]); //should be equivlant to br (bytes read)
							}
							cache_high[chan] = sample_file_curpos[chan];
						}

WATCH0=cache_low[0]; WATCH1=cache_high[0];
						if (err)
						{
//							DEBUG3_ON;
							g_error |= READ_BUFF1_OVERRUN*(1+chan);
						}
//						 else
//							DEBUG3_OFF;

					}

				}

			}

			//Check if we've prebuffered enough to start playing
			if ((is_buffered_to_file_end[chan] || play_buff_bufferedamt[chan] >= PRE_BUFF_SIZE) && play_state[chan] == PREBUFFERING)
			{

				if (f_param[chan][LENGTH] < 0.5 && i_param[chan][REV])
					play_state[chan] = PLAYING_PERC;
				else
					play_state[chan] = PLAY_FADEUP;

			}

		} //play_state != SILENT, FADEDOWN
	} //for (chan)

}

void play_audio_from_buffer(int32_t *outL, int32_t *outR, uint8_t chan)
{
	uint16_t i;
	int32_t t_i32;
	float t_f;
	uint32_t t_u32;

	//Resampling:
	static float resampling_fpos[NUM_PLAY_CHAN*2]={0.0f, 0.0f, 0.0f, 0.0f};
	float rs;
	uint32_t resampled_buffer_size;

	uint32_t sample_file_playpos;
	float gain;

	//convenience variables
	float length;
	uint8_t samplenum, banknum;


	samplenum = sample_num_now_playing[chan];
	banknum = sample_bank_now_playing[chan];

	// Fill buffer with silence
	if (play_state[chan] == PREBUFFERING || play_state[chan] == SILENT)
	{
		 for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
		 {
			 outL[i]=0;
			 outR[i]=0;
		}
	}
	else
	{
		//Read from SDRAM into out[]

		//Re-sampling rate (0.0 < rs <= 4.0)
		rs = f_param[chan][PITCH];

	DEBUG3_ON;
		//Resample data read from the play_buff and store into out[]
		if (samples[banknum][samplenum].numChannels == 2)
		{
			t_u32 = play_buff[chan]->out;
			resample_read16(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, STEREO_LEFT, samples[banknum][samplenum].blockAlign, chan, resampling_fpos, outL);

			play_buff[chan]->out = t_u32;
			resample_read16(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, STEREO_RIGHT, samples[banknum][samplenum].blockAlign, chan, resampling_fpos, outR);
		}
		else
		{
			resample_read16(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, STEREO_LEFT, samples[banknum][samplenum].blockAlign, chan, resampling_fpos, outL);
			for (i=0;i<HT16_CHAN_BUFF_LEN;i++) outR[i] = outL[i];
		}

	DEBUG3_OFF;

		//Calculate length and where to stop playing
		length = f_param[chan][LENGTH];

		//Update the endpos (we should do this in params, no?)
		if (i_param[chan][REV])
			sample_file_startpos[chan] = calc_stop_points(length, &samples[banknum][samplenum], sample_file_endpos[chan]);
		else
			sample_file_endpos[chan] = calc_stop_points(length, &samples[banknum][samplenum], sample_file_startpos[chan]);
	

		gain = samples[banknum][samplenum].inst_gain;

		 switch (play_state[chan])
		 {

			 case (PLAY_FADEDOWN):
			 case (RETRIG_FADEDOWN):

				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					t_f = (float)(HT16_CHAN_BUFF_LEN-i) / (float)HT16_CHAN_BUFF_LEN;

					outL[i] = (float)outL[i] * t_f;
					t_i32 = (float)outL[i] * gain;
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					outL[i] = t_i32;

					outR[i] = (float)outR[i] * t_f;
					t_i32 = (float)outR[i] * gain;
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					outR[i] = t_i32;
				}

				play_led_state[chan] = 0;

				end_out_ctr[chan] = (length>0.05)? 35 : ((length * 540) + 8);
				play_led_flicker_ctr[chan]=(length>0.3)? 75 : ((length * 216)+10);


				if (play_state[chan]==RETRIG_FADEDOWN || i_param[chan][LOOPING])
					flags[Play1But+chan] = 1;

				//else
				play_state[chan] = SILENT;

				break;

			 case (PLAYING):
			 case (PLAY_FADEUP):
			 		if (play_state[chan] == PLAY_FADEUP) 
			 		{
						for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
						{
							t_f = gain * (float)i / (float)HT16_CHAN_BUFF_LEN;

							t_i32 = (float)outL[i] * t_f;
							asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
							outL[i] = t_i32;

							t_i32 = (float)outR[i] * t_f;
							asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
							outR[i] = t_i32;
						}
					} 
					else
					{
						for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
						{
							t_i32 = (float)outL[i] * gain;
							asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
							outL[i] = t_i32;

							t_i32 = (float)outR[i] * gain;
							asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
							outR[i] = t_i32;
						}


					}


					if (length>0.5) //Play a longer portion of the sample, and go to PLAY_FADEDOWN when done
					{
	 					play_state[chan]=PLAYING;

						resampled_buffer_size = (uint32_t)((HT16_CHAN_BUFF_LEN * samples[banknum][samplenum].blockAlign) * rs);
						resampled_buffer_size *= 2;


						//Stop playback if we've played enough samples

						//Find out how far ahead our output data is from the start of the cache
						sample_file_playpos = CB_distance_points(play_buff[chan]->out, cache_offset[chan], play_buff[chan]->size, 0);
						//add that to the start of the cache file address, to get the address in the file that we're currently outputting
						sample_file_playpos += cache_low[chan];

						//See if we are about to surpass the calculated position in the file where we should end our sample
						//Even if we reversed while playing, the _endpos should be correct
						if (!i_param[chan][REV])
						{
							if ((sample_file_playpos + resampled_buffer_size) >= sample_file_endpos[chan])
								play_state[chan] = PLAY_FADEDOWN;
						} else {
							if (sample_file_playpos <= (resampled_buffer_size*2 + sample_file_endpos[chan]))//  ((sample_file_playpos - resampled_buffer_size) <= sample_file_endpos[chan]))
								play_state[chan] = PLAY_FADEDOWN;
						}

						if (play_state[chan] != PLAY_FADEDOWN)
						{
							//Check if we are about to hit the end of the file (or buffer undderrun)
							play_buff_bufferedamt[chan] = CB_distance(play_buff[chan], i_param[chan][REV]);

							if (play_buff_bufferedamt[chan] <= resampled_buffer_size)
							{

								if (is_buffered_to_file_end[chan])
									play_state[chan] = PLAY_FADEDOWN; //hit end of file

								else//buffer underrun: tried to read too much out. Try to recover!
								{
									g_error |= READ_BUFF1_OVERRUN<<chan;
									check_errors();
//									DEBUG2_ON;

									//play_state[chan] = PREBUFFERING;
									//is_buffered_to_file_end[chan] = 0;

									play_buff[chan]->out = play_buff[chan]->in;

									//if (i_param[chan][REV]) resampled_buffer_size *= 2;

									CB_offset_out_address(play_buff[chan], resampled_buffer_size, !i_param[chan][REV]);
									play_buff[chan]->out &= 0xFFFFFFF8;

								}
							}
//							else
//								DEBUG2_OFF;

						}

					}
					else
						play_state[chan] = PLAYING_PERC;
			 	 	break;

			 case (PLAYING_PERC):
		//		decay_inc[chan] += 1.0f/((length)*2621440.0f);
				decay_inc[chan] = 1.0f/((length)*20000.0f);

				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					if (i_param[chan][REV])	decay_amp_i[chan] += decay_inc[chan];
					else					decay_amp_i[chan] -= decay_inc[chan];

					if (decay_amp_i[chan] < 0.0f) 		decay_amp_i[chan] = 0.0f;
					else if (decay_amp_i[chan] > 1.0f) 	decay_amp_i[chan] = 1.0f;

					if (length>0.02){ //fixme: not efficient to have an if inside the for-loop! All we want to do is use decay_amp_i as a duration counter, but not apply an envelope if we have a very short length
						outL[i] = ((float)outL[i]) * decay_amp_i[chan];
						outR[i] = ((float)outR[i]) * decay_amp_i[chan];
					}

					t_i32 = (float)outL[i] * gain;
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					outL[i] = t_i32;

					t_i32 = (float)outR[i] * gain;
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					outR[i] = t_i32;
				}

				if (decay_amp_i[chan] <= 0.0f || decay_amp_i[chan] >= 1.0f)
				{
					decay_amp_i[chan] = 0.0f;

					end_out_ctr[chan] = (length>0.05)? 35 : ((length * 540) + 8);
					play_led_flicker_ctr[chan]=(length>0.3)? 75 : ((length * 216)+10);

					play_led_state[chan] = 0;

					if (i_param[chan][REV])
						play_state[chan] = PLAY_FADEDOWN;

					else
					{
						if (/*play_state[chan]==RETRIG_FADEDOWN ||*/ i_param[chan][LOOPING])
							flags[Play1But+chan] = 1;

						play_state[chan] = SILENT;
					}
				}
				break;

			 case (SILENT):
					play_led_state[chan] = 0;
				break;

			 default:
				 break;

		 }//switch play_state

	} //if play_state

	if (	(play_state[0]==PLAYING && (play_buff_bufferedamt[0] < 15000)) ||
			(play_state[1]==PLAYING && (play_buff_bufferedamt[1] < 15000)) )

		play_load_triage = PRIORITIZE_PLAYING;
	else
		play_load_triage = NO_PRIORITY;

}




void SDIO_read_IRQHandler(void)
{
	if (TIM_GetITStatus(SDIO_read_TIM, TIM_IT_Update) != RESET) {

		//Set the flag to tell the main loop to read more from the sdcard
		//We don't want to read_storage_to_buffer() here inside the interrupt because we might be interrupting a current read/write process
		//Keeping sdcard read/writes out of interrupts forces us to complete each read/write before beginning another

		flags[TimeToReadStorage] = 1;
		//read_storage_to_buffer();

		TIM_ClearITPendingBit(SDIO_read_TIM, TIM_IT_Update);
	}
}
