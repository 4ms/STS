#include "globals.h"
#include "params.h"
#include "ff.h"
#include "sts_filesystem.h"
#include "str_util.h"
#include "sampler.h"
#include "wavefmt.h"
#include "sample_file.h"
#include "dig_pins.h"

extern enum g_Errors g_error;
extern uint8_t	i_param[NUM_ALL_CHAN][NUM_I_PARAMS];
extern uint8_t 	flags[NUM_FLAGS];
extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern FATFS FatFs;

extern enum PlayStates play_state				[NUM_PLAY_CHAN];

Sample samples[MAX_NUM_BANKS][NUM_SAMPLES_PER_BANK];

//uint8_t is_valid
FRESULT reload_sample_file(FIL *fil, Sample *s_sample)
{
	FRESULT res;

	//Try closing and re-opening file
	f_close(fil);
	res = f_open(fil, s_sample->filename, FA_READ);

	//If it fails, try re-mounting the sd card and opening again
	if (res != FR_OK)
	{
		f_close(fil);

		res = reload_sdcard();
		if (res == FR_OK)
			res = f_open(fil, s_sample->filename, FA_READ);

		if (res != FR_OK)
			f_close(fil);

	}

	return (res);
}


//
// Create a fast-lookup table (linkmap)
//
#define SZ_TBL 256
CCMDATA DWORD chan_clmt[NUM_PLAY_CHAN][NUM_SAMPLES_PER_BANK][SZ_TBL];

FRESULT create_linkmap(FIL *fil, uint8_t chan, uint8_t samplenum)
{
	FRESULT res;

	fil->cltbl = chan_clmt[chan][samplenum];
	chan_clmt[chan][samplenum][0] = SZ_TBL;

	res = f_lseek(fil, CREATE_LINKMAP);

	return (res);
}

//
// Load the sample header from the provided file
//
uint8_t load_sample_header(Sample *s_sample, FIL *sample_file)
{
	WaveHeader sample_header;
	WaveFmtChunk fmt_chunk;
	FRESULT res;
	uint32_t br;
	uint32_t rd;
	WaveChunk chunk;
	uint32_t next_chunk_start;

	rd = sizeof(WaveHeader);
	res = f_read(sample_file, &sample_header, rd, &br);

	if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; return(res);}//file not opened
	else if (br < rd)	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);}//file ended unexpectedly when reading first header
	else if ( !is_valid_wav_header(sample_header) )	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);	}//first header error (not a valid wav file)

	else
	{
		//Look for a WaveFmtChunk (which starts off as a chunk)
		chunk.chunkId = 0;
		rd = sizeof(WaveChunk);

		while (chunk.chunkId != ccFMT)
		{
			res = f_read(sample_file, &chunk, rd, &br);

			if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; f_close(sample_file); break;}
			if (br < rd)		{g_error |= FILE_WAVEFORMATERR;	f_close(sample_file); break;}

			//Fix an odd-sized chunk, it should always be even
			if (chunk.chunkSize & 0b1)
				chunk.chunkSize++;

			next_chunk_start = f_tell(sample_file) + chunk.chunkSize;
			//fast-forward to the next chunk
			if (chunk.chunkId != ccFMT)
				f_lseek(sample_file, next_chunk_start);

		}

		if (chunk.chunkId == ccFMT)
		{
			//Go back to beginning of chunk --probably could do this more elegantly by removing fmtID and fmtSize from WaveFmtChunk and just reading the next bit of data
			f_lseek(sample_file, f_tell(sample_file) - br);

			//Re-read the whole chunk (or at least the fields we need) since it's a WaveFmtChunk
			rd = sizeof(WaveFmtChunk);
			res = f_read(sample_file, &fmt_chunk, rd, &br);

			if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; return(res);}//file not read
			else if (br < rd)	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);}//file ended unexpectedly when reading format header
			else if ( !is_valid_format_chunk(fmt_chunk) )	{g_error |= FILE_WAVEFORMATERR; f_close(sample_file); return(FR_INT_ERR);	}//format header error (not a valid wav file)
			else
			{
				//We found the 'fmt ' chunk, now skip to the next chunk
				//Note: this is necessary in case the 'fmt ' chunk is not exactly sizeof(WaveFmtChunk) bytes, even though that's how many we care about
				f_lseek(sample_file, next_chunk_start);

				//Look for the DATA chunk
				chunk.chunkId = 0;
				rd = sizeof(WaveChunk);

				while (chunk.chunkId != ccDATA)
				{
					res = f_read(sample_file, &chunk, rd, &br);

					if (res != FR_OK)	{g_error |= HEADER_READ_FAIL; f_close(sample_file); break;}
					if (br < rd)		{g_error |= FILE_WAVEFORMATERR;	f_close(sample_file); break;}

					//Fix an odd-sized chunk, it should always be even
					if (chunk.chunkSize & 0b1)
						chunk.chunkSize++;

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

						//Check the file is really as long as the data chunkSize says it is
						if (f_size(sample_file) < (f_tell(sample_file) + chunk.chunkSize))
						{
							chunk.chunkSize = f_size(sample_file) - f_tell(sample_file);
						}

						s_sample->sampleSize 		= chunk.chunkSize;
						s_sample->sampleByteSize 	= fmt_chunk.bitsPerSample>>3;
						s_sample->sampleRate		= fmt_chunk.sampleRate;
						s_sample->numChannels 		= fmt_chunk.numChannels;
						s_sample->blockAlign 		= fmt_chunk.numChannels * fmt_chunk.bitsPerSample>>3;
						s_sample->startOfData 		= f_tell(sample_file);

						if (fmt_chunk.audioFormat == 0xFFFE)
								s_sample->PCM 		= 3;
						else 	s_sample->PCM 		= fmt_chunk.audioFormat;

						s_sample->file_found 		= 1;
						s_sample->inst_end 			= s_sample->sampleSize;
						s_sample->inst_size 		= s_sample->sampleSize;
						s_sample->inst_start 		= 0;
						s_sample->inst_gain 		= 1.0;

						return(FR_OK);

					} //else chunk
				} //while chunk
			} //is_valid_format_chunk
		}//if ccFMT
	}//no file error

	return(FR_INT_ERR);
}


void clear_sample_header(Sample *s_sample)
{
	s_sample->filename[0] = 0;
	s_sample->sampleSize = 0;
	s_sample->sampleByteSize = 0;
	s_sample->sampleRate = 0;
	s_sample->numChannels = 0;
	s_sample->blockAlign = 0;
	s_sample->startOfData = 0;
	s_sample->PCM = 0;

	s_sample->file_found = 0;

	s_sample->inst_start = 0;
	s_sample->inst_end = 0;
	s_sample->inst_size = 0;
	s_sample->inst_gain = 1.0;
}
