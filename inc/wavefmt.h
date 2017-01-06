/*
 * wavefmt.h
 *
 *  Created on: Dec 5, 2016
 *      Author: design
 */

#ifndef INC_WAVEFMT_H_
#define INC_WAVEFMT_H_

#include <stm32f4xx.h>

#define ccWAVE	0x45564157
#define ccRIFF	0x46464952
#define ccFMT	0x20746D66
#define ccDATA	0x61746164


typedef struct WaveHeader {
    // Riff Wave Header
	uint32_t RIFFId;
    uint32_t fileSize;
    uint32_t WAVEId;

    // Format Subchunk
    uint32_t fmtId;
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} WaveHeader;

typedef struct WaveChunk {
    // Data Subchunk
    uint32_t chunkId;
    uint32_t  chunkSize;

} WaveChunk;

typedef struct WaveHeaderAndChunk {
	WaveHeader wh;
	WaveChunk wc;
} WaveHeaderAndChunk;

uint8_t is_valid_wav_format(WaveHeader sample_header);

void create_waveheader(WaveHeader *w);
void create_chunk(uint32_t chunkId, uint32_t chunkSize, WaveChunk *wc);

#endif /* INC_WAVEFMT_H_ */
