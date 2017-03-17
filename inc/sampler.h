/*
 * sampler.h
 */

#ifndef __sampler_h__
#define __sampler_h__

//#define ARM_MATH_CM4

#include <stm32f4xx.h>
#include "sts_filesystem.h"
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
	RETRIG_FADEDOWN

};

enum PlayLoadTriage{
	NO_PRIORITY,
	PRIORITIZE_PLAYING
};



#define PRE_BUFF_SIZE (8192*3) /*0x6000*/


#define MAX_RS 4
#define MAX_RS_READ_BUFF_LEN ((codec_BUFF_LEN >> 2) * MAX_RS)

void audio_buffer_init(void);
void read_storage_to_buffer(void);
void play_audio_from_buffer(int32_t *out, uint8_t chan);


void toggle_playing(uint8_t chan);
void toggle_reverse(uint8_t chan);
void check_change_bank(uint8_t chan);

//uint8_t preload_sample(uint32_t samplenum, FIL* sample_file);

uint32_t calc_start_point(float start_param, Sample *sample);
uint32_t calc_stop_points(float length, Sample *sample, uint32_t startpos);
uint32_t calc_play_length(float length, Sample *sample);

uint32_t map_cache_to_buffer(uint32_t cache_point, uint32_t ref_cachepos, uint32_t ref_bufferpos, CircularBuffer *b);

#endif

