/*
 * sampler.h
 */

#ifndef __sampler_h__
#define __sampler_h__

//#define ARM_MATH_CM4

#include <stm32f4xx.h>
#include "sample_file.h"
#include "ff.h"
#include "circular_buffer.h"


/* Playback states */
enum PlayStates {
	SILENT,
	PREBUFFERING,
	PLAY_FADEUP,
	PLAYING,
	PLAYING_PERC,
	PLAY_FADEDOWN,
	RETRIG_FADEDOWN,
	PLAYING_PERC_FADEDOWN,
	PAD_SILENCE,
	HOLD

};

enum PlayLoadTriage{
	NO_PRIORITY,
	PRIORITIZE_PLAYING
};
#define SDIO_read_IRQHandler TIM7_IRQHandler
#define SDIO_read_TIM TIM7


//44.1k/16b/stereo@ pitch=1.0: the time from the first block read to the first sample of audio outputted:
//(This is addition to the delay from the Trigger jack to the first block being read, around 16ms)
//BASE_BUFFER_THRESHOLD/READ_BLOCK_SIZE
//6144/16384: 13ms
//3072/16384: 9ms
//1536/16384: 6ms
//6144/8192: 13ms
//3072/8192: 9ms
//1536/8192: 7ms

//#define BASE_BUFFER_THRESHOLD (6144)
#define BASE_BUFFER_THRESHOLD (3072)
//#define BASE_BUFFER_THRESHOLD (1536)

#define READ_BLOCK_SIZE 8192
//#define READ_BLOCK_SIZE 16384


#define MAX_RS 20 /* over 4 octaves at 44.1k */
//#define MAX_RS_READ_BUFF_LEN ((codec_BUFF_LEN >> 2) * MAX_RS)

void audio_buffer_init(void);
void read_storage_to_buffer(void);
//void play_audio_from_buffer(int32_t *out, uint8_t chan);
void play_audio_from_buffer(int32_t *outL, int32_t *outR, uint8_t chan);


void toggle_playing(uint8_t chan);
void start_playing(uint8_t chan);

void toggle_reverse(uint8_t chan);
void check_change_bank(uint8_t chan);
void check_change_sample(void);

//uint8_t preload_sample(uint32_t samplenum, FIL* sample_file);

uint32_t calc_start_point(float start_param, Sample *sample);
uint32_t calc_stop_point(float length_param, float resample_param, Sample *sample, uint32_t startpos);


void clear_is_buffered_to_file_end(uint8_t chan);
//void check_trim_bounds(void);

void SDIO_read_IRQHandler(void);

#endif

