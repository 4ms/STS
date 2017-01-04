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
 * -circular_buffer: uint32_t in, out; uint8_t is_wrapping; uint32_t min, size; (can then do diff_wrap(circ_buff);)... combines AUDIO_MEM_BASE and MEM_SIZE
 * -adc_value: uint16_t raw_val, uint16_t i_smoothed_val, float smoothed_val;
 *
 *
 */
#include "globals.h"
#include "audio_sdram.h"
#include "ff.h"
#include "sampler.h"
#include "audio_util.h"
#include "audio_sdcard.h"

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

#define READ_BLOCK_SIZE 512


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

volatile uint32_t*  SDIOSTA;

//
// Parameters
//
extern float 	f_param[NUM_PLAY_CHAN][NUM_F_PARAMS];
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
//extern uint8_t 	settings[NUM_ALL_CHAN][NUM_CHAN_SETTINGS];

extern float global_param[NUM_GLOBAL_PARAMS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];

extern uint8_t 	flags[NUM_FLAGS];
extern enum g_Errors g_error;

extern uint8_t recording_enabled;
extern uint8_t is_recording;



extern uint8_t play_led_state[NUM_PLAY_CHAN];
//extern uint8_t clip_led_state[NUM_PLAY_CHAN];
extern uint8_t ButLED_state[NUM_RGBBUTTONS];


uint8_t SAMPLINGBYTES=2;

extern int16_t CODEC_DAC_CALIBRATION_DCOFFSET[4];
//extern int16_t CODEC_ADC_CALIBRATION_DCOFFSET[4];

uint32_t end_out_ctr[NUM_PLAY_CHAN]={0,0};

//
// Memory
//
extern const uint32_t AUDIO_MEM_BASE[4];

//
// SDRAM buffer addresses for playing from sdcard
// SD Card --> SDARM @play_buff[]->in ... SDRAM @play_buff[]->out --> Codec
//
CircularBuffer splay_buff[NUM_PLAY_CHAN];
CircularBuffer* play_buff[NUM_PLAY_CHAN];

uint8_t 		play_fully_buffered[NUM_PLAY_CHAN];
enum PlayStates play_state[NUM_PLAY_CHAN]={SILENT,SILENT};
uint8_t			samplenum_now_playing[NUM_PLAY_CHAN];

uint32_t sample_file_startpos[NUM_PLAY_CHAN];
uint32_t sample_file_curpos[NUM_PLAY_CHAN] = {0,0};


//
// SDRAM buffer address for recording to sdcard
// Codec --> SDRAM (@rec_buff_in_addr) .... SDRAM (@rec_buff_out_addr) --> SD Card (@rec_storage_addr)
//
uint32_t	rec_buff_in_addr;
uint32_t	rec_buff_out_addr;

uint32_t	rec_storage_addr=0;


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
Sample samples[NUM_SAMPLES_PER_BANK];




void audio_buffer_init(void)
{
	uint32_t i;
	uint32_t sample_num;
	FIL temp_file;
	FRESULT res;
	DIR dir;
//	FILINFO fno;
	char* path;
	char tname[_MAX_LFN+1];


//	if (MODE_24BIT_JUMPER)
//		SAMPLINGBYTES=4;
//	else
		SAMPLINGBYTES=2;

	memory_clear(0);
	memory_clear(1);
	memory_clear(2);
	memory_clear(3);


	rec_buff_in_addr 		= AUDIO_MEM_BASE[RECCHAN];
	rec_buff_out_addr 		= AUDIO_MEM_BASE[RECCHAN];

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
	samplenum_now_playing[0] = 0xFF;
	samplenum_now_playing[1] = 0xFF;

	for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
	{
		//samples[i].filename="";
		samples[i].sampleSize=0;
	}

	sample_num=0;
	path="/";
	tname[0]=0;
	res = f_opendir(&dir, path);
	if (res==FR_OK)
	{

		while (sample_num<19)
		{
			res = find_next_ext_in_dir(&dir, ".wav", tname);
			if (res!=FR_OK) break;

			str_cpy(samples[sample_num++].filename, tname);
		}

        f_closedir(&dir);
	}
	else
	{
		g_error=CANNOT_OPEN_ROOT_DIR;
	}


	//Load all the sample headers
	for (i=0;i<NUM_SAMPLES_PER_BANK;i++)
	{
		res = f_open(&temp_file, samples[i].filename, FA_READ);
		f_sync(&temp_file);

		if (res==FR_OK)
		{
			res = load_sample_header(i, &temp_file);

//			if (res==FR_OK)
//				clmt[i] = load_file_clmt(&temp_file);

		}

		f_close(&temp_file);
	}

}


//
// not being used
// But we could make it preload the currently selected sample from the current startpos if nothing is being played
// This would allow a trigger to start the sample immediately instead of a latency needed to load it on demand
// of course this does not help if we are modulating sample number or startpos
uint8_t preload_sample(uint32_t samplenum, FIL* sample_file)
{
	FRESULT res;
	uint32_t sz,rd,br;
	int16_t t_buff16[BUFF_LEN];

	//file is open already

	// Seek the start of the sample data
	res = f_lseek(sample_file, samples[samplenum].startOfData);

	// Read it
	sz = samples[ samplenum ].sampleSize;
	if (sz > (256 * PRE_BUFF_SIZE)) sz = 256*PRE_BUFF_SIZE;

	while (sz)
	{
		//Read in 512byte blocks, unless <512 is remaining
		if (sz<512)	rd = sz;
		else		rd = 512;

		res = f_read(sample_file, t_buff16, rd, &br);

		if (res==FR_OK && br==rd)
		{
			//memory_write16()

			sz -= rd;
		}
		else
			return(FR_INT_ERR);
	}

	return(FR_OK);

}


//
// Load the sample header from the provided file
//
uint8_t load_sample_header(uint32_t samplenum, FIL* sample_file)
{
	WaveHeader sample_header;
	FRESULT res;
	uint32_t br;
	uint32_t rd;
	WaveChunk chunk;


	rd = sizeof(WaveHeader);
	res = f_read(sample_file, &sample_header, rd, &br);

	if (res != FR_OK)
	{
		g_error |= FILE_READ_FAIL; //file not opened
		return(res);
	}
	else if (br < rd)
	{
		g_error |= FILE_WAVEFORMATERR; //file ended unexpectedly when reading first header
		f_close(sample_file);
		return(FR_INT_ERR);
	}
	else if ( is_valid_wav_format(sample_header) )
	{
		g_error |= FILE_WAVEFORMATERR;	//first header error (not a valid wav file)
		f_close(sample_file);
		return(FR_INT_ERR);
	}

	else
	{
		chunk.chunkId = 0;
		rd = sizeof(WaveChunk);

		while (chunk.chunkId != ccDATA)
		{
			res = f_read(sample_file, &chunk, rd, &br);

			if (res != FR_OK)
			{
				g_error |= FILE_READ_FAIL;
				f_close(sample_file);
				break;
			}
			if (br < rd)
			{
				g_error |= FILE_WAVEFORMATERR;
				f_close(sample_file);
				break;
			}

			//fast-forward to the next chunk
			if (chunk.chunkId != ccDATA)
				f_lseek(sample_file, f_tell(sample_file) + chunk.chunkSize);

			//Set the sampleSize as defined in the chunk
			else
			{
				samples[samplenum].sampleSize = chunk.chunkSize;
				samples[samplenum].sampleBitSize = sample_header.bitsPerSample;
				samples[samplenum].sampleRate = sample_header.sampleRate;
				samples[samplenum].numChannels = sample_header.numChannels;
				samples[samplenum].blockAlign = sample_header.numChannels * sample_header.bitsPerSample>>3;
				samples[samplenum].startOfData = f_tell(sample_file);

				return(FR_OK);

			} //else chunk
		} //while chunk
	}//no file error

	return(FR_INT_ERR);
}

void toggle_reverse(uint8_t chan)
{
	uint8_t samplenum;
	FRESULT res;
	uint32_t t;

	if (play_state[chan] == PLAYING || play_state[chan]==PLAYING_PERC)
	{
		samplenum = i_param[chan][SAMPLE];

		//swap the current position with the starting position
		//this is because we already have read the file from the starting position to the current position
		//so to read more data in the opposite direction, beginning at the original starting position
		//we cache the current position as startpos in case we reverse again
		f_lseek(&fil[chan], samples[samplenum].startOfData + sample_file_startpos[chan]);
		t 							= sample_file_curpos[chan];
		sample_file_curpos[chan] 	= sample_file_startpos[chan];
		sample_file_startpos[chan]	= t;

		//also set the play_buff in ptr to the last starting place
		if (play_buff[chan]->seam != 0xFFFFFFFF)
		{
			t 						= play_buff[chan]->in;
			play_buff[chan]->in	  	= play_buff[chan]->seam;
			play_buff[chan]->seam 	= t;
		}

		play_fully_buffered[chan] = 0;


	}

	if (i_param[chan][REV])
	{
		ButLED_state[Reverse1ButtonLED + chan] = 0;
		i_param[chan][REV] = 0;
	}
	else
	{
		ButLED_state[Reverse1ButtonLED + chan] = 1;
		i_param[chan][REV] = 1;

	}

}


#define SZ_TBL 256
DWORD clmt[SZ_TBL];

//
// toggle_playing(chan)
// Toggles playing or restarts
//
void toggle_playing(uint8_t chan)
{
	uint8_t samplenum;
	FRESULT res;
	uint32_t startpos32;

	//Start playing, or re-start if we have a short length
	if (play_state[chan]==SILENT || play_state[chan]==PLAY_FADEDOWN || play_state[chan]==RETRIG_FADEDOWN)
	{
		if (i_param[chan][REV])
		{
			play_buff[chan]->wrapping = 0;
			play_buff[chan]->in = play_buff[chan]->max - READ_BLOCK_SIZE;
			play_buff[chan]->out = play_buff[chan]->max;
			play_buff[chan]->seam = play_buff[chan]->max;

		}
		else
		{
			play_buff[chan]->wrapping = 0;
			play_buff[chan]->in = play_buff[chan]->min;
			play_buff[chan]->out = play_buff[chan]->min;
			play_buff[chan]->seam = play_buff[chan]->min;
		}

		play_fully_buffered[chan] = 0;

		samplenum = i_param[chan][SAMPLE];

		if (samples[samplenum].filename[0] == 0)
			return;

		flags[PlayBuff1_Discontinuity+chan] = 1;

		//Check if sample changed
		//if so, close the current file and open the new sample file
		if (samplenum_now_playing[chan] != samplenum || fil[chan].obj.fs==0)
		{
			f_close(&fil[chan]);

			 // 384us up to 1.86ms or more? at -Ofast
			res = f_open(&fil[chan], samples[samplenum].filename, FA_READ);
			f_sync(&fil[chan]);

			if (res != FR_OK)
			{
				g_error |= FILE_OPEN_FAIL;
				samplenum_now_playing[chan] = 0xFF;
				f_close(&fil[chan]);
				return;
			}

			//takes over 3.3ms for a big file
			fil[chan].cltbl = clmt;
			clmt[0] = SZ_TBL;
			res = f_lseek(&fil[chan], CREATE_LINKMAP);
			//Could save time by pre-loading all the clmt when we load the bank
			//Then just do this instead of clmt[0]=SZ_TBL and CREATE_LINKMAP:
			//fil[chan].cltbl = clmt[samplenum];

			if (res != FR_OK)
			{
				g_error |= FILE_CANNOT_CREATE_CLTBL;
				samplenum_now_playing[chan] = 0xFF;
				f_close(&fil[chan]);
				return;
			}

			samplenum_now_playing[chan] = samplenum;
		}

		//Determine the starting address
		if (f_param[chan][START] < 0.002)			startpos32 = 0;
		else if (f_param[chan][START] > 0.999)		startpos32 = samples[samplenum].sampleSize - 512*32; //just play the last 32 blocks
		else										startpos32 = (uint32_t)(f_param[chan][START] * (float)samples[samplenum].sampleSize);


		//If reverse, start from the end of the sample
		if (i_param[chan][REV])
		{
			startpos32 = samples[samplenum].sampleSize - startpos32;
		//	startpos32 -= READ_BLOCK_SIZE;		//Also jump back one extra block because reading happens from the start of a block

		}

		//Align the start address
		if (samples[samplenum].blockAlign == 4)
			startpos32 &= 0xFFFFFFFC;

		else if (samples[samplenum].blockAlign == 2)
			startpos32 &= 0xFFFFFFFE;

		else if (samples[samplenum].blockAlign == 8)
			startpos32 &= 0xFFFFFFF8;
		else
		{
			//handle other blockAligns here
		}

		sample_file_startpos[chan] = startpos32;

		res = f_lseek(&fil[chan], samples[samplenum].startOfData + startpos32);
		f_sync(&fil[chan]);

		if (res != FR_OK)
			g_error |= FILE_SEEK_FAIL;

		else
		{
			// Set up parameters to begin playing
			sample_file_curpos[chan] = startpos32;

			decay_amp_i[chan]=1.0;
			decay_inc[chan]=0.0;

			play_led_state[chan]=1;
			ButLED_state[Play1ButtonLED+chan]=1;

			play_state[chan]=PREBUFFERING;
		}

	}

	else if (play_state[chan]==PLAYING_PERC)
	{
		play_state[chan]=RETRIG_FADEDOWN;
		play_led_state[chan]=0;
		ButLED_state[Play1ButtonLED+chan]=0;

	}

	//Stop it if we're playing a full sample
	else if (play_state[chan]==PLAYING )
	{
		play_state[chan]=PLAY_FADEDOWN;

		play_led_state[chan]=0;
		ButLED_state[Play1ButtonLED+chan]=0;
	}
}



void read_storage_to_buffer(void)
{
	uint8_t chan=0;
	uint32_t err;
	int16_t t_buff16[256*4];

	int32_t buffer_lead;

	FRESULT res;
	uint32_t br;
	uint32_t rd;
	uint8_t samplenum;


	//
	//Reset to beginning of sample if we changed sample or bank
	//ToDo: Move this to a routine that checks for flags, and call it before read_storage_to_buffer() is called
	//
	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank1Changed]){
		flags[PlayBank1Changed]=0;
	}

	//If user changed the play bank, start playing from the beginning of that slot
	if (flags[PlayBank2Changed]){
		flags[PlayBank2Changed]=0;
	}

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

		if (play_state[chan] != SILENT && play_state[chan] != PLAY_FADEDOWN && play_state[chan] != RETRIG_FADEDOWN)
		{

			samplenum = samplenum_now_playing[chan];

			// If our "in" pointer is not far enough ahead of the "out" pointer, then we need to buffer some more:
				//Can't we just do this? i_param[chan][REV] ? !(play_buff[chan]->wrapping) : (play_buff[chan]->wrapping)
			if (i_param[chan][REV])
				buffer_lead = diff_wrap(play_buff[chan]->out, play_buff[chan]->in,  play_buff[chan]->wrapping, MEM_SIZE);
			else
				buffer_lead = diff_wrap(play_buff[chan]->in, play_buff[chan]->out,  play_buff[chan]->wrapping, MEM_SIZE);


			if (buffer_lead < (PRE_BUFF_SIZE*256))
			{
				//Check if we're at the end (or start, if reversing) of the file
				if ((!i_param[chan][REV] && sample_file_curpos[chan] == samples[ samplenum ].sampleSize)
				  || (i_param[chan][REV] && sample_file_curpos[chan] == 0))
				{
					play_fully_buffered[chan] = 1;
					//f_close(&fil[chan]);
				}
				else if (sample_file_curpos[chan] > samples[ samplenum ].sampleSize) //we read too much data somehow
					g_error |= FILE_WAVEFORMATERR;

				else
				{
					//Forward
					if (i_param[chan][REV]==0)
					{
						rd = samples[ samplenum ].sampleSize -  sample_file_curpos[chan];

						if (rd > READ_BLOCK_SIZE) rd = READ_BLOCK_SIZE;

						res = f_read(&fil[chan], t_buff16, rd, &br);
						f_sync(&fil[chan]);

						if (br < rd)
							g_error |= FILE_UNEXPECTEDEOF; //unexpected end of file, but we can continue writing out the data we read

						sample_file_curpos[chan] += br;

						if (sample_file_curpos[chan] > samples[ samplenum ].sampleSize)
						{
							sample_file_curpos[chan] = samples[ samplenum ].sampleSize;
							play_fully_buffered[chan] = 1;
						}


					}

					//Reverse
					else
					{
					//	rd = f_tell(&fil[chan]);
						rd = sample_file_curpos[chan];
						if (rd > READ_BLOCK_SIZE)
						{
							//Jump back a block
							f_lseek(&fil[chan], rd - READ_BLOCK_SIZE);
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

						//Read one block forward
						res = f_read(&fil[chan], t_buff16, rd, &br);
						f_sync(&fil[chan]);

						if (res != FR_OK)
							g_error |= FILE_READ_FAIL;
						if (br < rd)
							g_error |= FILE_UNEXPECTEDEOF;

						//Jump backwards to where we started reading
						res = f_lseek(&fil[chan], f_tell(&fil[chan]) - br);
						f_sync(&fil[chan]);

						if (res != FR_OK)
							g_error |= FILE_SEEK_FAIL;



					}


					if (res != FR_OK)
						g_error |= FILE_READ_FAIL;
					else
					{
						err = memory_write16_cbin(play_buff[chan], t_buff16, rd>>1, 0);

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


void process_audio_block_codec(int16_t *src, int16_t *dst)
{
	int32_t in[2];
	int32_t out[2][BUFF_LEN];
	int32_t dummy;
	uint8_t samplenum;
	enum Stereo_Modes stereomode;

	uint16_t i;
	uint16_t topbyte, bottombyte;
	uint8_t chan;
	uint8_t overrun;

	//Resampling:
	static float resampling_fpos[NUM_PLAY_CHAN]={0.0f, 0.0f};
	float rs;

	float length;
	uint32_t end_playing_addr;
	uint32_t left_to_play;


	SDIOSTA =  ((uint32_t *)(0x40012C34));

	//
	// Incoming audio
	//
	// Dump BUFF_LEN samples of the rx buffer from codec (src) into t_buff
	// Then write t_buff to sdram at rec_buff_in_addr
	//
	if (is_recording && 0)
	{
		for (i=0;i<BUFF_LEN;i++)
		{
			//
			// Split incoming stereo audio into the two channels
			//
			if (SAMPLINGBYTES==2)
			{
				in[0] = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+0]*/;
				dummy=*src++;

				in[1] = (*src++) /*+ CODEC_ADC_CALIBRATION_DCOFFSET[channel+2]*/;
				dummy=*src++;
			}
			else
			{
				topbyte = (uint16_t)(*src++);
				bottombyte = (uint16_t)(*src++);
				in[0] = (topbyte << 16) + (uint16_t)bottombyte;

				topbyte = (uint16_t)(*src++);
				bottombyte = (uint16_t)(*src++);
				in[1] = (topbyte << 16) + (uint16_t)bottombyte;
			}


			//t_buff[i] = in[0];

			overrun = memory_write(&rec_buff_in_addr, RECCHAN, &(in[0]), 1, rec_buff_out_addr, 0);
			if (overrun) break;
		}

		//overrun = memory_write(&rec_buff_in_addr, RECCHAN, t_buff, BUFF_LEN, rec_buff_out_addr, 0);

		if (overrun)
		{
			g_error |= WRITE_BUFF_OVERRUN;
			is_recording=0;
			ButLED_state[RecButtonLED]=0;
		}
	}



	//
	// Outgoing audio
	//
	for (chan=0;chan<NUM_PLAY_CHAN;chan++)
	{
		samplenum = samplenum_now_playing[chan];

		// Fill buffer with silence
		if (play_state[chan] == PREBUFFERING || play_state[chan] == SILENT)
		{
			 for (i=0;i<BUFF_LEN;i++)
				 out[chan][i]=0;
		}
		else
		{
			//Read from SDRAM into out[chan][]

			//Re-sampling rate (0.0 < rs <= 4.0)
			rs = f_param[chan][PITCH];

			//Stereo Mode
			if (samples[samplenum].numChannels == 1)
				stereomode = MONO;

			else if	(samples[samplenum].numChannels == 2)
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
				break;
			}


			//Resample data read from the play_buff and store into out[]

			resample_read16(rs, play_buff[chan], BUFF_LEN, stereomode, samples[samplenum].blockAlign, chan, resampling_fpos, out[chan]);

			length = f_param[chan][LENGTH];

			 switch (play_state[chan])
			 {

				 case (PLAY_FADEUP):

					for (i=0;i<BUFF_LEN;i++)
						out[chan][i] = ((float)out[chan][i] * (float)i / (float)BUFF_LEN);

					play_state[chan]=PLAYING;
					break;


				 case (PLAY_FADEDOWN):
				 case (RETRIG_FADEDOWN):

					for (i=0;i<BUFF_LEN;i++)
						out[chan][i] = ((float)out[chan][i] * (float)(BUFF_LEN-i) / (float)BUFF_LEN);

					play_led_state[chan] = 0;
					ButLED_state[Play1ButtonLED+chan] = 0;

					end_out_ctr[chan]=400;

					if (play_state[chan]==RETRIG_FADEDOWN)
						flags[Play1Trig+chan] = 1;
					else
						play_state[chan]=SILENT;

					break;

				 case (PLAYING):
						if (length>0.5) //Play a longer portion of the sample, and go to PLAY_FADEDOWN when done
						{

							//Stop based on the LENGTH param
//
							//ToDo: Stop based on position of play_buff[chan]->out, if we are fully_buffered and it's going to hit ->seam or ->in
							//
//							if (!i_param[chan][REV])
//							{
//								end_playing_addr = (samples[samplenum].sampleSize * (length-0.5) * 2) + sample_file_startpos[chan];
//								if (end_playing_addr > (samples[samplenum].sampleSize - READ_BLOCK_SIZE))
//									end_playing_addr = samples[samplenum].sampleSize - READ_BLOCK_SIZE;
//
//								if (sample_file_curpos[chan] >= end_playing_addr )
//									play_state[chan] = PLAY_FADEDOWN;
//							}
//							else {
//								if (sample_file_startpos[chan] > (samples[samplenum].sampleSize * (length-0.5) * 2))
//									end_playing_addr = sample_file_startpos[chan] - (samples[samplenum].sampleSize * (length-0.5) * 2);
//								else
//									end_playing_addr = READ_BLOCK_SIZE;
//
//								if (sample_file_curpos[chan] <= end_playing_addr )
//									play_state[chan] = PLAY_FADEDOWN;
//
//							}

							//ToDo: incorporate LENGTH into this comparison
							// and get rid of the above comparisons
							// and probably force the file from not being read once we're fully buffered
							// and detect better when we are!

							//							if (diff_circular(play_buff[chan]->in, play_buff[chan]->out, MEM_SIZE) < (2*256))

							left_to_play = CB_distance(play_buff[chan], i_param[chan][REV]);

							if (play_fully_buffered[chan] &&  (left_to_play <= READ_BLOCK_SIZE))
								play_state[chan] = PLAY_FADEDOWN;
						}
						else
							play_state[chan] = PLAYING_PERC;
				 	 	break;

				 case (PLAYING_PERC):

					//if ((play_state[chan] == PLAYING_PERC) || length<=0.5) //Play a short portion of the sample, with an envelope on the whole thing
					//{
						play_state[chan] = PLAYING_PERC;

						decay_inc[chan] += 1.0f/((length)*2621440.0f);

						for (i=0;i<BUFF_LEN;i++)
						{
							decay_amp_i[chan] -= decay_inc[chan];
							if (decay_amp_i[chan] < 0.0f) decay_amp_i[chan] = 0.0f;

							out[chan][i] = ((float)out[chan][i]) * decay_amp_i[chan];
						}

						if (decay_amp_i[chan] <= 0.0f)
						{
							decay_amp_i[chan] = 0.0f;
							play_state[chan] = SILENT;
							end_out_ctr[chan] = 400;

							play_led_state[chan] = 0;
							ButLED_state[Play1ButtonLED+chan] = 0;
						}


					//}
					break;

				 default:
					 break;

			 }//switch play_state

		} //if play_state

	}

	for (i=0;i<BUFF_LEN;i++)
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

				//L out
				*dst++ = out[0][i] + CODEC_DAC_CALIBRATION_DCOFFSET[0];
				*dst++ = 0;

				//R out
				*dst++ = out[1][i] + CODEC_DAC_CALIBRATION_DCOFFSET[1];
				*dst++ = 0;
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



void toggle_recording(void)
{
	if (is_recording)
	{
		is_recording=0;
		ButLED_state[RecButtonLED]=0;

	} else
	{
		if (recording_enabled)
		{
			rec_storage_addr=0;
			is_recording=1;
			ButLED_state[RecButtonLED]=1;
		}
	}
}


void write_buffer_to_storage(void)
{
	uint32_t err;
	uint32_t start_of_sample_addr;
	uint8_t addr_exceeded;
	//uint32_t i;
	//int32_t t_buff32[BUFF_LEN];
	int16_t t_buff16[BUFF_LEN];


	//If user changed the record sample slot, start recording from the beginning of that slot
	if (flags[RecSampleChanged])
	{
		flags[RecSampleChanged]=0;
		rec_storage_addr = 0;
	}

	//If user changed the record bank, start recording from the beginning of that slot
	if (flags[RecBankChanged])
	{
		flags[RecBankChanged]=0;
		rec_storage_addr = 0;
	}



	// Handle write buffers (transfer SDRAM to SD card)


	if (rec_buff_out_addr != rec_buff_in_addr)
	{

		addr_exceeded = memory_read16(&rec_buff_out_addr, RECCHAN, t_buff16, BUFF_LEN, rec_buff_in_addr, 0);

		// Stop the circular buffer if we wrote it all out to storage
		if (addr_exceeded) rec_buff_out_addr = rec_buff_in_addr;

		start_of_sample_addr = (i_param[REC][SAMPLE] * SAMPLE_SIZE) + (i_param[REC][BANK] * BANK_SIZE);

		//err = write_sdcard((uint16_t *)t_buff16, start_of_sample_addr + rec_storage_addr);

		if (err==0)
		{
			rec_storage_addr++;

			//if (rec_storage_addr>=(BANK_SIZE){
			if (rec_storage_addr>=SAMPLE_SIZE)
			{
				rec_storage_addr=0; //reset and stop at the end of the sample
				is_recording=0;
				ButLED_state[RecButtonLED]=0;
			}
		}
		else
		{
			g_error |= WRITE_SDCARD_ERROR;
			is_recording=0;
			ButLED_state[RecButtonLED]=0;
		}


	}

}





//If we're fading, increment the fade position
//If we've cross-faded 100%:
//	-Stop the cross-fade
//	-Set read_addr to the destination
//	-Load the next queued fade (if it exists)
void increment_read_fade(uint8_t channel)
{

	if (read_fade_pos[channel]>0.0)
	{
		read_fade_pos[channel] += global_param[SLOW_FADE_INCREMENT];

		//If we faded 100%, stop fading by setting read_fade_pos to 0
		if (read_fade_pos[channel] > 1.0)
		{
			read_fade_pos[channel] = 0.0;
			play_buff[channel]->out = fade_dest_read_addr[channel];
			flags[PlayBuff1_Discontinuity+channel] = 1;

			//Load the next queued fade (if it exists)
			if (fade_queued_dest_read_addr[channel])
			{
				fade_dest_read_addr[channel] = fade_queued_dest_read_addr[channel];
				fade_queued_dest_read_addr[channel]=0;
				read_fade_pos[channel] = global_param[SLOW_FADE_INCREMENT];
			}
		}
	}
}

