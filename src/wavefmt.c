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
			|| (sample_header.bitsPerSample		!= 8 		//Only 8,16,24,and 32 bit samplerate allowed
				&& sample_header.bitsPerSample	!= 16
				&& sample_header.bitsPerSample	!= 24
				&& sample_header.bitsPerSample	!= 32)
		)
		return 1;
	else
		return 0;

}
