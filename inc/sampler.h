/*
 * looping_delay.h
 */

#ifndef __audio__
#define __audio__

//#define ARM_MATH_CM4

#include <stm32f4xx.h>
#include "sts_filesystem.h"
#include "ff.h"


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
//void play_audio_from_buffer(int32_t *out, uint8_t chan);
void play_audio_from_buffer(int32_t *out, uint8_t chan);

//void increment_read_fade(uint8_t channel);
void process_audio_block_codec(int16_t *src, int16_t *dst);

void toggle_playing(uint8_t chan);
void toggle_reverse(uint8_t chan);
void check_change_bank(uint8_t chan);

//uint8_t preload_sample(uint32_t samplenum, FIL* sample_file);

uint32_t calc_start_point(float start_param, Sample *sample);
void calc_stop_points(float length, Sample *sample, uint32_t startpos, uint32_t *fwd_stop_point);
uint32_t calc_play_length(float length, Sample *sample);


#endif

