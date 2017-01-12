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

uint8_t 		play_fully_buffered		[NUM_PLAY_CHAN];
enum PlayStates play_state				[NUM_PLAY_CHAN];
uint8_t			sample_num_now_playing	[NUM_PLAY_CHAN];
uint8_t			sample_bank_now_playing	[NUM_PLAY_CHAN];

uint32_t 		sample_file_startpos	[NUM_PLAY_CHAN];
uint32_t		sample_file_endpos		[NUM_PLAY_CHAN];
uint32_t 		sample_file_curpos		[NUM_PLAY_CHAN];
uint32_t		sample_file_seam		[NUM_PLAY_CHAN];

uint32_t 		play_buff_bufferedamt	[NUM_PLAY_CHAN];
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
extern Sample samples[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK];


void audio_buffer_init(void)
{
	uint32_t i;
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

	check_bank_dirs();

	i = next_enabled_bank(0xFF); //find the first enabled bank
	i_param[0][BANK] = i;
	i_param[1][BANK] = i;

	load_bank_from_disk(i, 0); //load bank i into channel 0
	load_bank_from_disk(i, 1); //load bank i into channel 1


}






//
// Load the sample header from the provided file
//
uint8_t load_sample_header(Sample *s_sample, FIL *sample_file)
{
	WaveHeader sample_header;
	FRESULT res;
	uint32_t br;
	uint32_t rd;
	WaveChunk chunk;


	rd = sizeof(WaveHeader);
	res = f_read(sample_file, &sample_header, rd, &br);

	if (res != FR_OK)	{g_error |= FILE_READ_FAIL; return(res);}//file not opened
	else if (br < rd)	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);}//file ended unexpectedly when reading first header
	else if ( !is_valid_wav_format(sample_header) )	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);	}//first header error (not a valid wav file)

	else
	{
		chunk.chunkId = 0;
		rd = sizeof(WaveChunk);

		while (chunk.chunkId != ccDATA)
		{
			res = f_read(sample_file, &chunk, rd, &br);

			if (res != FR_OK)	{g_error |= FILE_READ_FAIL; f_close(sample_file); break;}
			if (br < rd)		{g_error |= FILE_WAVEFORMATERR;	f_close(sample_file); break;}

			//fast-forward to the next chunk
			if (chunk.chunkId != ccDATA)
				f_lseek(sample_file, f_tell(sample_file) + chunk.chunkSize);

			//Set the sampleSize as defined in the chunk
			else
			{
				if(chunk.chunkSize == 0)
				{
					f_close(sample_file);break;
				}
				s_sample->sampleSize = chunk.chunkSize;
				s_sample->sampleByteSize = sample_header.bitsPerSample>>3;
				s_sample->sampleRate = sample_header.sampleRate;
				s_sample->numChannels = sample_header.numChannels;
				s_sample->blockAlign = sample_header.numChannels * sample_header.bitsPerSample>>3;
				s_sample->startOfData = f_tell(sample_file);

				return(FR_OK);

			} //else chunk
		} //while chunk
	}//no file error

	return(FR_INT_ERR);
}

void toggle_reverse(uint8_t chan)
{
	uint8_t samplenum;
	uint32_t t;

	if (play_state[chan] == PLAYING || play_state[chan]==PLAYING_PERC || play_state[chan] == PREBUFFERING)
	{
		samplenum = i_param[chan][SAMPLE];

		//swap the current position with the starting position
		//this is because we already have read the file from the starting position to the current position
		//so to read more data in the opposite direction, beginning at the original starting position
		//we cache the current position as startpos in case we reverse again


		if (play_state[chan] != PREBUFFERING)
		{
			f_lseek(&fil[chan], samples[chan][samplenum].startOfData + sample_file_seam[chan]);

			t 						 	= sample_file_curpos[chan];
			sample_file_curpos[chan]	= sample_file_seam[chan];
			sample_file_seam[chan]		= t;

			//also set the play_buff in ptr to the last starting place
			if (play_buff[chan]->seam != 0xFFFFFFFF)
			{
				t 						= play_buff[chan]->in;
				play_buff[chan]->in	  	= play_buff[chan]->seam;
				play_buff[chan]->seam 	= t;
			}

		}
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


		//play_fully_buffered[chan] = 0;
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
	uint8_t samplenum;
	FRESULT res;
	uint32_t fwd_stop_point, tt;


	//Start playing, or re-start if we have a short length
	if (play_state[chan]==SILENT || play_state[chan]==PLAY_FADEDOWN || play_state[chan]==RETRIG_FADEDOWN )
	{

		///load new bank




		CB_init(play_buff[chan], i_param[chan][REV]);

		play_fully_buffered[chan] = 0;

		samplenum = i_param[chan][SAMPLE];

		if (samples[chan][samplenum].filename[0] == 0)
			return;

		flags[PlayBuff1_Discontinuity+chan] = 1;

		//Check if sample changed
		//if so, close the current file and open the new sample file
		if (sample_num_now_playing[chan] != samplenum || sample_bank_now_playing[chan] != i_param[chan][BANK] || fil[chan].obj.fs==0)
		{
			f_close(&fil[chan]);

			 // 384us up to 1.86ms or more? at -Ofast
			res = f_open(&fil[chan], samples[chan][samplenum].filename, FA_READ);
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

		}

		//Determine starting address
		sample_file_startpos[chan] = calc_start_point(f_param[chan][START], &samples[chan][samplenum]);

		//Determine ending address
		if (i_param[chan][REV])
		{
			//start at the "end" (fwd_stop_point) and end at the "beginning" (STARTPOS)

			sample_file_endpos[chan] = sample_file_startpos[chan] + READ_BLOCK_SIZE;

			calc_stop_points(f_param[chan][LENGTH], &samples[chan][samplenum], sample_file_endpos[chan], &fwd_stop_point, &tt);
			sample_file_startpos[chan] = fwd_stop_point;

//			sample_file_endpos[chan] = startpos32 + READ_BLOCK_SIZE;
//			startpos32 = fwd_stop_point + READ_BLOCK_SIZE;

//			if (startpos32 > samples[chan][ samplenum ].sampleSize)
//				startpos32 = samples[chan][ samplenum ].sampleSize;

		}
		else
		{
			calc_stop_points(f_param[chan][LENGTH], &samples[chan][samplenum], sample_file_startpos[chan], &fwd_stop_point, &tt);
			sample_file_endpos[chan] = fwd_stop_point;
		}


		res = f_lseek(&fil[chan], samples[chan][samplenum].startOfData + sample_file_startpos[chan]);
		f_sync(&fil[chan]);

		if (res != FR_OK) g_error |= FILE_SEEK_FAIL;

		else
		{
			// Set up parameters to begin playing
			sample_file_curpos[chan] 	= sample_file_startpos[chan];
			sample_file_seam[chan] 		= sample_file_startpos[chan];

			decay_amp_i[chan]=1.0;
			decay_inc[chan]=0.0;

			play_led_state[chan]=1;

			play_state[chan]=PREBUFFERING;

			if (global_mode[MONITOR_AUDIO])
			{
				global_mode[MONITOR_AUDIO] = 0;
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

	if (sample->blockAlign == 4)			align = 0xFFFFFFFC;
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

	if (length>=0.5)
	{
		if (length > 0.98)
			dist = sample->sampleSize * (length-0.9) * 10;
		else //>0.5 to <0.9
			dist = (length - 0.4375) * 16.0 * sample->sampleRate;

		*fwd_stop_point = startpos + dist;
		if (*fwd_stop_point > sample->sampleSize)
			*fwd_stop_point = sample->sampleSize;

		if (startpos > dist)
			*rev_stop_point = startpos - dist;
		else
			*rev_stop_point = 0;

	}
	else
	{
		*fwd_stop_point = sample->sampleSize;
		*rev_stop_point = 0;
	}

	if (sample->blockAlign == 4)			*fwd_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 2)	*fwd_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 8)	*fwd_stop_point &= 0xFFFFFFF8;

	if (sample->blockAlign == 4)			*rev_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 2)	*rev_stop_point &= 0xFFFFFFFC;
	else if (sample->blockAlign == 8)	*rev_stop_point &= 0xFFFFFFF8;

}

void check_change_bank(uint8_t chan)
{
	if (flags[PlayBank1Changed + chan*2])
	{
		flags[PlayBank1Changed + chan*2]=0;

		load_bank_from_disk(i_param[chan][BANK], chan);
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
	uint8_t samplenum;
	uint32_t buffer_lead;
	uint32_t fwd_stop_point, rev_stop_point;
	FSIZE_t t_fptr;

	//
	//Reset to beginning of sample if we changed sample or bank
	//ToDo: Move this to a routine that checks for flags, and call it before read_storage_to_buffer() is called
	//

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample1Changed]){
		flags[PlaySample1Changed]=0;

		if (play_state[0] == SILENT || play_state[0]==PREBUFFERING)
			sample_file_curpos[0] = 0;
		else
			play_state[0] = PLAY_FADEDOWN;

	}

	//If user changed the play sample, start playing from the beginning of that slot
	if (flags[PlaySample2Changed]){
		flags[PlaySample2Changed]=0;

		if (play_state[1] == SILENT || play_state[1]==PREBUFFERING)
			sample_file_curpos[1] = 0;
		else
			play_state[1] = PLAY_FADEDOWN;
	}


	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{
		if (play_state[chan] == SILENT)
		{
			check_change_bank(chan);
		}

		if (play_state[chan] != SILENT && play_state[chan] != PLAY_FADEDOWN && play_state[chan] != RETRIG_FADEDOWN)
		{
			// Check if we need to buffer ahead more

			buffer_lead = CB_distance(play_buff[chan], i_param[chan][REV]);
			if (!play_fully_buffered[chan] && (buffer_lead < PRE_BUFF_SIZE)) //FixMe: should be PRE_BUFF_SIZE * blockAlign
			{
				samplenum = sample_num_now_playing[chan];

				// Calculate stop points for sample_file_curpos[chan], based on the LENGTH param

				calc_stop_points(f_param[chan][LENGTH], &samples[chan][samplenum], sample_file_startpos[chan], &fwd_stop_point, &rev_stop_point);

				if (i_param[chan][REV])
					sample_file_endpos[chan] = rev_stop_point;
				else
					sample_file_endpos[chan] = fwd_stop_point;


				if ((!i_param[chan][REV] && sample_file_curpos[chan] >= sample_file_endpos[chan])
				  || (i_param[chan][REV] && sample_file_curpos[chan] <= sample_file_endpos[chan]))
				{
					play_fully_buffered[chan] = 1;
					//f_close(&fil[chan]);
				}

				else if (sample_file_curpos[chan] > samples[chan][ samplenum ].sampleSize) //we read too much data somehow
					g_error |= FILE_WAVEFORMATERR;

				else
				{

					//Read from file forward
					if (i_param[chan][REV]==0)
					{
						rd = samples[chan][ samplenum ].sampleSize -  sample_file_curpos[chan];

						if (rd > READ_BLOCK_SIZE) rd = READ_BLOCK_SIZE;

				DEBUG1_ON;
						res = f_read(&fil[chan], t_buff16, rd, &br);
						f_sync(&fil[chan]);
				DEBUG1_OFF;

						if (br < rd)
							g_error |= FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read

						sample_file_curpos[chan] += br;

						if (sample_file_curpos[chan] > samples[chan][ samplenum ].sampleSize)
						{
							sample_file_curpos[chan] = samples[chan][ samplenum ].sampleSize;
							play_fully_buffered[chan] = 1;
						}

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
							play_fully_buffered[chan] = 1;
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

						if (err)
						{
							DEBUG3_ON;
							//g_error |= READ_BUFF1_OVERRUN*(1+chan);
						}

					}

				}

			}
			else //Otherwise, we've buffered enough
			{
				if (play_state[chan] == PREBUFFERING)
					play_state[chan] = PLAY_FADEUP;
			}

		}
	}

}

void play_audio_from_buffer(int32_t *out, uint8_t chan)
{
	uint16_t i;
	uint8_t samplenum;

//	int16_t t_buff16[HT16_BUFF_LEN>>1]; //stereo

	//Resampling:
	static float resampling_fpos[NUM_PLAY_CHAN]={0.0f, 0.0f};
	float rs;
	enum Stereo_Modes stereomode;

	//convenience variables
	float length;
	uint32_t resampled_buffer_size;


	samplenum = sample_num_now_playing[chan];

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
		if (samples[chan][samplenum].numChannels == 1)
			stereomode = MONO;

		else if	(samples[chan][samplenum].numChannels == 2)
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

		resample_read16(rs, play_buff[chan], HT16_CHAN_BUFF_LEN, stereomode, samples[chan][samplenum].blockAlign, chan, resampling_fpos, out);

		length = f_param[chan][LENGTH];

		 switch (play_state[chan])
		 {

			 case (PLAY_FADEUP):

				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
					out[i] = ((float)out[i] * (float)i / (float)HT16_CHAN_BUFF_LEN);

				play_state[chan]=PLAYING;
				break;


			 case (PLAY_FADEDOWN):
			 case (RETRIG_FADEDOWN):

				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
					out[i] = ((float)out[i] * (float)(HT16_CHAN_BUFF_LEN-i) / (float)HT16_CHAN_BUFF_LEN);

				play_led_state[chan] = 0;

				end_out_ctr[chan]=35;

				if (play_state[chan]==RETRIG_FADEDOWN || i_param[chan][LOOPING])
					flags[Play1Trig+chan] = 1;

				else
					play_state[chan] = SILENT;

				break;

			 case (PLAYING):
					if (length>0.5) //Play a longer portion of the sample, and go to PLAY_FADEDOWN when done
					{

						play_buff_bufferedamt[chan] = CB_distance(play_buff[chan], i_param[chan][REV]);

						//since we count play_buff[]->in from the start when REVersing, we need to offset it by a block
//						if (i_param[chan][REV] && play_buff_bufferedamt[chan] > READ_BLOCK_SIZE)
//							play_buff_bufferedamt[chan] -= READ_BLOCK_SIZE;

						resampled_buffer_size = (uint32_t)((HT16_CHAN_BUFF_LEN * samples[chan][samplenum].blockAlign) * rs);

						if (!i_param[chan][REV]) resampled_buffer_size *= 2;
						else resampled_buffer_size += READ_BLOCK_SIZE;

						if (play_buff_bufferedamt[chan] <= resampled_buffer_size)
						{

							if (!play_fully_buffered[chan])
							{
								g_error |= READ_BUFF1_OVERRUN<<chan;
								check_errors();

								//play_state[chan] = PREBUFFERING;
								//play_fully_buffered[chan] = 0;

								play_buff[chan]->out = play_buff[chan]->in;

								//if (i_param[chan][REV]) resampled_buffer_size *= 2;

								CB_offset_out_address(play_buff[chan], resampled_buffer_size, !i_param[chan][REV]);
								play_buff[chan]->out &= 0xFFFFFFFC;

							}
							else
								play_state[chan] = PLAY_FADEDOWN;

						}
					}
					else
						play_state[chan] = PLAYING_PERC;
			 	 	break;

			 case (PLAYING_PERC):

				play_state[chan] = PLAYING_PERC;

				decay_inc[chan] += 1.0f/((length)*2621440.0f);

				for (i=0;i<HT16_CHAN_BUFF_LEN;i++)
				{
					decay_amp_i[chan] -= decay_inc[chan];
					if (decay_amp_i[chan] < 0.0f) decay_amp_i[chan] = 0.0f;

					out[i] = ((float)out[i]) * decay_amp_i[chan];
				}

				if (decay_amp_i[chan] <= 0.0f)
				{
					decay_amp_i[chan] = 0.0f;

					end_out_ctr[chan] = 35;
					play_led_state[chan] = 0;

					play_state[chan] = SILENT;

					if (play_state[chan]==RETRIG_FADEDOWN || i_param[chan][LOOPING])
						flags[Play1Trig+chan] = 1;
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
				if (global_mode[MONITOR_AUDIO])
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
				if (global_mode[MONITOR_AUDIO])
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

		if (STEREOSW==SWITCH_CENTER) *dst++ = cvadc_buffer[channel+4]*4;
		else if (STEREOSW==SWITCH_LEFT) *dst++ = cvadc_buffer[channel+0]*4;
		else *dst++ = cvadc_buffer[channel+2]*4;
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

