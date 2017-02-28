/*
 * sampler.c

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
//#include "audio_sdcard.h"

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

extern int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
//extern int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];



uint8_t SAMPLINGBYTES=2;

uint32_t end_out_ctr[NUM_PLAY_CHAN]={0,0};

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
uint32_t		map_low					[NUM_PLAY_CHAN];
uint32_t		map_high				[NUM_PLAY_CHAN];
uint32_t		map_offset				[NUM_PLAY_CHAN];

//make this a struct
uint32_t 		sample_file_startpos	[NUM_PLAY_CHAN];
uint32_t		sample_file_endpos		[NUM_PLAY_CHAN];
uint32_t 		sample_file_curpos		[NUM_PLAY_CHAN];
uint32_t		sample_file_seam		[NUM_PLAY_CHAN];

uint32_t 		play_buff_bufferedamt	[NUM_PLAY_CHAN];
uint32_t		play_buff_outputted		[NUM_PLAY_CHAN];
enum PlayLoadTriage play_load_triage;




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
	play_buff[0]->seam		= AUDIO_MEM_BASE[0];
	play_buff[0]->stop_pt	= 0;
	play_buff[0]->wrapping	= 0;
	play_buff[0]->filled	= 0;


	play_buff[1]->in 		= AUDIO_MEM_BASE[1];
	play_buff[1]->out		= AUDIO_MEM_BASE[1];
	play_buff[1]->min		= AUDIO_MEM_BASE[1];
	play_buff[1]->max		= AUDIO_MEM_BASE[1] + MEM_SIZE;
	play_buff[1]->size		= MEM_SIZE;
	play_buff[1]->seam		= AUDIO_MEM_BASE[1];
	play_buff[1]->stop_pt	= 0;
	play_buff[1]->filled	= 0;
	play_buff[1]->wrapping	= 0;

	play_buff_outputted[0] 	= 0;
	play_buff_outputted[1]  = 0;

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

	i = next_enabled_bank(0xFF); //find the first enabled bank
	i_param[0][BANK] = i;
	i_param[1][BANK] = i;



}




void toggle_reverse(uint8_t chan)
{
	uint8_t samplenum, banknum;
	uint32_t t;
//	uint32_t numwraps;

	if (play_state[chan] == PLAYING || play_state[chan]==PLAYING_PERC || play_state[chan] == PREBUFFERING)
	{
		//samplenum = i_param[chan][SAMPLE];
		samplenum =sample_num_now_playing[chan];
		banknum = sample_bank_now_playing[chan];

		// Swap sample_file_curpos with sample_file_seam.
		// This is because we already have read the file from the seam position (starting position) to the current position,
		// so to read more data in the opposite direction, we should begin at the original starting position.
		// We also cache the current position as sample_file_seam in case we reverse again


		if (play_state[chan] != PREBUFFERING)
		{



			//Set ->in to the last starting place
			if (play_buff[chan]->seam < 0xFFFFFFF0) //->filled ?
			{
				t 						 	= sample_file_curpos[chan];
				sample_file_curpos[chan]	= sample_file_seam[chan];
				sample_file_seam[chan]		= t;

				t 						= play_buff[chan]->in;
				play_buff[chan]->in	  	= play_buff[chan]->seam;
				play_buff[chan]->seam 	= t;
			}
			else
			{
				sample_file_seam[chan]		= sample_file_curpos[chan];

//				numwraps =  0xFFFFFFFF - play_buff[chan]->seam;
//				numwraps++;
				//???FixMe:
				//sample_file_curpos[chan]= sample_file_seam[chan] - play_buff[chan]->max * numwraps;
				sample_file_curpos[chan] = map_low[chan];
			}

			f_lseek(&fil[chan], samples[banknum][samplenum].startOfData + samples[banknum][samplenum].inst_start + sample_file_curpos[chan]);

		}

		//If we are PREBUFFERING, then re-start the playback
		else
		{
			sample_file_curpos[chan] 	= sample_file_endpos[chan];
			sample_file_seam[chan] 		= sample_file_endpos[chan];

			CB_init(play_buff[chan], i_param[chan][REV]);

		}

		//swap the endpos with the startpos
		t							= sample_file_endpos[chan];
		sample_file_endpos[chan]	= sample_file_startpos[chan];
		sample_file_startpos[chan]	= t;


		//is_buffered_to_file_end[chan] = 0;
	}

	if (i_param[chan][REV])
	{
		i_param[chan][REV] = 0;
	}
	else
	{
		i_param[chan][REV] = 1;

	}

}




//
// toggle_playing(chan)
// Toggles playing or restarts
//
#define SZ_TBL 32
DWORD chan_clmt[NUM_PLAY_CHAN][SZ_TBL];

void toggle_playing(uint8_t chan)
{
	uint8_t samplenum, banknum;
	FRESULT res;
	uint32_t fwd_stop_point, tt;


	//If we can restart without reading from sdcard
//	if (can_restart[chan])
//	{
//		if (!i_param[chan][REV]) play_buff[chan]->out = play_buff[chan]->min;
//		else					 play_buff[chan]->out = play_buff[chan]->max;
//
//	}


	//Start playing, or re-start if we have a short length
	if (play_state[chan]==SILENT || play_state[chan]==PLAY_FADEDOWN || play_state[chan]==RETRIG_FADEDOWN )
	{



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

			sample_num_now_playing[chan] = samplenum;
			sample_bank_now_playing[chan] = i_param[chan][BANK];

			map_low[chan] = 0;
			map_high[chan] = 0;
			map_offset[chan] = 0;

		}

		//Determine starting address
		sample_file_startpos[chan] = calc_start_point(f_param[chan][START], &samples[banknum][samplenum]);

		//Determine ending address
		if (i_param[chan][REV])
		{
			//start at the "end" (fwd_stop_point) and end at the "beginning" (STARTPOS)

			sample_file_endpos[chan] = sample_file_startpos[chan] + READ_BLOCK_SIZE;

			calc_stop_points(f_param[chan][LENGTH], &samples[banknum][samplenum], sample_file_endpos[chan], &fwd_stop_point, &tt);
			sample_file_startpos[chan] = fwd_stop_point;

//			sample_file_endpos[chan] = startpos32 + READ_BLOCK_SIZE;
//			startpos32 = fwd_stop_point + READ_BLOCK_SIZE;

//			if (startpos32 > samples[banknum][ samplenum ].sampleSize)
//				startpos32 = samples[banknum][ samplenum ].sampleSize;

		}
		else
		{
			calc_stop_points(f_param[chan][LENGTH], &samples[banknum][samplenum], sample_file_startpos[chan], &fwd_stop_point, &tt);
			sample_file_endpos[chan] = fwd_stop_point;
		}


		res = f_lseek(&fil[chan], samples[banknum][samplenum].startOfData + samples[banknum][samplenum].inst_start + sample_file_startpos[chan]);
		f_sync(&fil[chan]);

		if (res != FR_OK) g_error |= FILE_SEEK_FAIL;

		else
		{
			// Set up parameters to begin playing

			// Jump back if the requested startpos is already buffered
			if ((map_low[chan] <= sample_file_startpos[chan]) && (sample_file_startpos[chan] < map_high[chan]))
			{

				play_buff[chan]->out = sample_file_startpos[chan] - map_offset[chan];

				while (play_buff[chan]->out > play_buff[chan]->size)
					play_buff[chan]->out -= play_buff[chan]->size;

				play_buff[chan]->out += play_buff[chan]->min;

				play_state[chan]=PLAY_FADEUP;//change to PLAY_FADEUP


			}
			else //...otherwise, start buffering from scratch
			{
				CB_init(play_buff[chan], i_param[chan][REV]);

				sample_file_curpos[chan] 	= sample_file_startpos[chan];
				sample_file_seam[chan] 		= sample_file_startpos[chan];

				map_low[chan] 				= sample_file_startpos[chan];
				map_high[chan] 				= sample_file_startpos[chan];
				map_offset[chan] 			= sample_file_startpos[chan];

				is_buffered_to_file_end[chan] = 0;

				play_state[chan]=PREBUFFERING;

			}

			if (i_param[chan][REV])	decay_amp_i[chan]=0.0;
			else					decay_amp_i[chan]=1.0;
			decay_inc[chan]=0.0;

			play_led_state[chan]=1;

			play_buff_outputted[chan] = 0;

			if (global_mode[MONITOR_RECORDING])
			{
				global_mode[MONITOR_RECORDING] = 0;
			}
		}

	}

	else if (play_state[chan]==PLAYING_PERC || (play_state[chan]==PLAYING && f_param[chan][LENGTH] <= 0.98))
	{
		play_state[chan]=RETRIG_FADEDOWN;
		play_led_state[chan]=0;

	}

	//Stop it if we're playing a full sample
	else if (play_state[chan]==PLAYING && f_param[chan][LENGTH] > 0.98)
	{
		play_state[chan]=PLAY_FADEDOWN;
		play_led_state[chan]=0;
	}
}


uint32_t calc_start_point(float start_param, Sample *sample)
{
	uint32_t align;

	if (sample->blockAlign == 4)		align = 0xFFFFFFFC;
	else if (sample->blockAlign == 2)	align = 0xFFFFFFFC; //was E? but it clicks if we align to 2 not 4, even if our file claims blockAlign = 2
	else if (sample->blockAlign == 8)	align = 0xFFFFFFF8;

	//Determine the starting address
	if (start_param < 0.002)			return(0);
	else if (start_param > 0.998)		return( sample->sampleSize - READ_BLOCK_SIZE*32 ); //just play the last 32 blocks

	else								return( align & ( (uint32_t)(start_param * (float)sample->sampleSize) ) );


}


void calc_stop_points(float length, Sample *sample, uint32_t startpos, uint32_t *fwd_stop_point, uint32_t *rev_stop_point)
{
	uint32_t dist;

	if (length > 0.98)
		dist = sample->sampleSize * (length-0.9) * 10;

	else if (length>=0.5) //>0.5 to <0.9
	{
		dist = (length - 0.4375) * 16.0 * sample->sampleRate;
	}

	else //length < 0.5
	{
//		dist = sample->sampleRate * 4.0;
		dist = 88200.0 * length; //close enough
	}

	*fwd_stop_point = startpos + dist;
	if (*fwd_stop_point > sample->sampleSize)
		*fwd_stop_point = sample->sampleSize;

	if (startpos > dist)
		*rev_stop_point = startpos - dist;
	else
		*rev_stop_point = 0;


	if (sample->blockAlign == 4)			*fwd_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 2)	*fwd_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 8)	*fwd_stop_point &= 0xFFFFFFF8;

	if (sample->blockAlign == 4)			*rev_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 2)	*rev_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 8)	*rev_stop_point &= 0xFFFFFFF8;

}

uint32_t calc_play_length(float length, Sample *sample)
{
	uint32_t dist;

	if (length > 0.98)
		dist = sample->sampleSize * (length-0.9) * 10;

	else if (length>=0.5) //>0.5 to <0.9
	{
		dist = (length - 0.4375) * 16.0 * sample->sampleRate;
	}

	else //length < 0.5
	{
//		dist = sample->sampleRate * 4.0;
		dist = 88200.0 * length; //close enough
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
		//make map_* = 0

		if (samples[ i_param[0][BANK] ][ i_param[0][SAMPLE] ].filename[0] == 0) //no file: fadedown or remain silent
		{
			if (play_state[0] != SILENT && play_state[0]!=PREBUFFERING)
				play_state[0] = PLAY_FADEDOWN;
			else
				play_state[0] = SILENT;

			if (global_mode[ASSIGN_MODE])
				flags[AssigningEmptySample] = 1;

		}
		else
		{
			if (play_state[0] == SILENT && i_param[0][LOOPING])
				flags[Play1Trig]=1;

			if (play_state[0] != SILENT && play_state[0]!=PREBUFFERING)
				play_state[0] = PLAY_FADEDOWN;

			if (global_mode[ASSIGN_MODE])
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
				flags[Play2Trig]=1;

			if (play_state[1] != SILENT && play_state[1]!=PREBUFFERING)
				play_state[1] = PLAY_FADEDOWN;

		}
	}
}


void read_storage_to_buffer(void)
{
	uint8_t chan=0;
	uint32_t err;
	int16_t t_buff16[READ_BLOCK_SIZE]; //shouldn't this be >>1 ?

	FRESULT res;
	uint32_t br;
	uint32_t rd;

	//convenience variables
	uint8_t samplenum, banknum;
	uint32_t buffer_lead;
//	uint32_t fwd_stop_point, rev_stop_point;
	FSIZE_t t_fptr;


	check_change_sample();


	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{
		if (play_state[chan] == SILENT)
		{
			check_change_bank(chan);
		}

		if (play_state[chan] != SILENT && play_state[chan] != PLAY_FADEDOWN && play_state[chan] != RETRIG_FADEDOWN)
		{
			// Check if we need to buffer ahead more

			//We should buffer more if:
			//     play_buff[]->out is approaching play_buff[]->in (buffer_lead)
			//     OR the circular buffer is not full (play_buff[]->filled)
			// Unless the entire file has been read from startpos to endpos (buffered_to_fileend)

			buffer_lead = CB_distance(play_buff[chan], i_param[chan][REV]);

			if (!is_buffered_to_file_end[chan] && (!play_buff[chan]->filled || (buffer_lead < PRE_BUFF_SIZE))) //FixMe: should be PRE_BUFF_SIZE * blockAlign
			{
				samplenum = sample_num_now_playing[chan];
				banknum = sample_bank_now_playing[chan];

				if (sample_file_curpos[chan] > samples[banknum][ samplenum ].sampleSize) //we read too much data somehow //When does this happen? sample_file_curpos has not changed recently...
					g_error |= FILE_WAVEFORMATERR;

				else
				{

					//Read from file forward
					if (i_param[chan][REV]==0)
					{
						rd = samples[banknum][ samplenum ].sampleSize -  sample_file_curpos[chan];

						if (rd > READ_BLOCK_SIZE) rd = READ_BLOCK_SIZE;

				DEBUG1_ON;
						res = f_read(&fil[chan], t_buff16, rd, &br);
						f_sync(&fil[chan]);
				DEBUG1_OFF;

						if (br < rd)
							g_error |= FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read

						sample_file_curpos[chan] += br;

						err = samples[banknum][ samplenum ].sampleSize;

						if (sample_file_curpos[chan] >= samples[banknum][ samplenum ].sampleSize)
						{
							sample_file_curpos[chan] = samples[banknum][ samplenum ].sampleSize;
							is_buffered_to_file_end[chan] = 1;
						}

						map_high[chan] = sample_file_curpos[chan];
						if (play_buff[chan]->filled) map_low[chan] += br;

					}

					//Read from file Reverse
					else
					{
						rd = sample_file_curpos[chan];
						if (rd > READ_BLOCK_SIZE)
						{
							//Jump back a block
				DEBUG2_ON;
							f_lseek(&fil[chan], rd - READ_BLOCK_SIZE);
				DEBUG2_OFF;
							rd = READ_BLOCK_SIZE;
							sample_file_curpos[chan] -= READ_BLOCK_SIZE;

						}
						else
						{
							//Jump to the beginning
							f_lseek(&fil[chan], 0);
							sample_file_curpos[chan] = 0;
							is_buffered_to_file_end[chan] = 1;
						}

						map_low[chan] = sample_file_curpos[chan];
						if (play_buff[chan]->filled)
						{
							if (map_high[chan]>br)	map_high[chan] -= br;
							else 					map_high[chan] = 0;
						}

			DEBUG1_ON;
						//Read one block forward
						t_fptr=f_tell(&fil[chan]);
						res = f_read(&fil[chan], t_buff16, rd, &br);
						f_sync(&fil[chan]);
			DEBUG1_OFF;

						if (res != FR_OK)	g_error |= FILE_READ_FAIL;
						if (br < rd)		g_error |= FILE_UNEXPECTEDEOF;
			DEBUG2_ON;
						//Jump backwards to where we started reading
						fil[chan].fptr = t_fptr;
						//res = f_lseek(&fil[chan], f_tell(&fil[chan]) - br);
						f_sync(&fil[chan]);
			DEBUG2_OFF;

						if (res != FR_OK)	g_error |= FILE_SEEK_FAIL;

					}


					//Write temporary buffer to play_buff[]->in
					if (res != FR_OK) 		g_error |= FILE_READ_FAIL;
					else
					{
						//Todo: avoid using a tbuff16 and copying it, rewrite f_read so that it writes directly to play_buff[] in SDRAM
						err = memory_write16_cb(play_buff[chan], t_buff16, rd>>1, 0);

						//Jump back two blocks if we're reversing
						if (i_param[chan][REV])
							CB_offset_in_address(play_buff[chan], READ_BLOCK_SIZE*2, 1);

						// Check if ->in wrapped around past min/max, in which case we can no longer
						// jump back to the seam if we hit reverse
						// Also mark play_buff as filled
						if (play_buff[chan]->wrapping)
						{
							play_buff[chan]->filled = 1;
							play_buff[chan]->seam = 0xFFFFFFFF;
						}

						if (err)
						{
							DEBUG3_ON;
							//g_error |= READ_BUFF1_OVERRUN*(1+chan);
						} else
							DEBUG3_OFF;

					}

				}

			}

			//Check if we've prebuffered enough to start playing
			if ((is_buffered_to_file_end[chan] || buffer_lead >= PRE_BUFF_SIZE) && play_state[chan] == PREBUFFERING)
			{

				if (f_param[chan][LENGTH] < 0.5 && i_param[chan][REV])
					play_state[chan] = PLAYING_PERC;
				else
					play_state[chan] = PLAY_FADEUP;

			}

		} //play_state != SILENT, FADEDOWN
	} //for (chan)

}

void play_audio_from_buffer(int32_t *out, uint8_t chan)
{
	uint16_t i;
	uint32_t play_length;
	uint32_t t_out;
	int32_t	t_i32;

	//Resampling:
	static float resampling_fpos[NUM_PLAY_CHAN]={0.0f, 0.0f};
	float rs;
	enum Stereo_Modes stereomode;

	//convenience variables
	float length;
	uint32_t resampled_buffer_size;
	float gain;
	uint8_t samplenum, banknum;



	samplenum = sample_num_now_playing[chan];
	banknum = sample_bank_now_playing[chan];

	// Fill buffer with silence
	if (play_state[chan] == PREBUFFERING || play_state[chan] == SILENT)
	{
		 for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
			 out[i]=0;

	}
	else
	{
		//Read from SDRAM into out[]

		//Re-sampling rate (0.0 < rs <= 4.0)
		rs = f_param[chan][PITCH];

		//Stereo Mode
		if (samples[banknum][samplenum].numChannels == 1)
			stereomode = MONO;

		else if	(samples[banknum][samplenum].numChannels == 2)
		{
			if (   i_param[chan][STEREO_MODE] == STEREO_LEFT
				|| i_param[chan][STEREO_MODE] == STEREO_RIGHT )
				stereomode = STEREO_LEFT+chan; //L or R

			else
				stereomode = STEREO_SUM; //mono or sum
		}
		else
		{
			g_error |= FILE_WAVEFORMATERR; //invalid number of channels
			return;
		}


		//Resample data read from the play_buff and store into out[]
		t_out = play_buff[chan]->out;

		resample_read16(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, stereomode, samples[banknum][samplenum].blockAlign, chan, resampling_fpos, out);

		t_out = CB_distance_points(play_buff[chan]->out, t_out, play_buff[chan]->size, i_param[chan][REV]);
		play_buff_outputted[chan] += t_out;

		//Calculate length and where to stop playing
		length = f_param[chan][LENGTH];
		play_length = calc_play_length(length, &samples[banknum][samplenum]);

		gain = samples[banknum][samplenum].inst_gain;

		 switch (play_state[chan])
		 {

			 case (PLAY_FADEUP):

				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
					out[i] = ((float)out[i] * gain * (float)i / (float)HT16_CHAN_BUFF_LEN);

				play_state[chan]=PLAYING;

				break;


			 case (PLAY_FADEDOWN):
			 case (RETRIG_FADEDOWN):

				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					out[i] = ((float)out[i] * (float)(HT16_CHAN_BUFF_LEN-i) / (float)HT16_CHAN_BUFF_LEN);
					t_i32 = (float)out[i] * gain;
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					out[i] = t_i32;
				}

				play_led_state[chan] = 0;

				end_out_ctr[chan]=35;

				if (play_state[chan]==RETRIG_FADEDOWN || i_param[chan][LOOPING])
					flags[Play1Trig+chan] = 1;

				//else
				play_state[chan] = SILENT;

				break;

			 case (PLAYING):

					for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
					{
						t_i32 = (float)out[i] * gain;
						asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
						out[i] = t_i32;
					}

					if (length>0.5) //Play a longer portion of the sample, and go to PLAY_FADEDOWN when done
					{
						resampled_buffer_size = (uint32_t)((HT16_CHAN_BUFF_LEN * samples[banknum][samplenum].blockAlign) * rs);

						if (!i_param[chan][REV]) 	resampled_buffer_size *= 2;
						else 						resampled_buffer_size += READ_BLOCK_SIZE;

						//Stop playback if we've played enough samples
						if ((play_buff_outputted[chan] + resampled_buffer_size) >= play_length)
							play_state[chan] = PLAY_FADEDOWN;

						else
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

									//play_state[chan] = PREBUFFERING;
									//is_buffered_to_file_end[chan] = 0;

									play_buff[chan]->out = play_buff[chan]->in;

									//if (i_param[chan][REV]) resampled_buffer_size *= 2;

									CB_offset_out_address(play_buff[chan], resampled_buffer_size, !i_param[chan][REV]);
									play_buff[chan]->out &= 0xFFFFFFFC;

								}
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

					if (length>0.02) //fixme: not efficient to have an if inside the for-loop! All we want to do is use decay_amp_i as a duration counter, but not apply an envelope if we have a very short length
						out[i] = ((float)out[i]) * decay_amp_i[chan];

					t_i32 = (float)out[i] * gain;
					asm("ssat %[dst], #16, %[src]" : [dst] "=r" (t_i32) : [src] "r" (t_i32));
					out[i] = t_i32;

				}

				if (decay_amp_i[chan] <= 0.0f || decay_amp_i[chan] >= 1.0f)
				{
					decay_amp_i[chan] = 0.0f;

					end_out_ctr[chan] = 35;
					play_led_state[chan] = 0;

					if (i_param[chan][REV])
						play_state[chan] = PLAY_FADEDOWN;

					else
					{
						if (/*play_state[chan]==RETRIG_FADEDOWN ||*/ i_param[chan][LOOPING])
							flags[Play1Trig+chan] = 1;

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


void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	int32_t out[2][HT16_CHAN_BUFF_LEN];
	uint16_t i;

	//	SDIOSTA =  ((uint32_t *)(0x40012C34));

	//
	// Incoming audio
	//
	record_audio_to_buffer(src);

	//
	// Outgoing audio
	//


	play_audio_from_buffer(out[0], 0);
	play_audio_from_buffer(out[1], 1);


	for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
	{

#ifndef DEBUG_ADC_TO_CODEC

		if (global_mode[CALIBRATE])
		{
			*dst++ = CODEC_DAC_CALIBRATION_DCOFFSET[0];
			*dst++ = 0;

			*dst++ = CODEC_DAC_CALIBRATION_DCOFFSET[1];
			*dst++ = 0;
		}
		else
		{
			if (SAMPLINGBYTES==2)
			{
				if (global_mode[MONITOR_RECORDING])
				{
					*dst++ = (*src++) + out[0][i] + CODEC_DAC_CALIBRATION_DCOFFSET[0];
					*dst++ = *src++;
					*dst++ = (*src++) + out[1][i] + CODEC_DAC_CALIBRATION_DCOFFSET[1];
					*dst++ = *src++;
				}
				else
				{
					//L out
					*dst++ = out[0][i] + CODEC_DAC_CALIBRATION_DCOFFSET[0];
					*dst++ = 0;

					//R out
					*dst++ = out[1][i] + CODEC_DAC_CALIBRATION_DCOFFSET[1];
					*dst++ = 0;
				}
			}
			else
			{
				if (global_mode[MONITOR_RECORDING])
				{
					*dst++ = (*src++) + (int16_t)(out[0][i]>>16) + CODEC_DAC_CALIBRATION_DCOFFSET[0];
					*dst++ = (*src++) + (int16_t)(out[0][i] & 0x0000FF00);
					*dst++ = (*src++) + (int16_t)(out[1][i]>>16) + CODEC_DAC_CALIBRATION_DCOFFSET[1];
					*dst++ = (*src++) + (int16_t)(out[1][i] & 0x0000FF00);
				}
				else
				{
					//L out
					*dst++ = (int16_t)(out[0][i]>>16) + (int16_t)CODEC_DAC_CALIBRATION_DCOFFSET[0];
					*dst++ = (int16_t)(out[0][i] & 0x0000FF00);

					//R out
					*dst++ = (int16_t)(out[1][i]>>16) + (int16_t)CODEC_DAC_CALIBRATION_DCOFFSET[1];
					*dst++ = (int16_t)(out[1][i] & 0x0000FF00);
				}
			}
		}

#else
		*dst++ = potadc_buffer[channel+2]*4;
		*dst++ = 0;

		//*dst++ = cvadc_buffer[channel+4]*4;
		*dst++ = cvadc_buffer[channel+0]*4;
		*dst++ = 0;

#endif
	}


}









//If we're fading, increment the fade position
//If we've cross-faded 100%:
//	-Stop the cross-fade
//	-Set read_addr to the destination
//	-Load the next queued fade (if it exists)
//void increment_read_fade(uint8_t channel)
//{
//
//	if (read_fade_pos[channel]>0.0)
//	{
//		read_fade_pos[channel] += global_param[SLOW_FADE_INCREMENT];
//
//		//If we faded 100%, stop fading by setting read_fade_pos to 0
//		if (read_fade_pos[channel] > 1.0)
//		{
//			read_fade_pos[channel] = 0.0;
//			play_buff[channel]->out = fade_dest_read_addr[channel];
//			flags[PlayBuff1_Discontinuity+channel] = 1;
//
//			//Load the next queued fade (if it exists)
//			if (fade_queued_dest_read_addr[channel])
//			{
//				fade_dest_read_addr[channel] = fade_queued_dest_read_addr[channel];
//				fade_queued_dest_read_addr[channel]=0;
//				read_fade_pos[channel] = global_param[SLOW_FADE_INCREMENT];
//			}
//		}
//	}
//}

