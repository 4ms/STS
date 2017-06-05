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
	RETRIG_FADEDOWN,
	HOLD

};

enum PlayLoadTriage{
	NO_PRIORITY,
	PRIORITIZE_PLAYING
};
#define SDIO_read_IRQHandler TIM7_IRQHandler
#define SDIO_read_TIM TIM7



//#define PRE_BUFF_SIZE (8192*3) /*0x6000*/
//#define ACTIVE_BUFF_SIZE (8192*64) /* measured gaps of about 32 blocks, so this is twice enough */
//#define ACTIVE_BUFF_SIZE (8192*16)
#define BASE_BUFFER_THRESHOLD (6144) /* 512*12 */
//#define BASE_BUFFER_THRESHOLD (3072) /* 512*6: divided by 2 because we now account for pitch*/

//96k, blockalign=4 (32-bit float mono), Pitch 1.0 --> ~42ms from trigger to playback
//96k, blockalign=4 (32-bit float mono), Pitch 4.0 --> ~144ms
//96k, blockalign=4 (16-bit stereo), Pitch 4.0 --> 144ms (and not enough! we get buffer underrun)
//96k, blockalign=4 (16-bit stereo), Pitch 1.0 --> 144ms (and not enough! we get buffer underrun)

//44.1k, blockalign=6 (24-bit stereo), Pitch 4.0 --> 100ms (and not enough! we get buffer underrun)

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

//uint8_t preload_sample(uint32_t samplenum, FIL* sample_file);

uint32_t calc_start_point(float start_param, Sample *sample);
uint32_t calc_stop_points(float length, Sample *sample, uint32_t startpos);
uint32_t calc_play_length(float length, Sample *sample);

uint32_t map_cache_to_buffer(uint32_t cache_point,  uint8_t sampleByteSize, uint32_t ref_cachepos, uint32_t ref_bufferpos, CircularBuffer *b);
uint32_t map_buffer_to_cache(uint32_t buffer_point, uint8_t sampleByteSize, uint32_t cache_start, uint32_t buffer_start, CircularBuffer *b);

void clear_is_buffered_to_file_end(uint8_t chan);
//void check_trim_bounds(void);

void SDIO_read_IRQHandler(void);

#endif

