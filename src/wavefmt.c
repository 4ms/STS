#include "wavefmt.h"

uint8_t is_valid_wav_format(WaveHeader sample_header)
{
	if (	sample_header.RIFFId 		!= ccRIFF			//'RIFF'
			|| sample_header.fileSize		 < 16			//File size - 8
			|| sample_header.WAVEId 		!= ccWAVE		//'WAVE'
			|| sample_header.fmtId 			!= ccFMT		//'fmt '
			|| sample_header.fmtSize 		 < 16			//Format Chunk size
			|| sample_header.audioFormat	!= 0x0001		//PCM format
			|| sample_header.numChannels 	 > 0x0002		//Stereo or mono allowed
			|| sample_header.sampleRate 	 > 48000		//Between 8k and 48k sampling rate allowed
			|| sample_header.sampleRate		 < 8000
			|| (sample_header.bitsPerSample		!= 16 		//Only 16 bit samplerate allowed (for now)
				//&& sample_header.bitsPerSample	!= 8
				//&& sample_header.bitsPerSample	!= 24
				//&& sample_header.bitsPerSample	!= 32
				)
		)
		return 0;
	else
		return 1;

}

void create_waveheader(WaveHeader *w)
{
	w->RIFFId		= ccRIFF;
	w->fileSize 	= sizeof(WaveHeader) - 8; //filesize - 8, for now we let this be the size of the wave header - 8
	w->WAVEId 		= ccWAVE;
	w->fmtId 		= ccFMT;
	w->fmtSize		= 16;
	w->audioFormat	= 1;
	w->numChannels	= 2;
	w->sampleRate	= 44100;
	w->byteRate		= (44100 * 2 * 2); //sampleRate * numChannels * bitsPerSample
	w->blockAlign	= 4;
	w->bitsPerSample= 16;
}

void create_chunk(uint32_t chunkId, uint32_t chunkSize, WaveChunk *wc)
{
	wc->chunkId	= chunkId;
	wc->chunkSize = chunkSize;

}


//uint32_t waveheader_to_string(WaveHeader w, char *str)
//{
//	//if (is_valid_wav_format(w))
//	//{
//		str=(char *)&w;
//		return sizeof(WaveHeader);
//	//}
//	//else return 0;
//}
//
//uint32_t append_chunk_header(uint32_t id, uint32_t size, char *str, uint32_t sz)
//{
//	str[sz++] = 'd';
//	str[sz++] = 'a';
//	str[sz++] = 't';
//	str[sz++] = 'a';
//	str[sz++] = size>>24 && 0xFF;
//	str[sz++] = size>>16 && 0xFF;
//	str[sz++] = size>>8 && 0xFF;
//	str[sz++] = size>>0 && 0xFF;
//
//
//
//	return sz;
//}
