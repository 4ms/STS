/*
 * looping_delay.h
 */

#ifndef __audio__
#define __audio__

//#define ARM_MATH_CM4

#include <stm32f4xx.h>

#include "ff.h"

#define NUM_SAMPLES_PER_BANK 19

/* Playback states */
enum PlayStates {
	SILENT,
	PREBUFFERING,
	PLAY_FADEUP,
	PLAYING,
	PLAYING_PERC,
	PLAY_FADEDOWN,
	RETRIG_FADEDOWN

};

enum RecStates {
	REC_OFF,
	CREATING_FILE,
	RECORDING,
	CLOSING_FILE,
	REC_PAUSED

};

typedef struct Sample {
	char	filename[_MAX_LFN];
	uint32_t sampleSize;
	uint32_t startOfData;
	uint8_t sampleByteSize;
	uint32_t sampleRate;
	uint8_t numChannels;
	uint8_t blockAlign;
} Sample;


#define MAX_RS 4
#define MAX_RS_READ_BUFF_LEN (BUFF_LEN * MAX_RS)

void audio_buffer_init(void);

void write_buffer_to_storage(void);
void read_storage_to_buffer(void);


void increment_read_fade(uint8_t channel);
void process_audio_block_codec(int16_t *src, int16_t *dst);

void toggle_playing(uint8_t chan);
void toggle_recording(void);

void toggle_reverse(uint8_t chan);

uint8_t load_sample_header(uint32_t samplenum, FIL* sample_file);
uint8_t preload_sample(uint32_t samplenum, FIL* sample_file);
void calc_stop_points(float length, uint32_t samplenum, uint32_t startpos, uint32_t *fwd_stop_point, uint32_t *rev_stop_point);


#endif

