/*
 * sampler.c - Main functionality for audio playback
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#include "sampler.h"
#include "audio_sdram.h"
#include "audio_util.h"
#include "ff.h"
#include "globals.h"

#include "adc.h"
#include "compressor.h"
#include "dig_pins.h"
#include "params.h"
#include "resample.h"
#include "rgb_leds.h"
#include "timekeeper.h"

#include "bank.h"
#include "circular_buffer.h"
#include "circular_buffer_cache.h"
#include "edit_mode.h"
#include "fatfs_util.h"
#include "leds.h"
#include "sample_file.h"
#include "sampler_calcs.h"
#include "str_util.h"
#include "sts_filesystem.h"
#include "wav_recording.h"
#include "wavefmt.h"

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
extern float f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t i_param[NUM_ALL_CHAN][NUM_I_PARAMS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern GlobalParams global_params;

extern uint8_t flags[NUM_FLAGS];
extern enum g_Errors g_error;

extern uint8_t play_led_state[NUM_PLAY_CHAN];

extern volatile uint32_t sys_tmr;
uint32_t last_play_start_tmr[NUM_PLAY_CHAN];

uint32_t end_out_ctr[NUM_PLAY_CHAN] = {0, 0};
uint32_t play_led_flicker_ctr[NUM_PLAY_CHAN] = {0, 0};

//
// Memory
//

uint32_t tmp_buff_u32[READ_BLOCK_SIZE >> 2];

//
// SDRAM buffer addresses for playing from sdcard
// SD Card:fil[]@sample_file_curpos --> SDARM @play_buff[]->in ... SDRAM @play_buff[]->out --> Codec
//
CircularBuffer splay_buff[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
CircularBuffer *play_buff[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

//make this a struct: play_state
enum PlayStates play_state[NUM_PLAY_CHAN];		//activity
uint8_t sample_num_now_playing[NUM_PLAY_CHAN];	//sample_now_playing
uint8_t sample_bank_now_playing[NUM_PLAY_CHAN]; //bank_now_playing

//ToDo: make cache_* a struct
//The cache tells us what section of the file has been cached into the play_buff
//cache_low and cache_high and cache_size are all in file's native units (varies by the file's bit rate and stereo/mono)
//cache_map_pt is in units of play_buff, and tells us what address cache_low maps to

//file position address that corresponds to lowest position in file that's cached
uint32_t cache_low[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
//file position address that corresponds to highest position in file that's cached
uint32_t cache_high[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
//size in bytes of the cache (should always equal play_buff[]->size / 2 * sampleByteSize
uint32_t cache_size[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
//address in play_buff[] that corresponds to cache_low
uint32_t cache_map_pt[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
//1 = file is totally cached (from inst_start to inst_end), otherwise 0
uint8_t is_buffered_to_file_end[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
uint32_t play_buff_bufferedamt[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];
uint8_t cached_rev_state[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

//ToDo: make this a struct
uint32_t sample_file_startpos[NUM_PLAY_CHAN]; //file position where we began playback.
//file position where we will end playback. endpos > startpos when REV==0, endpos < startpos when REV==1
uint32_t sample_file_endpos[NUM_PLAY_CHAN];

//current file position being read. Must match the actual open file's position. This is always inc/decrementing from startpos towards endpos
uint32_t sample_file_curpos[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

enum PlayLoadTriage play_load_triage;

//
//PLAYING_PERC envelopes:
//
static float env_level[NUM_PLAY_CHAN];
static float env_rate[NUM_PLAY_CHAN] = {0, 0};

//
// Filesystem
//
FIL fil[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];

//
// Sample info
//
extern Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

// Private functions
static void check_change_sample(void);
static void check_change_bank(uint8_t chan);
static void init_changed_bank(uint8_t chan);
static void reverse_file_positions(uint8_t chan, uint8_t samplenum, uint8_t banknum, uint8_t new_dir);
static void check_perc_ending(uint8_t chan);
static void apply_envelopes(int32_t *outL, int32_t *outR, uint8_t chan);
static void apply_gain(int32_t *outL, int32_t *outR, float gain);
static float fade(int32_t *outL, int32_t *outR, float gain, float starting_amp, float rate);
static inline FRESULT SET_FILE_POS(uint8_t c, uint8_t b, uint8_t s);
static inline int32_t _SSAT16(int32_t x);

void audio_buffer_init(void) {
	uint8_t i, chan;
	uint8_t window_num;

	memory_clear();

	init_rec_buff();

	for (chan = 0; chan < NUM_PLAY_CHAN; chan++) {
		for (i = 0; i < NUM_SAMPLES_PER_BANK; i++) {
			play_buff[chan][i] = &(splay_buff[chan][i]);

			window_num = (chan * NUM_SAMPLES_PER_BANK) + i;

			play_buff[chan][i]->min = PLAY_BUFF_START + (window_num * PLAY_BUFF_SLOT_SIZE);
			play_buff[chan][i]->max = play_buff[chan][i]->min + PLAY_BUFF_SLOT_SIZE;
			play_buff[chan][i]->size = PLAY_BUFF_SLOT_SIZE;

			play_buff[chan][i]->in = play_buff[chan][i]->min;
			play_buff[chan][i]->out = play_buff[chan][i]->min;

			play_buff[chan][i]->wrapping = 0;

			cache_map_pt[chan][i] = play_buff[chan][i]->min;
			cache_low[chan][i] = 0;
			cache_high[chan][i] = 0;
			cached_rev_state[chan][i] = 0;
			play_buff_bufferedamt[chan][i] = 0;
			is_buffered_to_file_end[chan][i] = 0;
		}
	}

	flags[PlayBuff1_Discontinuity] = 1;
	flags[PlayBuff2_Discontinuity] = 1;
}

void toggle_reverse(uint8_t chan) {
	uint8_t samplenum, banknum;
	enum PlayStates tplay_state;

	if (play_state[chan] == PLAYING || play_state[chan] == PLAYING_PERC || play_state[chan] == PREBUFFERING ||
		play_state[chan] == PLAY_FADEUP || play_state[chan] == PERC_FADEUP)
	{
		samplenum = sample_num_now_playing[chan];
		banknum = sample_bank_now_playing[chan];
	} else {
		samplenum = i_param[chan][SAMPLE];
		banknum = i_param[chan][BANK];
	}

	tplay_state = play_state[chan];
	play_state[chan] = SILENT;

	//If we are PREBUFFERING or PLAY_FADEUP, then that means we just started playing.
	//It could be the case a common trigger fired into PLAY and REV, but the PLAY trig was detected first
	//So we actually want to make it play from the end of the sample rather than reverse direction from the current spot
	if (tplay_state == PREBUFFERING || tplay_state == PLAY_FADEUP || tplay_state == PLAYING ||
		tplay_state == PERC_FADEUP)
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
		if ((sys_tmr - last_play_start_tmr[chan]) < (global_params.f_record_sample_rate * 0.1f)) //100ms
		{
			// See if the endpos is within the cache,
			// Then we can just play from that point
			if ((sample_file_endpos[chan] >= cache_low[chan][samplenum]) &&
				(sample_file_endpos[chan] <= cache_high[chan][samplenum]))
			{
				play_buff[chan][samplenum]->out = map_cache_to_buffer(sample_file_endpos[chan],
																	  samples[banknum][samplenum].sampleByteSize,
																	  cache_low[chan][samplenum],
																	  cache_map_pt[chan][samplenum],
																	  play_buff[chan][samplenum]);
			} else {
				//Otherwise we have to make a new cache, so run start_playing()
				//And return from this function
				if (i_param[chan][REV])
					i_param[chan][REV] = 0;
				else
					i_param[chan][REV] = 1;

				start_playing(chan);
				return;
			}
		}
	}

	if (i_param[chan][REV])
		i_param[chan][REV] = 0;
	else
		i_param[chan][REV] = 1;

	reverse_file_positions(chan, samplenum, banknum, i_param[chan][REV]);

	cached_rev_state[chan][i_param[chan][SAMPLE]] = i_param[chan][REV];

	play_state[chan] = tplay_state;
}

static void reverse_file_positions(uint8_t chan, uint8_t samplenum, uint8_t banknum, uint8_t new_dir) {
	uint32_t swap;
	FRESULT res;

	// Swap sample_file_curpos with cache_high or _low
	// and move ->in to the equivalant address in play_buff
	// This gets us ready to read new data to the opposite end of the cache.

	if (new_dir) {
		sample_file_curpos[chan][samplenum] = cache_low[chan][samplenum];
		play_buff[chan][samplenum]->in = cache_map_pt[chan][samplenum]; //cache_map_pt is the map of cache_low
	} else {
		sample_file_curpos[chan][samplenum] = cache_high[chan][samplenum];
		play_buff[chan][samplenum]->in = map_cache_to_buffer(cache_high[chan][samplenum],
															 samples[banknum][samplenum].sampleByteSize,
															 cache_low[chan][samplenum],
															 cache_map_pt[chan][samplenum],
															 play_buff[chan][samplenum]);
	}

	//Swap the endpos with the startpos
	//This way, curpos is always moving towards endpos and away from startpos
	swap = sample_file_endpos[chan];
	sample_file_endpos[chan] = sample_file_startpos[chan];
	sample_file_startpos[chan] = swap;

	//Seek the starting position in the file
	//This gets us ready to start reading from the new position
	if (fil[chan][samplenum].obj.id > 0) {
		res = SET_FILE_POS(chan, banknum, samplenum);
		if (res != FR_OK)
			g_error |= FILE_SEEK_FAIL;
	}
}

static void init_changed_bank(uint8_t chan) {
	uint8_t samplenum;
	FRESULT res;

	for (samplenum = 0; samplenum < NUM_SAMPLES_PER_BANK; samplenum++) {
		res = f_close(&fil[chan][samplenum]);
		if (res != FR_OK)
			fil[chan][samplenum].obj.fs = 0;

		is_buffered_to_file_end[chan][samplenum] = 0;

		CB_init(play_buff[chan][samplenum], 0);
	}
}

//
// start_playing()
// Starts playing on a channel at the current param positions
//

void start_playing(uint8_t chan) {
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

	if (banknum != sample_bank_now_playing[chan]) {
		sample_bank_now_playing[chan] = banknum;
		init_changed_bank(chan);
	}

	//
	// Reload the sample file if necessary
	//
	if (flags[ForceFileReload1 + chan] || fil[chan][samplenum].obj.fs == 0) {
		flags[ForceFileReload1 + chan] = 0;

		res = reload_sample_file(&fil[chan][samplenum], s_sample);
		if (res != FR_OK) {
			g_error |= FILE_OPEN_FAIL;
			play_state[chan] = SILENT;
			return;
		}

		res = create_linkmap(&fil[chan][samplenum], chan, samplenum);
		if (res == FR_NOT_ENOUGH_CORE) {
			g_error |= FILE_CANNOT_CREATE_CLTBL;
		} //ToDo: Log this error
		else if (res != FR_OK)
		{
			g_error |= FILE_CANNOT_CREATE_CLTBL;
			f_close(&fil[chan][samplenum]);
			play_state[chan] = SILENT;
			return;
		}

		//Check the file is really as long as the sampleSize says it is
		if (f_size(&fil[chan][samplenum]) < (s_sample->startOfData + s_sample->sampleSize)) {
			s_sample->sampleSize = f_size(&fil[chan][samplenum]) - s_sample->startOfData;

			if (s_sample->inst_end > s_sample->sampleSize)
				s_sample->inst_end = s_sample->sampleSize;

			if ((s_sample->inst_start + s_sample->inst_size) > s_sample->sampleSize)
				s_sample->inst_size = s_sample->sampleSize - s_sample->inst_start;
		}

		cache_low[chan][samplenum] = 0;
		cache_high[chan][samplenum] = 0;
		cache_map_pt[chan][samplenum] = play_buff[chan][samplenum]->min;
	}

	//Calculate our actual resampling rate
	rs = f_param[chan][PITCH] * ((float)s_sample->sampleRate / global_params.f_record_sample_rate);

	//Determine starting and ending addresses
	if (i_param[chan][REV]) {
		sample_file_endpos[chan] = calc_start_point(f_param[chan][START], s_sample);
		sample_file_startpos[chan] = calc_stop_point(f_param[chan][LENGTH], rs, s_sample, sample_file_endpos[chan]);
	} else {
		sample_file_startpos[chan] = calc_start_point(f_param[chan][START], s_sample);
		sample_file_endpos[chan] = calc_stop_point(f_param[chan][LENGTH], rs, s_sample, sample_file_startpos[chan]);
	}

	//See if the starting position is already cached
	if ((cache_high[chan][samplenum] > cache_low[chan][samplenum]) &&
		(cache_low[chan][samplenum] <= sample_file_startpos[chan]) &&
		(sample_file_startpos[chan] <= cache_high[chan][samplenum]))
	{
		play_buff[chan][samplenum]->out = map_cache_to_buffer(sample_file_startpos[chan],
															  s_sample->sampleByteSize,
															  cache_low[chan][samplenum],
															  cache_map_pt[chan][samplenum],
															  play_buff[chan][samplenum]);

		env_level[chan] = 0.f;
		if (f_param[chan][LENGTH] <= 0.5f)
			play_state[chan] = i_param[chan][REV] ? PLAYING_PERC : PERC_FADEUP;
		else
			play_state[chan] = PLAY_FADEUP;

	} else {
		//...otherwise, start buffering from scratch
		// Set state to silent so we don't run play_audio_buffer(), which could result in a glitch since the playbuff and cache values are being changed
		play_state[chan] = SILENT;

		CB_init(play_buff[chan][samplenum], i_param[chan][REV]);

		//Seek to the file position where we will start reading
		sample_file_curpos[chan][samplenum] = sample_file_startpos[chan];
		res = SET_FILE_POS(chan, banknum, samplenum);

		//If seeking fails, perhaps we need to reload the file
		if (res != FR_OK) {
			res = reload_sample_file(&fil[chan][samplenum], s_sample);
			if (res != FR_OK) {
				g_error |= FILE_OPEN_FAIL;
				play_state[chan] = SILENT;
				return;
			}

			res = create_linkmap(&fil[chan][samplenum], chan, samplenum);
			if (res != FR_OK) {
				g_error |= FILE_CANNOT_CREATE_CLTBL;
				f_close(&fil[chan][samplenum]);
				play_state[chan] = SILENT;
				return;
			}

			res = SET_FILE_POS(chan, banknum, samplenum);
			if (res != FR_OK) {
				g_error |= FILE_SEEK_FAIL;
			}
		}
		if (g_error & LSEEK_FPTR_MISMATCH) {
			sample_file_startpos[chan] =
				align_addr(f_tell(&fil[chan][samplenum]) - s_sample->startOfData, s_sample->blockAlign);
		}

		cache_low[chan][samplenum] = sample_file_startpos[chan];
		cache_high[chan][samplenum] = sample_file_startpos[chan];
		cache_map_pt[chan][samplenum] = play_buff[chan][samplenum]->min;
		cache_size[chan][samplenum] = (play_buff[chan][samplenum]->size >> 1) * s_sample->sampleByteSize;
		is_buffered_to_file_end[chan][samplenum] = 0;

		play_state[chan] = PREBUFFERING;
	}

	//used by toggle_reverse() to see if we hit a reverse trigger right after a play trigger
	last_play_start_tmr[chan] = sys_tmr;

	flags[PlayBuff1_Discontinuity + chan] = 1;

	env_level[chan] = 0.f;
	env_rate[chan] = 0.f;

	play_led_state[chan] = 1;

	if (global_mode[ALLOW_SPLIT_MONITORING] && global_mode[STEREO_MODE] == 0)
		//Turn off monitoring for just this channel
		global_mode[MONITOR_RECORDING] &= ~(1 << chan);
	else
		global_mode[MONITOR_RECORDING] = MONITOR_OFF;

#ifdef DEBUG_ENABLED
	str_cpy(dbg_sample.filename, s_sample->filename);
	dbg_sample.sampleSize = s_sample->sampleSize;
	dbg_sample.sampleByteSize = s_sample->sampleByteSize;
	dbg_sample.sampleRate = s_sample->sampleRate;
	dbg_sample.numChannels = s_sample->numChannels;
	dbg_sample.startOfData = s_sample->startOfData;
	dbg_sample.blockAlign = s_sample->blockAlign;
	dbg_sample.PCM = s_sample->PCM;
	dbg_sample.inst_start = s_sample->inst_start;
	dbg_sample.inst_end = s_sample->inst_end;
	dbg_sample.inst_size = s_sample->inst_size;
	dbg_sample.inst_gain = s_sample->inst_gain;
#endif
}

//
// Determines whether to start/restart/stop playing
//
void toggle_playing(uint8_t chan) {

	//Start playing
	if (play_state[chan] == SILENT || play_state[chan] == PLAY_FADEDOWN || play_state[chan] == RETRIG_FADEDOWN ||
		play_state[chan] == PREBUFFERING || play_state[chan] == PLAY_FADEUP || play_state[chan] == PERC_FADEUP)
	{
		start_playing(chan);
	}

	//Stop it if we're playing a full sample
	else if (play_state[chan] == PLAYING && f_param[chan][LENGTH] > 0.98f)
	{
		if (global_mode[LENGTH_FULL_START_STOP]) {
			play_state[chan] = PLAY_FADEDOWN;
			env_level[chan] = 1.f;
		} else
			play_state[chan] = RETRIG_FADEDOWN;

		play_led_state[chan] = 0;
	}

	//Re-start if we have a short length
	else if (play_state[chan] == PLAYING_PERC || play_state[chan] == PLAYING || play_state[chan] == PAD_SILENCE ||
			 play_state[chan] == REV_PERC_FADEDOWN)
	{
		play_state[chan] = RETRIG_FADEDOWN;
		play_led_state[chan] = 0;
	}
}

void start_restart_playing(uint8_t chan) {
	//Start playing immediately if we have envelopes disabled for the mode that's playing, or we're not playing
	if ((!global_mode[FADEUPDOWN_ENVELOPE] && (play_state[chan] == PLAYING || play_state[chan] == PLAY_FADEDOWN)) ||
		(!global_mode[PERC_ENVELOPE] && play_state[chan] == PLAYING_PERC) || play_state[chan] == SILENT ||
		play_state[chan] == PREBUFFERING)
	{
		start_playing(chan);
	}

	//Re-start if we're playing (and have envelopes enabled)
	else
	{
		play_state[chan] = RETRIG_FADEDOWN;
		play_led_state[chan] = 0;
	}
}

//
// Updates the play light to indicate if a sample is present or not
// Doesn't flash the light brightly, like it does when the Sample knob is turned,
// because that's distracting to see when pressing the bank button.
// Also sets the sample changed flag.
//
static void check_change_bank(uint8_t chan) {
	if (flags[PlayBank1Changed + chan]) {
		flags[PlayBank1Changed + chan] = 0;

		// Set flag that the sample has changed
		flags[PlaySample1Changed + chan] = 1;

		//
		//Changing bank updates the play button "not-playing" color (dim white or dim red)
		//But avoids the bright flash of white or red by setting PlaySampleXChanged_* = 1
		//

		if (samples[i_param[chan][BANK]][i_param[chan][SAMPLE]].filename[0] == 0) {
			flags[PlaySample1Changed_valid + chan] = 0;
			flags[PlaySample1Changed_empty + chan] = 1;
		} else {
			flags[PlaySample1Changed_valid + chan] = 1;
			flags[PlaySample1Changed_empty + chan] = 0;
		}
	}
}

static void check_change_sample(void) {
	uint8_t chan;

	for (chan = 0; chan < NUM_PLAY_CHAN; chan++) {
		if (flags[PlaySample1Changed + chan]) {
			flags[PlaySample1Changed + chan] = 0;

			if (cached_rev_state[chan][i_param[chan][SAMPLE]] != i_param[chan][REV]) {
				reverse_file_positions(chan, i_param[chan][SAMPLE], i_param[chan][BANK], i_param[chan][REV]);
				cached_rev_state[chan][i_param[chan][SAMPLE]] = i_param[chan][REV];
			}

			//FixMe: Clean up this logic:
			//no file: fadedown or remain silent
			if (samples[i_param[chan][BANK]][i_param[chan][SAMPLE]].filename[0] == 0) {
				//No sample in this slot:

				//Set the sample empty flag to 1 (dim) only if it's 0
				//(that way we avoid dimming it if we had already set the flag to 6 in order to flash it brightly)
				if (flags[PlaySample1Changed_empty + chan] == 0)
					flags[PlaySample1Changed_empty + chan] = 1;

				flags[PlaySample1Changed_valid + chan] = 0;

				if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] == AutoStop_ALWAYS ||
					(global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] == AutoStop_LOOPING && i_param[chan][LOOPING]))
				{
					if (play_state[chan] != SILENT && play_state[chan] != PREBUFFERING) {
						if (play_state[chan] == PLAYING_PERC)
							play_state[chan] = REV_PERC_FADEDOWN;
						else {
							play_state[chan] = PLAY_FADEDOWN;
							env_level[chan] = 1.f;
						}

					} else
						play_state[chan] = SILENT;
				}
			} else {
				//Sample found in this slot:

				//Set the sample valid flag to 1 (dim) only if it's 0
				//(that way we avoid dimming it if we had already set the flag to 6 in order to flash it brightly)
				if (flags[PlaySample1Changed_valid + chan] == 0)
					flags[PlaySample1Changed_valid + chan] = 1;

				flags[PlaySample1Changed_empty + chan] = 0;

				if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] == AutoStop_ALWAYS) {
					if (play_state[chan] == SILENT && i_param[chan][LOOPING])
						flags[Play1But + chan] = 1;

					if (play_state[chan] != SILENT && play_state[chan] != PREBUFFERING) {
						if (play_state[chan] == PLAYING_PERC)
							play_state[chan] = REV_PERC_FADEDOWN;
						else {
							play_state[chan] = PLAY_FADEDOWN;
							env_level[chan] = 1.f;
						}
					}
				} else {
					if (i_param[chan][LOOPING]) {
						if (play_state[chan] == SILENT)
							flags[Play1But + chan] = 1;

						else if (global_mode[AUTO_STOP_ON_SAMPLE_CHANGE] == AutoStop_LOOPING) {
							if (play_state[chan] == PLAYING_PERC)
								play_state[chan] = REV_PERC_FADEDOWN;
							else {
								play_state[chan] = PLAY_FADEDOWN;
								env_level[chan] = 1.f;
							}
						}
					}
				}
			}
		}
	}
}

void read_storage_to_buffer(void) {
	uint8_t chan = 0;
	uint32_t err;

	FRESULT res;
	uint32_t br;
	uint32_t rd;

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

	for (chan = 0; chan < NUM_PLAY_CHAN; chan++) {

		if ((play_state[chan] != SILENT) && (play_state[chan] != PLAY_FADEDOWN) &&
			(play_state[chan] != RETRIG_FADEDOWN))
		{
			samplenum = sample_num_now_playing[chan];
			banknum = sample_bank_now_playing[chan];
			s_sample = &(samples[banknum][samplenum]);

			//FixMe: Calculate play_buff_bufferedamt after play_buff changes, not here
			play_buff_bufferedamt[chan][samplenum] = CB_distance(play_buff[chan][samplenum], i_param[chan][REV]);

			//
			//Try to recover from a file read error
			//
			if (g_error & (FILE_READ_FAIL_1 << chan)) {
				res = reload_sample_file(&fil[chan][samplenum], s_sample);
				if (res != FR_OK) {
					g_error |= FILE_OPEN_FAIL;
					play_state[chan] = SILENT;
					return;
				}

				res = create_linkmap(&fil[chan][samplenum], chan, samplenum);
				if (res != FR_OK) {
					g_error |= FILE_CANNOT_CREATE_CLTBL;
					f_close(&fil[chan][samplenum]);
					play_state[chan] = SILENT;
					return;
				}

				//clear the error flag
				g_error &= ~(FILE_READ_FAIL_1 << chan);
			} else //If no file read error... [?? FixMe: does this logic make sense for clearing is_buffered_to_file_end ???]
			{
				if ((!i_param[chan][REV] && (sample_file_curpos[chan][samplenum] < s_sample->inst_end)) ||
					(i_param[chan][REV] && (sample_file_curpos[chan][samplenum] > s_sample->inst_start)))
					is_buffered_to_file_end[chan][samplenum] = 0;
			}

			//
			// Calculate the amount to pre-buffer before we play:
			//
			pb_adjustment = f_param[chan][PITCH] * (float)s_sample->sampleRate / global_params.f_record_sample_rate;

			//Calculate how many bytes we need to pre-load in our buffer
			//
			//Note of interest: blockAlign already includes numChannels, so we essentially square it in the calc below.
			//The reason is that we plow through the bytes in play_buff twice as fast if it's stereo,
			//and since it takes twice as long to load stereo data from the sd card,
			//we have to preload four times as much data (2^2) vs (1^1)
			//
			pre_buff_size = (uint32_t)((float)(BASE_BUFFER_THRESHOLD * s_sample->blockAlign * s_sample->numChannels) *
									   pb_adjustment);
			active_buff_size = pre_buff_size * 4;

			if (active_buff_size >
				((play_buff[chan][samplenum]->size * 7) / 10)) //limit amount of buffering ahead to 70% of buffer size
				active_buff_size = ((play_buff[chan][samplenum]->size * 7) / 10);

			if (!is_buffered_to_file_end[chan][samplenum] &&
				((play_state[chan] == PREBUFFERING && (play_buff_bufferedamt[chan][samplenum] < pre_buff_size)) ||
				 (play_state[chan] != PREBUFFERING && (play_buff_bufferedamt[chan][samplenum] < active_buff_size))))
			{
				if (sample_file_curpos[chan][samplenum] > s_sample->sampleSize) {
					//We read too much data somehow
					//TODO: When does this happen? sample_file_curpos has not changed recently...
					g_error |= FILE_WAVEFORMATERR;
					play_state[chan] = SILENT;
					start_playing(chan);

				} else if (sample_file_curpos[chan][samplenum] > s_sample->inst_end) {
					is_buffered_to_file_end[chan][samplenum] = 1;
				}

				else
				{
					//
					// Forward reading:
					//
					if (i_param[chan][REV] == 0) {
						rd = s_sample->inst_end - sample_file_curpos[chan][samplenum];

						if (rd > READ_BLOCK_SIZE)
							rd = READ_BLOCK_SIZE;

						res = f_read(&fil[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, &br);
						if (res != FR_OK) {
							g_error |= FILE_READ_FAIL_1 << chan;
							is_buffered_to_file_end[chan][samplenum] =
								1; //FixMe: Do we really want to set this in case of disk error? We don't when reversing.
						}

						if (br < rd) {
							g_error |=
								FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read
							is_buffered_to_file_end[chan][samplenum] = 1;
						}

						sample_file_curpos[chan][samplenum] = f_tell(&fil[chan][samplenum]) - s_sample->startOfData;

						if (sample_file_curpos[chan][samplenum] >= s_sample->inst_end) {
							is_buffered_to_file_end[chan][samplenum] = 1;
						}

					} else {
						//
						// Reverse reading:
						//
						if (sample_file_curpos[chan][samplenum] > s_sample->inst_start)
							rd = sample_file_curpos[chan][samplenum] - s_sample->inst_start;
						else
							rd = 0;

						if (rd >= READ_BLOCK_SIZE) {

							//Jump back a block
							rd = READ_BLOCK_SIZE;

							t_fptr = f_tell(&fil[chan][samplenum]);
							res = f_lseek(&fil[chan][samplenum], t_fptr - READ_BLOCK_SIZE);
							if (res || (f_tell(&fil[chan][samplenum]) != (t_fptr - READ_BLOCK_SIZE)))
								g_error |= LSEEK_FPTR_MISMATCH;

							sample_file_curpos[chan][samplenum] = f_tell(&fil[chan][samplenum]) - s_sample->startOfData;

						} else {
							//rd < READ_BLOCK_SIZE: read the first block (which is the last to be read, since we're reversing)
							//to-do:
							//align rd to 24

							//Jump to the beginning
							sample_file_curpos[chan][samplenum] = s_sample->inst_start;
							res = SET_FILE_POS(chan, banknum, samplenum);
							if (res != FR_OK)
								g_error |= FILE_SEEK_FAIL;

							is_buffered_to_file_end[chan][samplenum] = 1;
						}

						//Read one block forward
						t_fptr = f_tell(&fil[chan][samplenum]);
						res = f_read(&fil[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, &br);
						if (res != FR_OK)
							g_error |= FILE_READ_FAIL_1 << chan;

						if (br < rd)
							g_error |= FILE_UNEXPECTEDEOF;

						//Jump backwards to where we started reading
						res = f_lseek(&fil[chan][samplenum], t_fptr);
						if (res != FR_OK)
							g_error |= FILE_SEEK_FAIL;
						if (f_tell(&fil[chan][samplenum]) != t_fptr)
							g_error |= LSEEK_FPTR_MISMATCH;
					}

					//Write temporary buffer to play_buff[]->in
					if (res != FR_OK)
						g_error |= FILE_READ_FAIL_1 << chan;
					else {
						//Jump back in play_buff by the amount just read (re-sized from file addresses to buffer address)
						if (i_param[chan][REV])
							CB_offset_in_address(play_buff[chan][samplenum], (rd * 2) / s_sample->sampleByteSize, 1);

						err = 0;

						//
						// Write raw file data (tmp_buff_u32) into buffer (play_buff)
						//

						// 16 bit
						if (s_sample->sampleByteSize == 2)
							err = memory_write_16as16(play_buff[chan][samplenum], tmp_buff_u32, rd >> 2, 0);

						//24bit (rd must be a multiple of 3)
						else if (s_sample->sampleByteSize == 3)
							err = memory_write_24as16(play_buff[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, 0);

						//8bit
						else if (s_sample->sampleByteSize == 1)
							err = memory_write_8as16(play_buff[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, 0);

						//32-bit float (rd must be a multiple of 4)
						else if (s_sample->sampleByteSize == 4 && s_sample->PCM == 3)
							err = memory_write_32fas16(play_buff[chan][samplenum], (float *)tmp_buff_u32, rd >> 2, 0);

						//32-bit int rd must be a multiple of 4
						else if (s_sample->sampleByteSize == 4 && s_sample->PCM == 1)
							err = memory_write_32ias16(play_buff[chan][samplenum], (uint8_t *)tmp_buff_u32, rd, 0);

						//Update the cache addresses
						if (i_param[chan][REV]) {
							//Ignore head crossing error if we are reversing and ended up with in==out (that's normal for the first reading)
							if (err && (play_buff[chan][samplenum]->in == play_buff[chan][samplenum]->out))
								err = 0;

							//
							//Jump back again in play_buff by the amount just read (re-sized from file addresses to buffer address)
							//This ensures play_buff[]->in points to the buffer seam
							//
							CB_offset_in_address(play_buff[chan][samplenum], (rd * 2) / s_sample->sampleByteSize, 1);

							cache_low[chan][samplenum] = sample_file_curpos[chan][samplenum];
							cache_map_pt[chan][samplenum] = play_buff[chan][samplenum]->in;

							if ((cache_high[chan][samplenum] - cache_low[chan][samplenum]) >
								cache_size[chan][samplenum])
								cache_high[chan][samplenum] = cache_low[chan][samplenum] + cache_size[chan][samplenum];
						} else {

							cache_high[chan][samplenum] = sample_file_curpos[chan][samplenum];

							if ((cache_high[chan][samplenum] - cache_low[chan][samplenum]) >
								cache_size[chan][samplenum])
							{
								cache_map_pt[chan][samplenum] = play_buff[chan][samplenum]->in;
								cache_low[chan][samplenum] = cache_high[chan][samplenum] - cache_size[chan][samplenum];
							}
						}

						if (err)
							g_error |= READ_BUFF1_OVERRUN << chan;
					}
				}
			}

			//Check if we've prebuffered enough to start playing
			if ((is_buffered_to_file_end[chan][samplenum] || play_buff_bufferedamt[chan][samplenum] >= pre_buff_size) &&
				play_state[chan] == PREBUFFERING)
			{
				env_level[chan] = 0.f;
				if (f_param[chan][LENGTH] <= 0.5f)
					play_state[chan] = i_param[chan][REV] ? PLAYING_PERC : PERC_FADEUP;
				else
					play_state[chan] = PLAY_FADEUP;
			}

		} //play_state != SILENT, FADEDOWN
	}	  //for (chan)
}

void play_audio_from_buffer(int32_t *outL, int32_t *outR, uint8_t chan) {
	uint16_t i;
	float env;
	uint32_t t_u32;
	uint8_t t_flag;

	//Resampling:
	float rs;

	float gain;
	float play_time;

	// Fill buffer with silence and return if we're not playing
	if (play_state[chan] == PREBUFFERING || play_state[chan] == SILENT) {
		for (i = 0; i < HT16_CHAN_BUFF_LEN; i++) {
			outL[i] = 0;
			outR[i] = 0;
		}
		return;
	}

	float length = f_param[chan][LENGTH];
	uint8_t samplenum = sample_num_now_playing[chan];
	uint8_t banknum = sample_bank_now_playing[chan];
	Sample *s_sample = &(samples[banknum][samplenum]);

	//Calculate our actual resampling rate, based on the sample rate of the file being played

	if (s_sample->sampleRate == global_params.record_sample_rate)
		rs = f_param[chan][PITCH];
	else
		rs = f_param[chan][PITCH] * ((float)s_sample->sampleRate / global_params.f_record_sample_rate);

	// FixMe: Consider moving this to a new function that's called after play_audio_buffer()
	// See if we've played enough samples and should start fading down to stop playback
	//
	if (play_state[chan] == PLAYING || play_state[chan] == PLAY_FADEUP || play_state[chan] == PLAYING_PERC ||
		play_state[chan] == PERC_FADEUP)
	{
		// Amount play_buff[]->out changes with each audio block sent to the codec
		uint32_t resampled_buffer_size = calc_resampled_buffer_size(samplenum, banknum, rs);

		// Amount an imaginary pointer in the sample file would move with each audio block sent to the codec
		int32_t resampled_cache_size = calc_resampled_cache_size(samplenum, banknum, resampled_buffer_size);

		// Amount in the sample file we have remaining before we hit sample_file_endpos
		int32_t dist_to_end = calc_dist_to_end(chan, samplenum, banknum);

		// See if we are about to surpass the calculated position in the file where we should end our sample
		// We must start fading down at a point that depends on how long it takes to fade down

		uint32_t fadedown_blocks = calc_perc_fadedown_blocks(length) + 1;
		uint32_t fadedown_state = REV_PERC_FADEDOWN;

		if (dist_to_end < (resampled_cache_size * fadedown_blocks)) {
			play_state[chan] = fadedown_state;
			if (play_state[chan] != PLAYING_PERC)
				flags[ChangePlaytoPerc1 + chan] = 0;
		} else {
			//Check if we are about to hit buffer underrun
			play_buff_bufferedamt[chan][samplenum] = CB_distance(play_buff[chan][samplenum], i_param[chan][REV]);

			if (!is_buffered_to_file_end[chan][samplenum] &&
				play_buff_bufferedamt[chan][samplenum] <= resampled_buffer_size)
			{
				//buffer underrun: tried to read too much out. Try to recover!
				g_error |= READ_BUFF1_UNDERRUN << chan;
				check_errors();
				play_state[chan] = PREBUFFERING;
			}
		}
	}

	//
	//Resample data read from the play_buff and store into out[]
	//

	if (global_mode[STEREO_MODE]) {
		if ((rs * s_sample->numChannels) > MAX_RS)
			rs = MAX_RS / (float)s_sample->numChannels;

		if (s_sample->numChannels == 2) {
			t_u32 = play_buff[chan][samplenum]->out;
			t_flag = flags[PlayBuff1_Discontinuity + chan];
			resample_read16_left(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 4, chan, outL);

			play_buff[chan][samplenum]->out = t_u32;
			flags[PlayBuff1_Discontinuity + chan] = t_flag;
			resample_read16_right(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 4, chan, outR);
		} else {
			//MONO: read left channel and copy to right
			resample_read16_left(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 2, chan, outL);
			for (i = 0; i < HT16_CHAN_BUFF_LEN; i++)
				outR[i] = outL[i];
		}
	} else { //not STEREO_MODE:
		if (rs > MAX_RS)
			rs = MAX_RS;

		if (s_sample->numChannels == 2)
			resample_read16_avg(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 4, chan, outL);
		else
			resample_read16_left(rs, play_buff[chan][samplenum], HT16_CHAN_BUFF_LEN, 2, chan, outL);
	}

	apply_envelopes(outL, outR, chan);
}

// Linear fade of stereo data in outL and outR
// Gain is a fixed gain to apply to all samples
// Set rate to < 0 to fade down, > 0 to fade up
// Returns amplitude applied to the last sample
// Note: this increments amplitude before applying to the first sample
static float fade(int32_t *outL, int32_t *outR, float gain, float starting_amp, float rate) {
	float amp = starting_amp;
	uint32_t completed = 0;

	for (int i = 0; i < HT16_CHAN_BUFF_LEN; i++) {
		amp += rate;
		if (amp >= 1.0f)
			amp = 1.0f;
		if (amp <= 0.f)
			amp = 0.f;
		outL[i] = (float)outL[i] * amp * gain;
		outR[i] = (float)outR[i] * amp * gain;
		outL[i] = _SSAT16(outL[i]);
		outR[i] = _SSAT16(outR[i]);
	}
	return amp;
}

static void apply_gain(int32_t *outL, int32_t *outR, float gain) {
	for (int i = 0; i < HT16_CHAN_BUFF_LEN; i++) {
		outL[i] = (float)outL[i] * gain;
		outR[i] = (float)outR[i] * gain;
		outL[i] = _SSAT16(outL[i]);
		outR[i] = _SSAT16(outR[i]);
	}
}

static void apply_envelopes(int32_t *outL, int32_t *outR, uint8_t chan) {
	int i;
	float env;

	uint8_t samplenum = sample_num_now_playing[chan];
	uint8_t banknum = sample_bank_now_playing[chan];
	Sample *s_sample = &(samples[banknum][samplenum]);

	float length = f_param[chan][LENGTH];
	float gain = s_sample->inst_gain * f_param[chan][VOLUME];
	float rs = (s_sample->sampleRate == global_params.record_sample_rate)
				 ? f_param[chan][PITCH]
				 : f_param[chan][PITCH] * ((float)s_sample->sampleRate / global_params.f_record_sample_rate);

	//Update the start/endpos based on the length parameter
	//Update the play_time (used to calculate led flicker and END OUT pulse width
	//ToDo: we should do this in update_params

	float play_time;
	if (i_param[chan][REV]) {
		sample_file_startpos[chan] = calc_stop_point(length, rs, s_sample, sample_file_endpos[chan]);
		play_time = (sample_file_startpos[chan] - sample_file_endpos[chan]) /
					(s_sample->blockAlign * s_sample->sampleRate * f_param[chan][PITCH]);
	} else {
		sample_file_endpos[chan] = calc_stop_point(length, rs, s_sample, sample_file_startpos[chan]);
		play_time = (sample_file_endpos[chan] - sample_file_startpos[chan]) /
					(s_sample->blockAlign * s_sample->sampleRate * f_param[chan][PITCH]);
	}

	const float fast_perc_fade_rate = calc_fast_perc_fade_rate(length);

	// retrig fadedown rate is the faster of perc fade and global non-perc fadedown rate (larger rate == faster fade)
	const float fast_retrig_fade_rate =
		(fast_perc_fade_rate < global_params.fade_down_rate) ? global_params.fade_down_rate : fast_perc_fade_rate;

	switch (play_state[chan]) {

		case (RETRIG_FADEDOWN):
			// DEBUG0_ON;
			env_rate[chan] =
				global_mode[FADEUPDOWN_ENVELOPE] ? fast_retrig_fade_rate : (1.0f / (float)HT16_CHAN_BUFF_LEN);
			env_level[chan] = fade(outL, outR, gain, env_level[chan], -1.f * env_rate[chan]);
			flicker_endout(chan, play_time);

			if (env_level[chan] <= 0.f) {
				env_level[chan] = 0.f;
				//Start playing again unless we faded down because of a play trigger
				//TODO: Does this ever happen?
				if (!flags[Play1TrigDelaying + chan])
					flags[Play1But + chan] = 1;

				play_state[chan] = SILENT;
			}
			break;

		case (PLAY_FADEUP):
			if (global_mode[FADEUPDOWN_ENVELOPE]) {
				env_rate[chan] = global_params.fade_up_rate;
				env_level[chan] = fade(outL, outR, gain, env_level[chan], env_rate[chan]);
				if (env_level[chan] >= 1.f)
					play_state[chan] = PLAYING;

			} else {
				apply_gain(outL, outR, gain);
				play_state[chan] = PLAYING;
			}
			break;

		case (PERC_FADEUP):
			env_rate[chan] = fast_perc_fade_rate;
			if (global_mode[PERC_ENVELOPE]) {
				env_level[chan] = fade(outL, outR, gain, env_level[chan], env_rate[chan]);
			} else {
				//same rate as fadeing, but don't apply the envelope
				apply_gain(outL, outR, gain);
				env_level[chan] += env_rate[chan] * HT16_CHAN_BUFF_LEN;
			}
			if (env_level[chan] >= 1.f) {
				play_state[chan] = PLAYING_PERC;
				env_level[chan] = 1.f;
			}
			break;

		case (PLAYING):
			apply_gain(outL, outR, gain);
			if (length <= 0.5f)
				flags[ChangePlaytoPerc1 + chan] = 1;
			break;

		case (PLAY_FADEDOWN):
			if (global_mode[FADEUPDOWN_ENVELOPE]) {
				env_rate[chan] = global_params.fade_down_rate;
				env_level[chan] = fade(outL, outR, gain, env_level[chan], -1.f * env_rate[chan]);
			} else {
				apply_gain(outL, outR, gain);
				env_level[chan] = 0.f; //set this so we detect "end of fade" in the next block
			}

			if (env_level[chan] <= 0.f) {
				flicker_endout(chan, play_time);

				//Start playing again if we're looping, unless we faded down because of a play trigger
				if (i_param[chan][LOOPING] && !flags[Play1TrigDelaying + chan])
					flags[Play1But + chan] = 1;

				play_state[chan] = SILENT;
			}
			break;

		case (PLAYING_PERC):
			env_rate[chan] = (i_param[chan][REV] ? 1.f : -1.f) / (length * PERC_ENV_FACTOR);
			if (global_mode[PERC_ENVELOPE]) {
				env_level[chan] = fade(outL, outR, gain, env_level[chan], env_rate[chan]);
			} else {
				// Calculate the envelope in order to keep the timing the same vs. PERC_ENVELOPE enabled,
				// but just don't apply the envelope
				apply_gain(outL, outR, gain);
				env_level[chan] += env_rate[chan] * HT16_CHAN_BUFF_LEN;
			}

			//After fading up to full amplitude in a reverse percussive playback, fade back down to silence:
			if ((env_level[chan] >= 1.0f) && (i_param[chan][REV])) {
				play_state[chan] = REV_PERC_FADEDOWN;
				env_level[chan] = 1.f;
			} else
				check_perc_ending(chan);
			break;

		case (REV_PERC_FADEDOWN):
			// Fade down to silence before going to PAD_SILENCE mode or ending the playback
			// (this prevents a click if the sample data itself doesn't cleanly fade out)
			env_rate[chan] = -1.f * fast_perc_fade_rate;
			if (global_mode[PERC_ENVELOPE]) {
				env_level[chan] = fade(outL, outR, gain, env_level[chan], env_rate[chan]);
			} else {
				apply_gain(outL, outR, gain);
				env_level[chan] += env_rate[chan] * HT16_CHAN_BUFF_LEN;
			}

			// If the end point is the end of the sample data (which happens if the file is very short, or if we're at the end of it)
			// Then pad it with silence so we keep a constant End Out period when looping
			if (env_level[chan] <= 0.f && sample_file_endpos[chan] == s_sample->inst_end)
				play_state[chan] = PAD_SILENCE;
			else
				check_perc_ending(chan);
			break;

		case (PAD_SILENCE):
			for (i = 0; i < HT16_CHAN_BUFF_LEN; i++) {
				outL[i] = 0;
				outR[i] = 0;
			}
			check_perc_ending(chan);
			break;

		default: //PREBUFFERING or SILENT
			break;

	} //switch play_state
}

static void check_perc_ending(uint8_t chan) {
	//End the playback, go to silence, and trigger another play if looping
	if (env_level[chan] <= 0.0f || env_level[chan] >= 1.0f) {
		env_level[chan] = 0.0f;

		flicker_endout(chan, f_param[chan][LENGTH] * 3.0f);

		//Restart loop
		if (i_param[chan][LOOPING] && !flags[Play1TrigDelaying + chan])
			flags[Play1Trig + chan] = 1;

		play_state[chan] = SILENT;
	}
}

void SDIO_read_IRQHandler(void) {
	if (TIM_GetITStatus(SDIO_read_TIM, TIM_IT_Update) != RESET) {

		//Set the flag to tell the main loop to read more from the sdcard
		//We don't want to read_storage_to_buffer() here inside the interrupt because we might be interrupting a current read/write process
		//Keeping sdcard read/writes out of interrupts forces us to complete each read/write before beginning another

		flags[TimeToReadStorage] = 1;

		TIM_ClearITPendingBit(SDIO_read_TIM, TIM_IT_Update);
	}
}

inline FRESULT SET_FILE_POS(uint8_t c, uint8_t b, uint8_t s) {
	FRESULT r = f_lseek(&fil[c][s], samples[b][s].startOfData + sample_file_curpos[c][s]);
	if (fil[c][s].fptr != (samples[b][s].startOfData + sample_file_curpos[c][s]))
		g_error |= LSEEK_FPTR_MISMATCH;
	return r;
}
static inline int32_t _SSAT16(int32_t x) {
	asm("ssat %[dst], #16, %[src]" : [dst] "=r"(x) : [src] "r"(x));
	return x;
}
