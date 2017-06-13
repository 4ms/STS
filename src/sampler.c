/*
 * sampler.c

//Is reversing while PREBUFFERING working? Line 341
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
#include "sample_file.h"
#include "circular_buffer_cache.h"
#include "bank.h"

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

extern uint32_t WATCH0;
extern uint32_t WATCH1;
extern uint32_t WATCH2;
extern uint32_t WATCH3;

extern FATFS FatFs;

//
// System-wide parameters, flags, modes, states
//
extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern uint8_t 	flags[NUM_FLAGS];
extern enum g_Errors g_error;

extern uint8_t play_led_state[NUM_PLAY_CHAN];
//extern uint8_t clip_led_state[NUM_PLAY_CHAN];


uint8_t SAMPLINGBYTES=2;

uint32_t end_out_ctr[NUM_PLAY_CHAN]={0,0};
uint32_t play_led_flicker_ctr[NUM_PLAY_CHAN]={0,0};

Sample dbg_sample;

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

uint32_t		cache_low				[NUM_PLAY_CHAN]; //file position address that corresponds to lowest position in file that's cached
uint32_t		cache_high				[NUM_PLAY_CHAN]; //file position address that corresponds to highest position in file that's cached
uint32_t		cache_size				[NUM_PLAY_CHAN]; //size in bytes of the cache (should always equal play_buff[]->size / 2 * sampleByteSize
uint32_t		cache_map_pt			[NUM_PLAY_CHAN]; //address in play_buff[] that corresponds to cache_low

//make this a struct
uint32_t 		sample_file_startpos	[NUM_PLAY_CHAN];
uint32_t		sample_file_endpos		[NUM_PLAY_CHAN];
uint32_t 		sample_file_curpos		[NUM_PLAY_CHAN];

uint32_t 		play_buff_bufferedamt	[NUM_PLAY_CHAN];
enum PlayLoadTriage play_load_triage;

//must have chan, banknum, and samplenum defined to use this macro!
#define goto_filepos(p)	f_lseek(&fil[chan], samples[banknum][samplenum].startOfData + (p));\
						if(fil[chan].fptr != samples[banknum][samplenum].startOfData + (p)) g_error|=LSEEK_FPTR_MISMATCH;

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
	uint32_t bank;
	uint8_t force_reload;


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

	// if (SAMPLINGBYTES==2){
	// 	//min_vol = 10;
	// 	init_compressor(1<<15, 0.75);
	// }
	// else{
	// 	//min_vol = 10 << 16;
	// 	init_compressor(1<<31, 0.75);
	// }

	//Force reloading of banks from disk with button press on boot
	if (REV1BUT && REV2BUT) 
		force_reload = 1;
	else
		force_reload=0;

	load_all_banks(force_reload);

	bank = next_enabled_bank(0xFF); //Find the first enabled bank
	i_param[0][BANK] = bank;
	i_param[1][BANK] = bank;

	sample_num_now_playing[0] = 0;
	sample_num_now_playing[1] = 0;
	sample_bank_now_playing[0] = i_param[0][BANK];
	sample_bank_now_playing[1] = i_param[1][BANK];

}

//
// void clear_is_buffered_to_file_end(uint8_t chan)
// {
// 	is_buffered_to_file_end[chan] = 0;
// 	flags[ForceFileReload1+chan] = 1;
// }



void toggle_reverse(uint8_t chan)
{
	uint8_t samplenum, banknum;
	uint32_t t;
	FRESULT res;
	enum PlayStates tplay_state;


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
	if (play_state[chan] == PREBUFFERING || play_state[chan]==PLAY_FADEUP)
	{

		//Check the cache position of play_buff[chan]->out, to see if it's a certain distance from sample_file_startpos[chan]
		//If so, reverse direction (by keeping ->out the same)
		//If not, play from the other end (by moving ->out to the opposite end) 
		t = map_buffer_to_cache(play_buff[chan]->out, samples[banknum][samplenum].sampleByteSize, cache_low[chan], cache_map_pt[chan], play_buff[chan]);

		if (!i_param[chan][REV]){ //currently going forward, so find distance ->out is ahead of startpos
			if (t > sample_file_startpos[chan])
				t = t - sample_file_startpos[chan];
			else t=0;
		}else{
			if (t < sample_file_startpos[chan])
				t = sample_file_startpos[chan] - t;
			else t=0;
		}

		if (t <= READ_BLOCK_SIZE * 2)
		{
			if (sample_file_endpos[chan] <= cache_high[chan])
				play_buff[chan]->out = map_cache_to_buffer(sample_file_endpos[chan], samples[banknum][samplenum].sampleByteSize, cache_low[chan], cache_map_pt[chan], play_buff[chan]);
			else
			{
				if (i_param[chan][REV])	i_param[chan][REV] = 0;
				else 					i_param[chan][REV] = 1;

				start_playing(chan);
				return;
			}
		}
	}
	// Swap sample_file_curpos with cache_high or _low
	// and move ->in to the equivlant address in play_buff
	// This gets us ready to read new data to the opposite end of the cache.

	if (i_param[chan][REV])
	{
		i_param[chan][REV] = 0;
		sample_file_curpos[chan] = cache_high[chan];
		play_buff[chan]->in = map_cache_to_buffer(cache_high[chan], samples[banknum][samplenum].sampleByteSize, cache_low[chan], cache_map_pt[chan], play_buff[chan]);
	}
	else
	{
		i_param[chan][REV] = 1;
		sample_file_curpos[chan] = cache_low[chan];
		play_buff[chan]->in = cache_map_pt[chan]; //cache_map_pt is the map of cache_low
	}

	//swap the endpos with the startpos
	t							= sample_file_endpos[chan];
	sample_file_endpos[chan]	= sample_file_startpos[chan];
	sample_file_startpos[chan]	= t;

	res = goto_filepos(sample_file_curpos[chan]); //uses samplenum and banknum to find startOfData

	if (res!=FR_OK)
		g_error |= FILE_SEEK_FAIL;

	 play_state[chan] = tplay_state;

}



//
// start_playing()
// Starts playing on a channel at the current param positions
//


void start_playing(uint8_t chan)
{
	uint8_t samplenum, banknum;
	Sample *s_sample;
	FRESULT res;
	uint8_t file_loaded;

	samplenum = i_param[chan][SAMPLE];
	banknum = i_param[chan][BANK];
	s_sample = &(samples[banknum][samplenum]);

	if (s_sample->filename[0] == 0)
		return;


	file_loaded = 0;
	check_change_bank(chan);

	//Check if sample/bank changed, or file is flagged for reload
	//if so, close the current file and open the new sample file
	if (flags[ForceFileReload1+chan] || (sample_num_now_playing[chan] != samplenum) || (sample_bank_now_playing[chan] != banknum) || fil[chan].obj.fs==0)
	{
		flags[ForceFileReload1+chan] = 0;

		res = reload_sample_file(&fil[chan], s_sample);
		if (res != FR_OK)	{g_error |= FILE_OPEN_FAIL;play_state[chan] = SILENT;return;}

		res = create_linkmap(&fil[chan], chan);
		if (res != FR_OK) {g_error |= FILE_CANNOT_CREATE_CLTBL; f_close(&fil[chan]);play_state[chan] = SILENT;return;}
		
		file_loaded = 1;

		//Check the file is really as long as the sampleSize says it is
		if (f_size(&fil[chan]) < (s_sample->startOfData + s_sample->sampleSize))
		{
			s_sample->sampleSize = f_size(&fil[chan]) - s_sample->startOfData;

			if (s_sample->inst_end > s_sample->sampleSize)
				s_sample->inst_end = s_sample->sampleSize;

			if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
				s_sample->inst_size = s_sample->sampleSize - s_sample->inst_start;
		}

		sample_num_now_playing[chan] = samplenum;
		sample_bank_now_playing[chan] = i_param[chan][BANK];

		cache_low[chan] = 0;
		cache_high[chan] = 0;
		cache_map_pt[chan] = play_buff[chan]->min;
	}

	//Determine starting and ending addresses
	if (i_param[chan][REV])
	{
		sample_file_endpos[chan] = calc_start_point(f_param[chan][START], s_sample);
		sample_file_startpos[chan] = calc_stop_points(f_param[chan][LENGTH], s_sample, sample_file_endpos[chan]);
	}
	else
	{
		sample_file_startpos[chan] = calc_start_point(f_param[chan][START], s_sample);
		sample_file_endpos[chan] = calc_stop_points(f_param[chan][LENGTH], s_sample, sample_file_startpos[chan]);
	}

	// Set up parameters to begin playing


	//see if starting position is already cached
	if ((cache_high[chan]>cache_low[chan]) && (cache_low[chan] <= sample_file_startpos[chan]) && (sample_file_startpos[chan] <= cache_high[chan]))
	{
		play_buff[chan]->out = map_cache_to_buffer(sample_file_startpos[chan], s_sample->sampleByteSize, cache_low[chan], cache_map_pt[chan], play_buff[chan]);

		if (play_buff[chan]->out < play_buff[chan]->in)	play_buff[chan]->wrapping = 0;
		else											play_buff[chan]->wrapping = 1;

		if (f_param[chan][LENGTH] < 0.5 && i_param[chan][REV])	play_state[chan] = PLAYING_PERC;
		else													play_state[chan] = PLAY_FADEUP;


	}
	else //...otherwise, start buffering from scratch
	{
		CB_init(play_buff[chan], i_param[chan][REV]);

		if (i_param[chan][REV])
			play_buff[chan]->in = play_buff[chan]->max - ((READ_BLOCK_SIZE * 2) / s_sample->sampleByteSize);

		//Reload the sample file (important to do, in case card has been removed)
		if (!file_loaded)
		{
			res = reload_sample_file(&fil[chan], s_sample);
			if (res != FR_OK)	{g_error |= FILE_OPEN_FAIL;play_state[chan] = SILENT;return;}

			res = create_linkmap(&fil[chan], chan);
			if (res != FR_OK) {g_error |= FILE_CANNOT_CREATE_CLTBL; f_close(&fil[chan]);play_state[chan] = SILENT;return;}
		}

		res = goto_filepos(sample_file_startpos[chan]);
		if (res != FR_OK) g_error |= FILE_SEEK_FAIL;

		if (g_error & LSEEK_FPTR_MISMATCH)
		{
			sample_file_startpos[chan] = align_addr(f_tell(&fil[chan]) - s_sample->startOfData, s_sample->blockAlign);
		}

		sample_file_curpos[chan] 		= sample_file_startpos[chan];

		cache_low[chan] 				= sample_file_startpos[chan];
		cache_high[chan] 				= sample_file_startpos[chan];
		cache_map_pt[chan] 				= play_buff[chan]->min;
		cache_size[chan]				= (play_buff[chan]->size>>1) * s_sample->sampleByteSize;
		is_buffered_to_file_end[chan] = 0;

		play_state[chan]=PREBUFFERING;
//		DEBUG2_ON;
	}

	flags[PlayBuff1_Discontinuity+chan] = 1;

	if (i_param[chan][REV])	decay_amp_i[chan]=0.0;
	else					decay_amp_i[chan]=1.0;
	decay_inc[chan]=0.0;

	play_led_state[chan]=1;

	if (global_mode[MONITOR_RECORDING])	global_mode[MONITOR_RECORDING] = 0;

	//DEBUG
	str_cpy(dbg_sample.filename , s_sample->filename);
	dbg_sample.sampleSize 		= s_sample->sampleSize;
	dbg_sample.sampleByteSize	= s_sample->sampleByteSize;
	dbg_sample.sampleRate 		= s_sample->sampleRate;
	dbg_sample.numChannels 		= s_sample->numChannels;
	dbg_sample.blockAlign 		= s_sample->blockAlign;
	dbg_sample.PCM 				= s_sample->PCM;
	dbg_sample.inst_start 		= s_sample->inst_start;
	dbg_sample.inst_end 		= s_sample->inst_end;
	dbg_sample.inst_size 		= s_sample->inst_size;
	dbg_sample.inst_gain 		= s_sample->inst_gain;
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
	uint32_t zeropt;
	uint32_t inst_size;

	zeropt = sample->inst_start;
	inst_size = sample->inst_end - sample->inst_start;

	//Determine the starting address
	if (start_param < 0.002)			return(align_addr(zeropt, sample->blockAlign));
	else if (start_param > 0.998)		return(align_addr( (zeropt + inst_size - (READ_BLOCK_SIZE*2)), sample->blockAlign )); //just play the last 32 blocks (~64k samples)

	else								return(align_addr( (zeropt  +  ( (uint32_t)(start_param * (float)inst_size) )), sample->blockAlign ));


}


uint32_t calc_stop_points(float length, Sample *sample, uint32_t startpos)
{
	uint32_t dist, fwd_stop_point;

	dist = calc_play_length(length, sample);

	fwd_stop_point = startpos + dist;
	if (fwd_stop_point > sample->inst_end)
		fwd_stop_point = sample->inst_end;

	return (align_addr(fwd_stop_point, sample->blockAlign));

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


uint32_t tmp_buff_u32[READ_BLOCK_SIZE>>2];

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
	Sample *s_sample;
	FSIZE_t t_fptr;
	uint32_t pre_buff_size;
	uint32_t active_buff_size;
	float pb_adjustment;

//	DEBUG3_ON;

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
			s_sample = &(samples[banknum][samplenum]);

			play_buff_bufferedamt[chan] = CB_distance(play_buff[chan], i_param[chan][REV]);

			//
			//Try to recover from a file read error
			//
			if (g_error & (FILE_READ_FAIL_1 << chan))
			{
				res = reload_sample_file(&fil[chan], s_sample);
				if (res != FR_OK) {g_error |= FILE_OPEN_FAIL;play_state[chan] = SILENT;return;}

				res = create_linkmap(&fil[chan], chan);
				if (res != FR_OK) {g_error |= FILE_CANNOT_CREATE_CLTBL;f_close(&fil[chan]);play_state[chan] = SILENT;return;}

				//clear the error flag
				g_error &= ~(FILE_READ_FAIL_1 << chan);
			}
			else //If no file read error... [?? what are we doing here???]
			{

				if ( (!i_param[chan][REV] && (sample_file_curpos[chan] < s_sample->inst_end))
				 	|| (i_param[chan][REV] && (sample_file_curpos[chan] > s_sample->inst_start)) )
					is_buffered_to_file_end[chan] = 0;
			}

			//
			// Calculate the amount to pre-buffer before we play:
			//
			pb_adjustment = f_param[chan][PITCH] * (float)s_sample->sampleRate / (float)BASE_SAMPLE_RATE ;

			//Calculate how many bytes we need to pre-load in our buffer
			//
			//Note of interest: blockAlign already includes numChannels, so we essentially square it in the calc below.
			//Why? ...because we plow through the bytes in play_buff twice as fast if it's stereo,
			//and since it takes twice as long to load stereo data,
			//we have to preload four times as much data (2^2) vs (1^1)
			//
			pre_buff_size = (uint32_t)((float)(BASE_BUFFER_THRESHOLD * s_sample->blockAlign * s_sample->numChannels) * pb_adjustment);
			active_buff_size = pre_buff_size * 8;

			if (!is_buffered_to_file_end[chan] && 
				(
					(play_state[chan]==PREBUFFERING && (play_buff_bufferedamt[chan] < pre_buff_size)) ||
					(play_state[chan]!=PREBUFFERING && (play_buff_bufferedamt[chan] < active_buff_size))
				))
			{

				if (sample_file_curpos[chan] > s_sample->sampleSize) //we read too much data somehow //When does this happen? sample_file_curpos has not changed recently...
					g_error |= FILE_WAVEFORMATERR;							//Breakpoint


				else if (sample_file_curpos[chan] > s_sample->inst_end)
				{
					//if (i_param[chan][LOOPING])
					//	flags[Play1But]=1;
					//else
						is_buffered_to_file_end[chan] = 1;

					//sample_file_curpos[chan] = s_sample->inst_end;
					//goto_filepos(sample_file_curpos[chan]);
				}

				else
				{
					//
					// Forward reading:
					//
					if (i_param[chan][REV]==0)
					{
						rd = s_sample->inst_end -  sample_file_curpos[chan];

						if (rd > READ_BLOCK_SIZE) rd = READ_BLOCK_SIZE;
						//else align rd to 24

						DEBUG1_ON;
						res = f_read(&fil[chan], (uint8_t *)tmp_buff_u32, rd, &br);
						DEBUG1_OFF;

						if (res != FR_OK) 
						{
							g_error |= FILE_READ_FAIL_1 << chan; 
							is_buffered_to_file_end[chan] = 1;
						}


						if (br < rd)
						{
							g_error |= FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read
							is_buffered_to_file_end[chan] = 1;
						}

						//sample_file_curpos[chan] += br;
						sample_file_curpos[chan] = f_tell(&fil[chan]) - s_sample->startOfData;

						if (sample_file_curpos[chan] >= s_sample->inst_end)
						{
							//sample_file_curpos[chan] = s_sample->inst_end;
							//Breakpoint
							is_buffered_to_file_end[chan] = 1;
						}

					}

					//
					// Reverse reading:
					//
					else
					{
						if (sample_file_curpos[chan] > s_sample->inst_start)
							rd = sample_file_curpos[chan] - s_sample->inst_start;
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
							
							sample_file_curpos[chan] = f_tell(&fil[chan]) - s_sample->startOfData;

						}
						else //rd < READ_BLOCK_SIZE: read the first block (which is the last to be read, since we're reversing)
						{
							//to-do:
							//align rd to 24

							//Jump to the beginning
							sample_file_curpos[chan] = s_sample->inst_start;
							res = goto_filepos(sample_file_curpos[chan]);
							if (res!=FR_OK)
								g_error |= FILE_SEEK_FAIL;

							is_buffered_to_file_end[chan] = 1;
						}


						DEBUG1_ON;
						//Read one block forward
						t_fptr=f_tell(&fil[chan]);
						res = f_read(&fil[chan], (uint8_t *)tmp_buff_u32, rd, &br);
						if (res != FR_OK) 
							g_error |= FILE_READ_FAIL_1 << chan;
						DEBUG1_OFF;

						if (br < rd)		g_error |= FILE_UNEXPECTEDEOF;

						//Jump backwards to where we started reading
						res = f_lseek(&fil[chan], t_fptr);
						if (res != FR_OK)	g_error |= FILE_SEEK_FAIL;
						if (f_tell(&fil[chan])!=t_fptr)		g_error |= LSEEK_FPTR_MISMATCH;

					}


					//Write temporary buffer to play_buff[]->in
					if (res != FR_OK) 		g_error |= FILE_READ_FAIL_1 << chan;
					else
					{
						//Jump back in play_buff by the amount just read (re-sized from file addresses to buffer address)
						if (i_param[chan][REV])
							CB_offset_in_address(play_buff[chan], (rd * 2) / s_sample->sampleByteSize, 1);

						DEBUG3_ON;
						err=0;

						//
						// Write raw file data into buffer
						//
						if (s_sample->sampleByteSize == 2) //16bit
							err = memory_write32_cb(play_buff[chan], tmp_buff_u32, rd>>2, 0); 

						else
						if (s_sample->sampleByteSize == 3) //24bit
						{
							err = memory_write_24as16(play_buff[chan], (uint8_t *)tmp_buff_u32, rd, 0); //rd must be a multiple of 3
						} 
						else
						if (s_sample->sampleByteSize == 4 && s_sample->PCM == 3) //32-bit float
						{
							err = memory_write_32fas16(play_buff[chan], (float *)tmp_buff_u32, rd>>2, 0); //rd must be a multiple of 4

						}else
						if (s_sample->sampleByteSize == 4 && s_sample->PCM == 1) //32-bit int
						{
							err = memory_write_32ias16(play_buff[chan], (uint8_t *)tmp_buff_u32, rd, 0); //rd must be a multiple of 4
						}
						DEBUG3_OFF;

						//Update the cache addresses
						if (i_param[chan][REV])
						{
							//Ignore head crossing error if we are reversing and ended up with in==out (that's normal for the first reading)
							if (err && (play_buff[chan]->in == play_buff[chan]->out)) 
								err = 0;

							//
							//Jump back again in play_buff by the amount just read (re-sized from file addresses to buffer address)
							//This ensures play_buff[]->in points to the buffer seam
							//
							if (i_param[chan][REV])
								CB_offset_in_address(play_buff[chan], (rd * 2) / s_sample->sampleByteSize, 1);

							cache_low[chan] = sample_file_curpos[chan]; 
							cache_map_pt[chan] = play_buff[chan]->in;

							if ((cache_high[chan] - cache_low[chan]) > cache_size[chan])
							{
								cache_high[chan] = cache_low[chan] + cache_size[chan];
							}
						} 
						else {

							cache_high[chan] = sample_file_curpos[chan];

							if ((cache_high[chan] - cache_low[chan]) > cache_size[chan])
							{
								cache_map_pt[chan] = play_buff[chan]->in;
								cache_low[chan] = cache_high[chan] - cache_size[chan];
							}
						}

						if (err)
						{
							g_error |= READ_BUFF1_OVERRUN*(1+chan);
						}

					}

				}

			}

			//Check if we've prebuffered enough to start playing
			if ((is_buffered_to_file_end[chan] || play_buff_bufferedamt[chan] >= pre_buff_size) && play_state[chan] == PREBUFFERING)
			{
			//	DEBUG2_OFF;
				if (f_param[chan][LENGTH] < 0.5 && i_param[chan][REV])
					play_state[chan] = PLAYING_PERC;
				else
					play_state[chan] = PLAY_FADEUP;

			}

		} //play_state != SILENT, FADEDOWN
	} //for (chan)
//	DEBUG3_OFF;

}

void play_audio_from_buffer(int32_t *outL, int32_t *outR, uint8_t chan)
{
	uint16_t i;
	int32_t t_i32;
	float t_f;
	uint32_t t_u32;
	uint8_t t_flag;

	//Resampling:
	float rs;
	uint32_t resampled_buffer_size;
	uint32_t resampled_cache_size;

	uint32_t sample_file_playpos;
	float gain;

	//convenience variables
	float length;
	uint8_t samplenum, banknum;
	Sample *s_sample;


	samplenum = sample_num_now_playing[chan];
	banknum = sample_bank_now_playing[chan];
	s_sample = &(samples[banknum][samplenum]);


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

		if (s_sample->sampleRate == BASE_SAMPLE_RATE)
			rs = f_param[chan][PITCH];
		else
			rs = f_param[chan][PITCH] * ((float)s_sample->sampleRate / (float)BASE_SAMPLE_RATE);
		//
		//Resample data read from the play_buff and store into out[]
		//
DEBUG2_ON;
		
		if (global_mode[STEREO_MODE])
		{
			if ((rs*s_sample->numChannels)>MAX_RS)
				rs = MAX_RS / (float)s_sample->numChannels;

			if (s_sample->numChannels == 2)
			{
				t_u32 = play_buff[chan]->out;
				t_flag = flags[PlayBuff1_Discontinuity+chan];
				resample_read16_left(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, 4, chan, outL);

				play_buff[chan]->out = t_u32;
				flags[PlayBuff1_Discontinuity+chan] = t_flag;
				resample_read16_right(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, 4, chan, outR);
			}
			else	//MONO: read left channel and copy to right
			{
				resample_read16_left(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, 2, chan, outL);
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++) outR[i] = outL[i];
			}
		}
		else //not STEREO_MODE:
		{
			if (rs>(MAX_RS))
				rs = (MAX_RS);

			if (s_sample->numChannels == 2)
				resample_read16_avg(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, 4, chan, outL);
			else
				resample_read16_left(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, 2, chan, outL);

		}
DEBUG2_OFF;

		//Calculate length and where to stop playing
		length = f_param[chan][LENGTH];

		//Update the endpos (we should do this in params, no?)
		if (i_param[chan][REV])
			sample_file_startpos[chan] = calc_stop_points(length, &samples[banknum][samplenum], sample_file_endpos[chan]);
		else
			sample_file_endpos[chan] = calc_stop_points(length, &samples[banknum][samplenum], sample_file_startpos[chan]);
	

		gain = s_sample->inst_gain;

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

	 					//
						// See if we've played enough samples and should start fading down to stop playback
						//

	 					//Amount play_buff[]->out changes with each audio block sent to the codec
	 					resampled_buffer_size = (uint32_t)((HT16_CHAN_BUFF_LEN * s_sample->numChannels * 2) * rs);

	 					//Amount an imaginary pointer in the sample file would move forward with each audio block sent to the codec
						resampled_cache_size = (resampled_buffer_size * s_sample->sampleByteSize) >> 1;

						//Find out how far ahead our output data is from the start of the cache
						sample_file_playpos = map_buffer_to_cache(play_buff[chan]->out, s_sample->sampleByteSize, cache_low[chan], cache_map_pt[chan], play_buff[chan]); 

						//See if we are about to surpass the calculated position in the file where we should end our sample
						if (!i_param[chan][REV])
						{
							if ((sample_file_playpos + (resampled_cache_size*2)) >= sample_file_endpos[chan])
								play_state[chan] = PLAY_FADEDOWN;
						} else {
							if (sample_file_playpos <= ((resampled_cache_size*2) + sample_file_endpos[chan]))
								play_state[chan] = PLAY_FADEDOWN;
						}

						if (play_state[chan] != PLAY_FADEDOWN)
						{
							//Check if we are about to hit the end of the file (or buffer undderrun)
							play_buff_bufferedamt[chan] = CB_distance(play_buff[chan], i_param[chan][REV]);

							if (play_buff_bufferedamt[chan] <= resampled_buffer_size)
							{

								//This gives a false end of file sometimes, if the ->in pointer is misplaced
								//
								//if (is_buffered_to_file_end[chan])
								//	play_state[chan] = PLAY_FADEDOWN; //hit end of file

								//else//buffer underrun: tried to read too much out. Try to recover!
								//{
									//DEBUG2_ON;
									g_error |= READ_BUFF1_OVERRUN<<chan;
									check_errors();

									play_state[chan] = PREBUFFERING;
									//is_buffered_to_file_end[chan] = 0;

									//play_buff[chan]->out = play_buff[chan]->in;

									//if (i_param[chan][REV]) resampled_buffer_size *= 4;

									//CB_offset_out_address(play_buff[chan], resampled_buffer_size, !i_param[chan][REV]);
									//play_buff[chan]->out = play_buff[chan]->min + align_addr((play_buff[chan]->out - play_buff[chan]->min), s_sample->blockAlign);

								//}
							}

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
