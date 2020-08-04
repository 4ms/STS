/*
 * sampler.c
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
#include "str_util.h"
#include "wav_recording.h"
#include "sts_filesystem.h"
#include "edit_mode.h"
#include "sample_file.h"
#include "circular_buffer_cache.h"
#include "bank.h"
#include "leds.h"

static inline int32_t _SSAT16(int32_t x);
static inline int32_t _SSAT16(int32_t x) {asm("ssat %[dst], #16, %[src]" : [dst] "=r" (x) : [src] "r" (x)); return x;}


//
// DEBUG
//
#ifdef DEBUG_ENABLED
Sample dbg_sample;
#endif

//
// Filesystem:
//
extern FATFS FatFs;

//
// System-wide parameters, flags, modes, states
//
extern float 				f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t				i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern uint8_t 				global_mode[NUM_GLOBAL_MODES];

extern uint8_t 				flags[NUM_FLAGS];
extern enum 				g_Errors g_error;

extern uint8_t 				play_led_state[NUM_PLAY_CHAN];

extern volatile uint32_t 	sys_tmr;
uint32_t					last_play_start_tmr[NUM_PLAY_CHAN];

uint8_t 					SAMPLINGBYTES=2;

uint32_t 					end_out_ctr[NUM_PLAY_CHAN]={0,0};
uint32_t 					play_led_flicker_ctr[NUM_PLAY_CHAN]={0,0};


//
// Memory
//

uint32_t tmp_buff_u32[READ_BLOCK_SIZE>>2];

//
// SDRAM buffer addresses for playing from sdcard
// SD Card:fil[]@sample_file_curpos --> SDARM @play_buff[]->in ... SDRAM @play_buff[]->out --> Codec
//
CircularBuffer splay_buff				[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
CircularBuffer* play_buff				[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

//make this a struct: play_state
enum PlayStates play_state				[NUM_PLAY_CHAN]; //activity
uint8_t			sample_num_now_playing	[NUM_PLAY_CHAN]; //sample_now_playing
uint8_t			sample_bank_now_playing	[NUM_PLAY_CHAN]; //bank_now_playing

//ToDo: make cache_* a struct
//The cache tells us what section of the file has been cached into the play_buff
//cache_low and cache_high and cache_size are all in file's native units (varies by the file's bit rate and stereo/mono)
//cache_map_pt is in units of play_buff, and tells us what address cache_low maps to
//
uint32_t		cache_low				[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK]; //file position address that corresponds to lowest position in file that's cached
uint32_t		cache_high				[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK]; //file position address that corresponds to highest position in file that's cached
uint32_t		cache_size				[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK]; //size in bytes of the cache (should always equal play_buff[]->size / 2 * sampleByteSize
uint32_t		cache_map_pt			[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK]; //address in play_buff[] that corresponds to cache_low
uint8_t 		is_buffered_to_file_end	[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK]; //1 = file is totally cached (from inst_start to inst_end), otherwise 0
uint32_t 		play_buff_bufferedamt	[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
uint8_t			cached_rev_state		[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

//ToDo: make this a struct
uint32_t 		sample_file_startpos	[NUM_PLAY_CHAN]; //file position where we began playback. 
uint32_t		sample_file_endpos		[NUM_PLAY_CHAN]; //file position where we will end playback. endpos > startpos when REV==0, endpos < startpos when REV==1

uint32_t 		sample_file_curpos		[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK]; //current file position being read. Must match the actual open file's position. This is always inc/decrementing from startpos towards endpos


enum PlayLoadTriage play_load_triage;

#define SET_FILE_POS(c, b, s)	f_lseek(&fil[c][s], samples[b][s].startOfData + sample_file_curpos[c][s]);\
								if(fil[c][s].fptr != (samples[b][s].startOfData + sample_file_curpos[c][s]) ) g_error|=LSEEK_FPTR_MISMATCH;


//
//PLAYING_PERC envelopes:
//
float decay_amp_i[NUM_PLAY_CHAN];
float decay_inc[NUM_PLAY_CHAN]={0,0};


//
// Filesystem
//
FIL fil[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

//
// Sample info
//
extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];


void audio_buffer_init(void)
{
	uint8_t i, chan;
	uint8_t window_num;

	SAMPLINGBYTES=2;

	memory_clear();

	init_rec_buff();

	for ( chan=0; chan<NUM_PLAY_CHAN; chan++ ){
		for ( i=0; i<NUM_SAMPLES_PER_BANK; i++ )
		{
			play_buff[chan][i] 				= &(splay_buff[chan][i]);

			window_num = (chan * NUM_SAMPLES_PER_BANK) + i;

			play_buff[chan][i]->min			= PLAY_BUFF_START + (window_num * PLAY_BUFF_SLOT_SIZE);
			play_buff[chan][i]->max			= play_buff[chan][i]->min + PLAY_BUFF_SLOT_SIZE;
			play_buff[chan][i]->size		= PLAY_BUFF_SLOT_SIZE;

			play_buff[chan][i]->in			= play_buff[chan][i]->min;
			play_buff[chan][i]->out			= play_buff[chan][i]->min;

			play_buff[chan][i]->wrapping	= 0;

			cache_map_pt[chan][i]		 	= play_buff[chan][i]->min;
			cache_low[chan][i] 				= 0;
			cache_high[chan][i] 			= 0;
			cached_rev_state[chan][i]		= 0;
			play_buff_bufferedamt[chan][i]	= 0;
			is_buffered_to_file_end[chan][i]= 0;
		}
	}


	flags[PlayBuff1_Discontinuity] = 1;
	flags[PlayBuff2_Discontinuity] = 1;

}


void toggle_reverse(uint8_t chan)
{
	uint8_t samplenum, banknum;
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
	if (tplay_state == PREBUFFERING || tplay_state==PLAY_FADEUP || tplay_state==PLAYING)
	{
		// If we just started playing, and then we get a reverse flag a short time afterwards,
		// then we should not just reverse direction, but instead play from the opposite end (sample_file_endpos).
		// The reason for this can be shown in the following patch:
		// If the user fires two triggers into Play and Rev, it should play the sample backwards. 
		// Then if the user fires those triggers again, it should play the sample forwards.
		// So let's say these two triggers are a little bit off (<100ms)
		// (or perhaps they are the same trigger but we detect them at different times)
		// For example if Rev was detected 20 milliseconds after Play, the Sampler would start playing 20 milliseconds of sound,
		// then reverse and and play those 20 milliseconds of audio backwards, then stop, for a total of 40ms of audio, then silence until the next play trig.
		// This is not the intended behavior of the user.
		//
		// To acheive the right behavior, we need to make a decision about how much difference between triggers is considered "simulataneous"
		// We just estimate this at 100ms (ToDo: should this be longer or shorter? The QCDEXP has a min delay of 135ms, so it should be less than that)
		//
		if ((sys_tmr - last_play_start_tmr[chan]) < (f_BASE_SAMPLE_RATE*0.1)) //100ms
		{
			// See if the endpos is within the cache,
			// Then we can just play from that point
			if ((sample_file_endpos[chan] >= cache_low[chan][samplenum]) && (sample_file_endpos[chan] <= cache_high[chan][samplenum]))
				play_buff[chan][samplenum]->out = map_cache_to_buffer(sample_file_endpos[chan], samples[banknum][samplenum].sampleByteSize, cache_low[chan][samplenum], cache_map_pt[chan][samplenum], play_buff[chan][samplenum]);
			else
			{
				//Otherwise we have to make a new cache, so run start_playing()
				//And return from this function
				if (i_param[chan][REV])	i_param[chan][REV] = 0;
				else 					i_param[chan][REV] = 1;

				start_playing(chan);
				return;
			}
		}
	}

	if (i_param[chan][REV])		i_param[chan][REV] = 0;
	else						i_param[chan][REV] = 1;

	reverse_file_positions(chan, samplenum, banknum, i_param[chan][REV]);

	cached_rev_state[chan][ i_param[chan][SAMPLE] ] = i_param[chan][REV];

	play_state[chan] = tplay_state;
}

void reverse_file_positions(uint8_t chan, uint8_t samplenum, uint8_t banknum, uint8_t new_dir)
{
	uint32_t swap;
	FRESULT res;

	// Swap sample_file_curpos with cache_high or _low
	// and move ->in to the equivalant address in play_buff
	// This gets us ready to read new data to the opposite end of the cache.

	if (new_dir) {
		sample_file_curpos[chan][samplenum] = cache_low[chan][samplenum];
		play_buff[chan][samplenum]->in		= cache_map_pt[chan][samplenum]; //cache_map_pt is the map of cache_low
	}
	else {
		sample_file_curpos[chan][samplenum] = cache_high[chan][samplenum];
		play_buff[chan][samplenum]->in 		= map_cache_to_buffer(cache_high[chan][samplenum], samples[banknum][samplenum].sampleByteSize, cache_low[chan][samplenum], cache_map_pt[chan][samplenum], play_buff[chan][samplenum]);
	}

	// if (play_buff[chan][samplenum]->out < play_buff[chan][samplenum]->in)	play_buff[chan][samplenum]->wrapping = i_param[chan][REV]? 1 : 0;
	// else																	play_buff[chan][samplenum]->wrapping = i_param[chan][REV]? 0 : 1;

	//Swap the endpos with the startpos
	//This way, curpos is always moving towards endpos and away from startpos
	swap						= sample_file_endpos[chan];
	sample_file_endpos[chan]	= sample_file_startpos[chan];
	sample_file_startpos[chan]	= swap;

	//Seek the starting position in the file 
	//This gets us ready to start reading from the new position
	if (fil[chan][samplenum].obj.id > 0)
	{
		res = SET_FILE_POS(chan, banknum, samplenum);
		if (res!=FR_OK) g_error |= FILE_SEEK_FAIL;
	}


} 

void init_changed_bank(uint8_t chan)
{
	uint8_t samplenum;
	FRESULT res;

	for ( samplenum=0; samplenum<NUM_SAMPLES_PER_BANK; samplenum++ )
	{
		res = f_close(&fil[chan][samplenum]);
		if (res != FR_OK) fil[chan][samplenum].obj.fs = 0;

		is_buffered_to_file_end[chan][samplenum] = 0;

		CB_init(play_buff[chan][samplenum], 0);
	}
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
	float rs;
	//uint8_t file_loaded;

	samplenum = i_param[chan][SAMPLE];
	banknum = i_param[chan][BANK];
	s_sample = &(samples[banknum][samplenum]);

	if (s_sample->filename[0] == 0)
		return;

	sample_num_now_playing[chan] = samplenum;

	if ( banknum != sample_bank_now_playing[chan] )
	{
		sample_bank_now_playing[chan] = banknum;
		init_changed_bank(chan);
	}

	//
	// Reload the sample file if necessary
	//
	if (flags[ForceFileReload1+chan] || fil[chan][samplenum].obj.fs==0)
	{
		flags[ForceFileReload1+chan] = 0;

		res = reload_sample_file(&fil[chan][samplenum], s_sample);
		if (res != FR_OK)	{g_error |= FILE_OPEN_FAIL;play_state[chan] = SILENT;return;}

		res = create_linkmap(&fil[chan][samplenum], chan, samplenum);
		if (res == FR_NOT_ENOUGH_CORE) {g_error |= FILE_CANNOT_CREATE_CLTBL;} //ToDo: Log this error
		else if (res != FR_OK) {g_error |= FILE_CANNOT_CREATE_CLTBL; f_close(&fil[chan][samplenum]);play_state[chan] = SILENT;return;}
		
		//Check the file is really as long as the sampleSize says it is
		if (f_size(&fil[chan][samplenum]) < (s_sample->startOfData + s_sample->sampleSize))
		{
			s_sample->sampleSize = f_size(&fil[chan][samplenum]) - s_sample->startOfData;

			if (s_sample->inst_end > s_sample->sampleSize)
				s_sample->inst_end = s_sample->sampleSize;

			if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
				s_sample->inst_size = s_sample->sampleSize - s_sample->inst_start;
		}

		cache_low[chan][samplenum] 		= 0;
		cache_high[chan][samplenum] 	= 0;
		cache_map_pt[chan][samplenum] 	= play_buff[chan][samplenum]->min;
	}


	//Calculate our actual resampling rate
	rs = f_param[chan][PITCH] * ((float)s_sample->sampleRate / f_BASE_SAMPLE_RATE);

	//Determine starting and ending addresses
	if (i_param[chan][REV])
	{
		sample_file_endpos[chan] = calc_start_point(f_param[chan][START], s_sample);
		sample_file_startpos[chan] = calc_stop_point(f_param[chan][LENGTH], rs, s_sample, sample_file_endpos[chan]);
	}
	else
	{
		sample_file_startpos[chan] = calc_start_point(f_param[chan][START], s_sample);
		sample_file_endpos[chan] = calc_stop_point(f_param[chan][LENGTH], rs, s_sample, sample_file_startpos[chan]);
	}


	//See if the starting position is already cached
	if (   (cache_high[chan][samplenum] > cache_low[chan][samplenum]) 
		&& (cache_low[chan][samplenum] <= sample_file_startpos[chan]) 
		&& (sample_file_startpos[chan] <= cache_high[chan][samplenum]) )
	{
		play_buff[chan][samplenum]->out = map_cache_to_buffer(sample_file_startpos[chan], s_sample->sampleByteSize, cache_low[chan][samplenum], cache_map_pt[chan][samplenum], play_buff[chan][samplenum]);

		if (f_param[chan][LENGTH] <= 0.5 && i_param[chan][REV])	play_state[chan] = PLAYING_PERC;
		else													play_state[chan] = PLAY_FADEUP;

	}
	else //...otherwise, start buffering from scratch
	{
		play_state[chan]=PREBUFFERING;
		CB_init(play_buff[chan][samplenum], i_param[chan][REV]);

		//Seek to the file position where we will start reading
		sample_file_curpos[chan][samplenum] 		= sample_file_startpos[chan];
		res = SET_FILE_POS(chan, banknum, samplenum);

		//If seeking fails, perhaps we need to reload the file
		if (res != FR_OK) {
			res = reload_sample_file(&fil[chan][samplenum], s_sample);
			if (res != FR_OK)	{g_error |= FILE_OPEN_FAIL;play_state[chan] = SILENT;return;}

			res = create_linkmap(&fil[chan][samplenum], chan, samplenum);
			if (res != FR_OK) {g_error |= FILE_CANNOT_CREATE_CLTBL; f_close(&fil[chan][samplenum]);play_state[chan] = SILENT;return;}

			res = SET_FILE_POS(chan, banknum, samplenum);
			if (res != FR_OK) {g_error |= FILE_SEEK_FAIL;}
		}
		if (g_error & LSEEK_FPTR_MISMATCH)
		{
			sample_file_startpos[chan] = align_addr(f_tell(&fil[chan][samplenum]) - s_sample->startOfData, s_sample->blockAlign);
		}

		cache_low[chan][samplenum] 					= sample_file_startpos[chan];
		cache_high[chan][samplenum] 				= sample_file_startpos[chan];
		cache_map_pt[chan][samplenum] 				= play_buff[chan][samplenum]->min;
		cache_size[chan][samplenum]					= (play_buff[chan][samplenum]->size>>1) * s_sample->sampleByteSize;
		is_buffered_to_file_end[chan][samplenum] 	= 0;
	}

	last_play_start_tmr[chan]	= sys_tmr; //used by toggle_reverse() to see if we hit a reverse trigger right after a play trigger

	flags[PlayBuff1_Discontinuity+chan] = 1;

	if (i_param[chan][REV])	decay_amp_i[chan]=0.0;
	else					decay_amp_i[chan]=1.0;

	decay_inc[chan]=0.0;

	play_led_state[chan]=1;

	if (global_mode[ALLOW_SPLIT_MONITORING] && global_mode[STEREO_MODE]==0)
		//Turn off monitoring for just this channel
		global_mode[MONITOR_RECORDING] &= ~(1<<chan);
	else
		global_mode[MONITOR_RECORDING] = MONITOR_OFF;

	//DEBUG
	#ifdef DEBUG_ENABLED
	str_cpy(dbg_sample.filename , s_sample->filename);
	dbg_sample.sampleSize 		= s_sample->sampleSize;
	dbg_sample.sampleByteSize	= s_sample->sampleByteSize;
	dbg_sample.sampleRate 		= s_sample->sampleRate;
	dbg_sample.numChannels 		= s_sample->numChannels;
	dbg_sample.startOfData 		= s_sample->startOfData;
	dbg_sample.blockAlign 		= s_sample->blockAlign;
	dbg_sample.PCM 				= s_sample->PCM;
	dbg_sample.inst_start 		= s_sample->inst_start;
	dbg_sample.inst_end 		= s_sample->inst_end;
	dbg_sample.inst_size 		= s_sample->inst_size;
	dbg_sample.inst_gain 		= s_sample->inst_gain;
	#endif
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

	//Stop it if we're playing a full sample
	else if (play_state[chan]==PLAYING && f_param[chan][LENGTH] > 0.98)
	{
		if (global_mode[LENGTH_FULL_START_STOP])	play_state[chan]=PLAY_FADEDOWN;
		else 										play_state[chan]=RETRIG_FADEDOWN;
		
		play_led_state[chan]=0;
	}

	//Re-start if we have a short length
	else if (play_state[chan]==PLAYING_PERC || play_state[chan]==PLAYING || play_state[chan]==PAD_SILENCE  || play_state[chan]==PLAYING_PERC_FADEDOWN)
	{
		play_state[chan]=RETRIG_FADEDOWN;
		play_led_state[chan]=0;

	}


}


uint32_t calc_start_point(float start_param, Sample *sample)
{
	uint32_t zeropt;
	uint32_t inst_size;

	zeropt = sample->inst_start;
	inst_size = sample->inst_end - sample->inst_start;

	//If the sample size is smaller than two blocks, the start point is forced to the start
	if (inst_size <= (READ_BLOCK_SIZE*2))	return(align_addr(zeropt, sample->blockAlign));

	if (start_param < 0.002)				return(align_addr(zeropt, sample->blockAlign));
	else if (start_param > 0.998)			return(align_addr( (zeropt + inst_size - (READ_BLOCK_SIZE*2)), sample->blockAlign )); //just play the last 32 blocks (~64k samples)
	else									return(align_addr( (zeropt  +  ( (uint32_t)(start_param * (float)inst_size) )), sample->blockAlign ));


}

//Returns an offset from the startpos, based on the length param (knob and CV) and resampling rate
//
uint32_t calc_stop_point(float length_param, float resample_param, Sample *sample, uint32_t startpos)
{
	uint32_t fwd_stop_point;
	uint32_t num_samples_to_play;
	uint32_t max_play_length;
	float seconds;
	float t_f;
	uint32_t t_int;

	seconds  = (float)(sample->sampleRate * sample->blockAlign);
	max_play_length = sample->inst_end - sample->inst_start; 	// as opposed to taking sample->inst_size because that won't be clipped to the end of a sample file

	if (length_param > 0.98){num_samples_to_play = max_play_length;}														// 100% >= knob > 98% -->  play 100%


	else if (length_param > 0.5 && max_play_length <= (0.625 * seconds))													// 98% >= knob >= 50% and sample length <= 0.625s
	{
		if (max_play_length <= (0.5 * seconds))
			num_samples_to_play = max_play_length;																			// --- 0 < sample length <= 0.5s ---> play full sample length
		else 
			num_samples_to_play = 0.5  * seconds + ((max_play_length - (0.5 * seconds)) * (length_param - 0.5)/(0.98-0.5));	// --- 0.5s < sample length <= 0.625 ---> play between 0.5s and full sample
	}

	else if (length_param>0.95) {num_samples_to_play = max_play_length * (6.67 * length_param - 5.5366);} 					// 98% >= knob > 95%  --> play bweteen 100% to 80% of full sample

	else if (length_param>0.50) 																							// 95% >= knob > 50%  (and sample length > 0.625s)
	{
		if (max_play_length > (5.0 * seconds) ) 																			// --- sample length > 5s
		{
			if (length_param>0.85)
				num_samples_to_play = 4.5 * seconds + ((length_param-0.85) * 8) * (max_play_length - 4.5 * seconds);  		// -------- 95% >= knob > 85%  --> play 80%  to 5.5s
			else
				num_samples_to_play = (11.43 * length_param - 5.215) * seconds;												// -------- 85% >= knob > 50%  --> play 4.5s to 0.5s
		}

		else 																												// --- 5s >= sample length > 0.625s --> play 0.5s to 80%																					
			num_samples_to_play = 0.5 * seconds + (length_param * 1.7778 - 0.8889) * (max_play_length - 0.625 * seconds); 

	}
	
	// length_param <= 50%: fixed envelope 
	else
	{
		//number of samples to play is length*PERC_ENV_FACTOR, rounded up to multiples of HT16_CHAN_BUFF_LEN
		//times the block align
		//times the playback resample rate (1.0=44.1kHz), rounded up the the next integer

		t_f = ((length_param)*PERC_ENV_FACTOR)/((float)HT16_CHAN_BUFF_LEN);
		t_int = (uint32_t)t_f;
		t_int = (t_int + 3) * HT16_CHAN_BUFF_LEN; //+1 does ceiling(t_f)
		t_f = (float)t_int;
		t_f *= resample_param;
		t_int = (((uint32_t)t_f)) * sample->blockAlign;
		num_samples_to_play = t_int;
	}

	fwd_stop_point = startpos + num_samples_to_play;

	if (fwd_stop_point > sample->inst_end)
		fwd_stop_point = sample->inst_end;

	return (align_addr(fwd_stop_point, sample->blockAlign));

}


//
// Updates the play light to indicate if a sample is present or not
// Doesn't flash the light brightly, like it does when the Sample knob is turned,
// because that's distracting to see when pressing the bank button.
// Also sets the sample changed flag.
//
void check_change_bank(uint8_t chan)
{
	if (flags[PlayBank1Changed + chan])
	{
		flags[PlayBank1Changed + chan]=0;

		// Set flag that the sample has changed
		flags[PlaySample1Changed + chan]=1;

		//
		//Changing bank updates the play button "not-playing" color (dim white or dim red)
		//But avoids the bright flash of white or red by setting PlaySampleXChanged_* = 1
		//

		if (samples[ i_param[chan][BANK] ][ i_param[chan][SAMPLE] ].filename[0] == 0)
		{
			flags[PlaySample1Changed_valid + chan] = 0;
			flags[PlaySample1Changed_empty + chan] = 1;
		} else 
		{
			flags[PlaySample1Changed_valid + chan] = 1;
			flags[PlaySample1Changed_empty + chan] = 0;
		}

	}
}

void check_change_sample(void)
{
	uint8_t chan;

	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{
		if (flags[PlaySample1Changed+chan])
		{
			flags[PlaySample1Changed+chan]=0;


			if ( cached_rev_state[ chan ][ i_param[chan][SAMPLE] ] != i_param[chan][REV] )
			{
				reverse_file_positions(chan, i_param[chan][SAMPLE], i_param[chan][BANK], i_param[chan][REV]);
				cached_rev_state[ chan ][ i_param[chan][SAMPLE] ] = i_param[chan][REV];
			}

			//FixMe: Clean up this logic:
			if (samples[ i_param[chan][BANK] ][ i_param[chan][SAMPLE] ].filename[0] == 0) //no file: fadedown or remain silent
			{
				//No sample in this slot:

				//Set the sample empty flag to 1 (dim) only if it's 0
				//(that way we avoid dimming it if we had already set the flag to 6 in order to flash it brightly)
				if (flags[PlaySample1Changed_empty+chan]==0)
					flags[PlaySample1Changed_empty+chan] = 1;

				flags[PlaySample1Changed_valid+chan] = 0;

				if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_ALWAYS || (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_LOOPING && i_param[chan][LOOPING]))
				{
					if (play_state[chan] != SILENT && play_state[chan] != PREBUFFERING){
						if (play_state[chan] == PLAYING_PERC)	play_state[chan] = PLAYING_PERC_FADEDOWN;
						else									play_state[chan] = PLAY_FADEDOWN;

					}else
						play_state[chan] = SILENT;
				}
			}
			else
			{
				//Sample found in this slot:

				//Set the sample valid flag to 1 (dim) only if it's 0
				//(that way we avoid dimming it if we had already set the flag to 6 in order to flash it brightly)
				if (flags[PlaySample1Changed_valid+chan]==0)
					flags[PlaySample1Changed_valid+chan] = 1;

				flags[PlaySample1Changed_empty+chan] = 0;

				if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_ALWAYS)
				{
					if (play_state[chan] == SILENT && i_param[chan][LOOPING])
						flags[Play1But+chan]=1;

					if (play_state[chan] != SILENT && play_state[chan] != PREBUFFERING){
						if (play_state[chan] == PLAYING_PERC)	play_state[chan] = PLAYING_PERC_FADEDOWN;
						else									play_state[chan] = PLAY_FADEDOWN;

					}
				}
				else
				{
					if (i_param[chan][LOOPING]){
						if (play_state[chan] == SILENT)
							flags[Play1But+chan]=1;

						else if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE]==AutoStop_LOOPING)
						{
							if (play_state[chan] == PLAYING_PERC)	play_state[chan] = PLAYING_PERC_FADEDOWN;
							else									play_state[chan] = PLAY_FADEDOWN;
						}
					}
				}
			}
		}
	}
}

// void set_play_state(uint8_t chan, enum PlayStates new_state)
// {
// 	uint32_t resampled_buffer_size;
// 	uint32_t resampled_cache_size;
// 	int32_t dist_to_end;
// 	uint8_t samplenum, banknum;

// 	switch(new_state){
// 		case PLAYING_PERC:
// 			if (play_state[chan] == PLAYING)
// 			{

// 				if (i_param[chan][REV])	decay_amp_i[chan]=0.0;
// 				else					decay_amp_i[chan]=1.0;

// 			}
// 		break;
// 	}
// }

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


	check_change_sample();
	check_change_bank(0);
	check_change_bank(1);

	// DEBUG0_ON;
	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{

		if (play_state[chan] != SILENT && play_state[chan] != PLAY_FADEDOWN && play_state[chan] != RETRIG_FADEDOWN)
		{
			samplenum = sample_num_now_playing[chan];
			banknum = sample_bank_now_playing[chan];
			s_sample = &(samples[banknum][samplenum]);

			//FixMe: Calculate play_buff_bufferedamt after play_buff changes, not here
			play_buff_bufferedamt[chan][samplenum] = CB_distance(play_buff[chan][samplenum], i_param[chan][REV]);

			//
			//Try to recover from a file read error
			//
			if (g_error & (FILE_READ_FAIL_1 << chan))
			{
				res = reload_sample_file(&fil[chan][samplenum], s_sample);
				if (res != FR_OK) {g_error |= FILE_OPEN_FAIL;play_state[chan] = SILENT;return;}

				res = create_linkmap(&fil[chan][samplenum], chan, samplenum);
				if (res != FR_OK) {g_error |= FILE_CANNOT_CREATE_CLTBL;f_close(&fil[chan][samplenum]);play_state[chan] = SILENT;return;}

				//clear the error flag
				g_error &= ~(FILE_READ_FAIL_1 << chan);
			}
			else //If no file read error... [?? FixMe: does this logic make sense for clearing is_buffered_to_file_end ???]
			{

				if ( (!i_param[chan][REV] && (sample_file_curpos[chan][samplenum] < s_sample->inst_end))
				 	|| (i_param[chan][REV] && (sample_file_curpos[chan][samplenum] > s_sample->inst_start)) )
					is_buffered_to_file_end[chan][samplenum] = 0;
			}

			//
			// Calculate the amount to pre-buffer before we play:
			//
			pb_adjustment = f_param[chan][PITCH] * (float)s_sample->sampleRate / f_BASE_SAMPLE_RATE ;

			//Calculate how many bytes we need to pre-load in our buffer
			//
			//Note of interest: blockAlign already includes numChannels, so we essentially square it in the calc below.
			//The reason is that we plow through the bytes in play_buff twice as fast if it's stereo,
			//and since it takes twice as long to load stereo data from the sd card,
			//we have to preload four times as much data (2^2) vs (1^1)
			//
			pre_buff_size = (uint32_t)((float)(BASE_BUFFER_THRESHOLD * s_sample->blockAlign * s_sample->numChannels) * pb_adjustment);
			active_buff_size = pre_buff_size * 4;

			if (active_buff_size > ((play_buff[chan][samplenum]->size * 7) / 10) ) //limit amount of buffering ahead to 90% of buffer size
				active_buff_size = ((play_buff[chan][samplenum]->size * 7) / 10);

			if (!is_buffered_to_file_end[chan][samplenum] && 
				(
					(play_state[chan]==PREBUFFERING && (play_buff_bufferedamt[chan][samplenum] < pre_buff_size)) ||
					(play_state[chan]!=PREBUFFERING && (play_buff_bufferedamt[chan][samplenum] < active_buff_size))
				))
			{

				if (sample_file_curpos[chan][samplenum] > s_sample->sampleSize) //we read too much data somehow //When does this happen? sample_file_curpos has not changed recently...
				{
					g_error |= FILE_WAVEFORMATERR;
					play_state[chan] = SILENT;
					start_playing(chan);
				}


				else if (sample_file_curpos[chan][samplenum] > s_sample->inst_end)
				{
					is_buffered_to_file_end[chan][samplenum] = 1;
				}

				else
				{
					//
					// Forward reading:
					//
					if (i_param[chan][REV]==0)
					{
						rd = s_sample->inst_end -  sample_file_curpos[chan][samplenum];

						if (rd > READ_BLOCK_SIZE) rd = READ_BLOCK_SIZE;

						res = f_read(&fil[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, &br);
						if (res != FR_OK) 
						{
							g_error |= FILE_READ_FAIL_1 << chan; 
							is_buffered_to_file_end[chan][samplenum] = 1; //FixMe: Do we really want to set this in case of disk error? We don't when reversing.
						}


						if (br < rd)
						{
							g_error |= FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read
							is_buffered_to_file_end[chan][samplenum] = 1; 
						}

						//sample_file_curpos[chan][samplenum] += br;
						sample_file_curpos[chan][samplenum] = f_tell(&fil[chan][samplenum]) - s_sample->startOfData;

						if (sample_file_curpos[chan][samplenum] >= s_sample->inst_end)
						{
							is_buffered_to_file_end[chan][samplenum] = 1;
						}

					}

					//
					// Reverse reading:
					//
					else
					{
						if (sample_file_curpos[chan][samplenum] > s_sample->inst_start)
							rd = sample_file_curpos[chan][samplenum] - s_sample->inst_start;
						else
							rd = 0;

						if (rd >= READ_BLOCK_SIZE)
						{

							//Jump back a block
							rd = READ_BLOCK_SIZE;

							t_fptr=f_tell(&fil[chan][samplenum]);
							res = f_lseek(&fil[chan][samplenum], t_fptr - READ_BLOCK_SIZE);
							if (res || (f_tell(&fil[chan][samplenum])!=(t_fptr - READ_BLOCK_SIZE)))
								g_error |= LSEEK_FPTR_MISMATCH;
							
							sample_file_curpos[chan][samplenum] = f_tell(&fil[chan][samplenum]) - s_sample->startOfData;

						}
						else //rd < READ_BLOCK_SIZE: read the first block (which is the last to be read, since we're reversing)
						{
							//to-do:
							//align rd to 24

							//Jump to the beginning
							sample_file_curpos[chan][samplenum] = s_sample->inst_start;
							res = SET_FILE_POS(chan, banknum, samplenum);
							if (res!=FR_OK)
								g_error |= FILE_SEEK_FAIL;

							is_buffered_to_file_end[chan][samplenum] = 1;
						}

						//Read one block forward
						t_fptr=f_tell(&fil[chan][samplenum]);
						res = f_read(&fil[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, &br);
						if (res != FR_OK) 
							g_error |= FILE_READ_FAIL_1 << chan;

						if (br < rd)		g_error |= FILE_UNEXPECTEDEOF;

						//Jump backwards to where we started reading
						res = f_lseek(&fil[chan][samplenum], t_fptr);
						if (res != FR_OK)	g_error |= FILE_SEEK_FAIL;
						if (f_tell(&fil[chan][samplenum])!=t_fptr)		g_error |= LSEEK_FPTR_MISMATCH;

					}


					//Write temporary buffer to play_buff[]->in
					if (res != FR_OK) 		g_error |= FILE_READ_FAIL_1 << chan;
					else
					{
						//Jump back in play_buff by the amount just read (re-sized from file addresses to buffer address)
						if (i_param[chan][REV])
							CB_offset_in_address(play_buff[chan][samplenum], (rd * 2) / s_sample->sampleByteSize, 1);

						err=0;

						//
						// Write raw file data (tmp_buff_u32) into buffer (play_buff)
						//
						if (s_sample->sampleByteSize == 2) //16bit
							err = memory_write_16as16(play_buff[chan][samplenum], tmp_buff_u32, rd>>2, 0);

						else
						if (s_sample->sampleByteSize == 3) //24bit
							err = memory_write_24as16(play_buff[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, 0); //rd must be a multiple of 3

						else
						if (s_sample->sampleByteSize == 1) //8bit
							err = memory_write_8as16(play_buff[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, 0);

						else
						if (s_sample->sampleByteSize == 4 && s_sample->PCM == 3) //32-bit float
							err = memory_write_32fas16(play_buff[chan][samplenum], (float *)tmp_buff_u32, rd>>2, 0); //rd must be a multiple of 4

						else
						if (s_sample->sampleByteSize == 4 && s_sample->PCM == 1) //32-bit int
							err = memory_write_32ias16(play_buff[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, 0); //rd must be a multiple of 4

						//Update the cache addresses
						if (i_param[chan][REV])
						{
							//Ignore head crossing error if we are reversing and ended up with in==out (that's normal for the first reading)
							if (err && (play_buff[chan][samplenum]->in == play_buff[chan][samplenum]->out)) 
								err = 0;

							//
							//Jump back again in play_buff by the amount just read (re-sized from file addresses to buffer address)
							//This ensures play_buff[]->in points to the buffer seam
							//
							CB_offset_in_address(play_buff[chan][samplenum], (rd * 2) / s_sample->sampleByteSize, 1);

							cache_low[chan][samplenum] 		= sample_file_curpos[chan][samplenum]; 
							cache_map_pt[chan][samplenum] 	= play_buff[chan][samplenum]->in;

							if ((cache_high[chan][samplenum] - cache_low[chan][samplenum]) > cache_size[chan][samplenum])
								 cache_high[chan][samplenum] = cache_low[chan][samplenum] + cache_size[chan][samplenum];
						} 
						else {

							cache_high[chan][samplenum] 		= sample_file_curpos[chan][samplenum];

							if ((cache_high[chan][samplenum] - cache_low[chan][samplenum]) > cache_size[chan][samplenum])
							{
								cache_map_pt[chan][samplenum] 	= play_buff[chan][samplenum]->in;
								cache_low[chan][samplenum] 		= cache_high[chan][samplenum] - cache_size[chan][samplenum];
							}
						}

						if (err)
							g_error |= READ_BUFF1_OVERRUN<<chan;

					}

				}

			}

			//Check if we've prebuffered enough to start playing
			if ((is_buffered_to_file_end[chan][samplenum] || play_buff_bufferedamt[chan][samplenum] >= pre_buff_size) && play_state[chan] == PREBUFFERING)
			{
				if (f_param[chan][LENGTH] <= 0.5 && i_param[chan][REV])
					play_state[chan] = PLAYING_PERC;
				else
					play_state[chan] = PLAY_FADEUP;
			}

		} //play_state != SILENT, FADEDOWN
	} //for (chan)
	// DEBUG0_OFF;
}


// calc_resampled_cache_size
// Amount an imaginary pointer in the sample file would move with each audio block sent to the codec
//
uint32_t 		calc_resampled_cache_size(uint8_t samplenum, uint8_t banknum, uint32_t resampled_buffer_size);
inline uint32_t	calc_resampled_cache_size(uint8_t samplenum, uint8_t banknum, uint32_t resampled_buffer_size)
{
	return ((resampled_buffer_size * samples[banknum][samplenum].sampleByteSize) / 2);
}

// calc_resampled_buffer_size
// Amount play_buff[]->out changes with each audio block sent to the codec
//
uint32_t 		calc_resampled_buffer_size(uint8_t chan, uint8_t samplenum, uint8_t banknum, float resample_rate);
inline uint32_t	calc_resampled_buffer_size(uint8_t chan, uint8_t samplenum, uint8_t banknum, float resample_rate)
{
	return ((uint32_t)((HT16_CHAN_BUFF_LEN * samples[banknum][samplenum].numChannels * 2) * resample_rate));
}

int32_t 		calc_dist_to_end(uint8_t chan, uint8_t samplenum, uint8_t banknum);
inline int32_t	calc_dist_to_end(uint8_t chan, uint8_t samplenum, uint8_t banknum)
{
	uint32_t sample_file_playpos;

	//Find out where the audio output data is relative to the start of the cache
	sample_file_playpos = map_buffer_to_cache(play_buff[chan][samplenum]->out, samples[banknum][samplenum].sampleByteSize, cache_low[chan][samplenum], cache_map_pt[chan][samplenum], play_buff[chan][samplenum]); 

	//Calculate the distance left to the end that we should be playing
	if (!i_param[chan][REV]) 	return (sample_file_endpos[chan] - sample_file_playpos);
	else 						return (sample_file_playpos - sample_file_endpos[chan]);

}


void play_audio_from_buffer(int32_t *outL, int32_t *outR, uint8_t chan)
{
	uint16_t i;
	float env;
	uint32_t t_u32;
	uint8_t t_flag;

	//Resampling:
	float rs;
	uint32_t resampled_buffer_size;
	int32_t resampled_cache_size;

	float gain;
	float play_time;
	int32_t dist_to_end;

	//convenience variables
	float length;
	uint8_t samplenum, banknum;
	Sample *s_sample;

	// DEBUG0_ON;

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
		samplenum = sample_num_now_playing[chan];
		banknum = sample_bank_now_playing[chan];
		s_sample = &(samples[banknum][samplenum]);

		//Calculate our actual resampling rate, based on the sample rate of the file being played

		if (s_sample->sampleRate == BASE_SAMPLE_RATE)	rs = f_param[chan][PITCH];
		else											rs = f_param[chan][PITCH] * ((float)s_sample->sampleRate / f_BASE_SAMPLE_RATE);


		// FixMe: Consider moving this to a new function that's called after play_audio_buffer()
		// See if we've played enough samples and should start fading down to stop playback
		//	
		if (play_state[chan] == PLAYING || play_state[chan] == PLAY_FADEUP || play_state[chan] == PLAYING_PERC)
		{
			resampled_buffer_size = calc_resampled_buffer_size(chan, samplenum, banknum, rs);				// Amount play_buff[]->out changes with each audio block sent to the codec
			resampled_cache_size = calc_resampled_cache_size(samplenum, banknum, resampled_buffer_size);	// Amount an imaginary pointer in the sample file would move with each audio block sent to the codec			
			dist_to_end = calc_dist_to_end(chan, samplenum, banknum);										// Amount in the sample file we have remaining before we hit sample_file_endpos

			if (dist_to_end < resampled_cache_size*2)														//See if we are about to surpass the calculated position in the file where we should end our sample
			{	
				if (flags[ChangePlaytoPerc1+chan]){		play_state[chan] = PLAY_FADEDOWN;					//If we just changed from PLAYING to PLAYING_PERC then, do a normal Fadedown or else we'll get annoying PAD_SILENCE
														flags[ChangePlaytoPerc1+chan]=0;}
				else
				if (play_state[chan]==PLAYING_PERC /*&& !i_param[chan][REV]*/)
														play_state[chan] = PLAYING_PERC_FADEDOWN;
				else									play_state[chan] = PLAY_FADEDOWN;
			}
			else {
				//Check if we are about to hit buffer underrun
				play_buff_bufferedamt[chan][samplenum] = CB_distance(play_buff[chan][samplenum], i_param[chan][REV]);

				if (!is_buffered_to_file_end[chan][samplenum] && play_buff_bufferedamt[chan][samplenum] <= resampled_buffer_size)
					{
						g_error |= READ_BUFF1_UNDERRUN<<chan;
						check_errors();
						play_state[chan] = PREBUFFERING;}//buffer underrun: tried to read too much out. Try to recover!
			}
		}


		//
		//Resample data read from the play_buff and store into out[]
		//

		if (global_mode[STEREO_MODE])
		{
			if ((rs*s_sample->numChannels)>MAX_RS)
				rs = MAX_RS / (float)s_sample->numChannels;

			if (s_sample->numChannels == 2)
			{
				t_u32 = play_buff[chan][samplenum]->out;
				t_flag = flags[PlayBuff1_Discontinuity+chan];
				resample_read16_left(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 4, chan, outL);

				play_buff[chan][samplenum]->out = t_u32;
				flags[PlayBuff1_Discontinuity+chan] = t_flag;
				resample_read16_right(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 4, chan, outR);
			}
			else	//MONO: read left channel and copy to right
			{
				resample_read16_left(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 2, chan, outL);
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++) outR[i] = outL[i];
			}
		}
		else //not STEREO_MODE:
		{
			if (rs>(MAX_RS))
				rs = (MAX_RS);

			if (s_sample->numChannels == 2)
				resample_read16_avg(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 4, chan, outL);
			else
				resample_read16_left(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 2, chan, outL);

		}

		//Calculate length and where to stop playing
		length = f_param[chan][LENGTH];
		gain = s_sample->inst_gain * f_param[chan][VOLUME];

		//Update the start/endpos based on the length parameter
		//Update the play_time (used to calculate led flicker and END OUT pulse width
		//ToDo: we should do this in update_params
		//
		if (i_param[chan][REV])
		{
			sample_file_startpos[chan] = calc_stop_point(length, rs, &samples[banknum][samplenum], sample_file_endpos[chan]);
			play_time = (sample_file_startpos[chan] - sample_file_endpos[chan]) / (s_sample->blockAlign * s_sample->sampleRate * f_param[chan][PITCH]);
		}else
		{
			sample_file_endpos[chan] = calc_stop_point(length, rs, &samples[banknum][samplenum], sample_file_startpos[chan]);
			play_time = (sample_file_endpos[chan] - sample_file_startpos[chan]) / (s_sample->blockAlign * s_sample->sampleRate * f_param[chan][PITCH]);
		}
		

		//We could make this code more readable by putting the switch() inside the for() loop, but that would waste a lot of processor cycles
		 switch (play_state[chan])
		 {

			 case (PLAY_FADEDOWN):
			 case (RETRIG_FADEDOWN):
			 	DEBUG1_ON;
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
				 	if (global_mode[FADEUPDOWN_ENVELOPE])	env = (float)(HT16_CHAN_BUFF_LEN-i) / (float)HT16_CHAN_BUFF_LEN;
					else									env = 1.0;

					outL[i] = (float)outL[i] * env * gain;
					outR[i] = (float)outR[i] * env * gain;

					outL[i] = _SSAT16(outL[i]);
					outR[i] = _SSAT16(outR[i]);
				}

				flicker_endout(chan, play_time);

				//Start playing again if we're looking, or re-triggered
				//Unless we faded down because of a play trigger
				if ((play_state[chan]==RETRIG_FADEDOWN || i_param[chan][LOOPING]) && !flags[Play1TrigDelaying+chan])
					flags[Play1But+chan] = 1;

				play_state[chan] = SILENT;
				DEBUG1_OFF;
			break;

			 case (PLAY_FADEUP):
			 DEBUG3_ON;
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					if (global_mode[FADEUPDOWN_ENVELOPE])	env = (float)i / (float)HT16_CHAN_BUFF_LEN;
					else									env = 1.0;

					outL[i] = (float)outL[i] * env * gain;
					outR[i] = (float)outR[i] * env * gain;

					outL[i] = _SSAT16(outL[i]);
					outR[i] = _SSAT16(outR[i]);
				}

				if (length>0.5)			play_state[chan]	= PLAYING;
				else					play_state[chan] 	= PLAYING_PERC;
				
				DEBUG3_OFF;
		 	 break;

			 case (PLAYING):
				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					outL[i] = (float)outL[i] * gain;
					outR[i] = (float)outR[i] * gain;

					outL[i] = _SSAT16(outL[i]);
					outR[i] = _SSAT16(outR[i]);
				}
				if (length<=0.5)	flags[ChangePlaytoPerc1+chan] = 1;

		 	 break;

			 case (PLAYING_PERC):
			 case (PLAYING_PERC_FADEDOWN):
			 case (PAD_SILENCE):
			 	
				decay_inc[chan] = 1.0f/((length)*PERC_ENV_FACTOR);

		 		if (play_state[chan]==PLAYING_PERC) 
		 		{
		 			DEBUG2_ON;
					for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
					{
						if (i_param[chan][REV])	decay_amp_i[chan] += decay_inc[chan];
						else					decay_amp_i[chan] -= decay_inc[chan];

						if (decay_amp_i[chan] < 0.0f) 		{decay_inc[chan] = 0.0; decay_amp_i[chan] = 0.0f;}
						else if (decay_amp_i[chan] > 1.0f) 	{decay_inc[chan] = 0.0; decay_amp_i[chan] = 1.0f;}

						if (global_mode[PERC_ENVELOPE]) env = decay_amp_i[chan];
						else env = 1.0;

						outL[i] = ((float)outL[i]) * env * gain;
						outR[i] = ((float)outR[i]) * env * gain;

						outL[i] = _SSAT16(outL[i]);
						outR[i] = _SSAT16(outR[i]);
					}
					DEBUG2_OFF;
				} else

				// Fade down to silence before going to PAD_SILENCE mode or ending the playback
				// (this prevents a click if the sample data itself doesn't cleanly fade out)
				//
		 		if (play_state[chan]==PLAYING_PERC_FADEDOWN) 
		 		{
		 			DEBUG0_ON;
					for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
					{
						if (i_param[chan][REV])	decay_amp_i[chan] += decay_inc[chan];
						else					decay_amp_i[chan] -= decay_inc[chan];

						if (decay_amp_i[chan] < 0.0f) 		{decay_inc[chan] = 0.0; decay_amp_i[chan] = 0.0f;}
						else if (decay_amp_i[chan] > 1.0f) 	{decay_inc[chan] = 0.0; decay_amp_i[chan] = 1.0f;}

						if (global_mode[FADEUPDOWN_ENVELOPE])	env = decay_amp_i[chan] * (float)(HT16_CHAN_BUFF_LEN-i) / (float)HT16_CHAN_BUFF_LEN;
						else									env = 1.0;

						outL[i] = ((float)outL[i]) * env * gain;
						outR[i] = ((float)outR[i]) * env * gain;

						outL[i] = _SSAT16(outL[i]);
						outR[i] = _SSAT16(outR[i]);
					}
					DEBUG0_OFF;
					//If the sample is very short, then pad it with silence so we get a consistant end out period
					// if ( (sample->inst_end - sample->inst_start) < (READ_BLOCK_SIZE*2) )

					// If the end point is the end of the sample data (which happens if the file is very short, or if we're at the end of it)
					// Then pad it with silence so we keep a constant End Out period when looping
					if ( sample_file_endpos[chan] == s_sample->inst_end )
						play_state[chan]=PAD_SILENCE;
					else
						decay_amp_i[chan] = 0.0f; //force a sample ending
				}
				else

				// Fill a short sample with silence in order to obtain the fixed loop time
				//
				if (play_state[chan]==PAD_SILENCE)
				{
					if (i_param[chan][REV])	decay_amp_i[chan] += HT16_CHAN_BUFF_LEN * decay_inc[chan];
					else					decay_amp_i[chan] -= HT16_CHAN_BUFF_LEN * decay_inc[chan];

					for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
					{
						outL[i] = 0;
						outR[i] = 0;
					}
				}

				//After fading up to full amplitude in a reverse percussive playback, fade back down to silence:
				if (decay_amp_i[chan] >= 1.0f && i_param[chan][REV] && play_state[chan]==PLAYING_PERC)
					play_state[chan] = PLAY_FADEDOWN;
				
				//End the playback, go to silence, and trigger another play if looping
				else if (decay_amp_i[chan] <= 0.0f || decay_amp_i[chan] >= 1.0f)
				{
					decay_amp_i[chan] = 0.0f;

					flicker_endout(chan, length*3.0f);

					//Restart loop
					if (i_param[chan][LOOPING] && !flags[Play1TrigDelaying+chan])
						flags[Play1Trig+chan] = 1;

					play_state[chan] = SILENT;
				}
				break;

			 default: //PREBUFFERING or SILENT
				 break;

		 }//switch play_state

	} //if play_state

	// //FixMe: samplenum is not necessarily set at this point in the function
	// if (	(play_state[0]==PLAYING && (play_buff_bufferedamt[0][samplenum] < 15000)) ||
	// 		(play_state[1]==PLAYING && (play_buff_bufferedamt[1][samplenum] < 15000)) )

	// 	play_load_triage = PRIORITIZE_PLAYING;
	// else
	// 	play_load_triage = NO_PRIORITY;

// DEBUG0_OFF;

}




void SDIO_read_IRQHandler(void)
{
	if (TIM_GetITStatus(SDIO_read_TIM, TIM_IT_Update) != RESET) {

		//Set the flag to tell the main loop to read more from the sdcard
		//We don't want to read_storage_to_buffer() here inside the interrupt because we might be interrupting a current read/write process
		//Keeping sdcard read/writes out of interrupts forces us to complete each read/write before beginning another

		flags[TimeToReadStorage] = 1;

		TIM_ClearITPendingBit(SDIO_read_TIM, TIM_IT_Update);
	}
}
