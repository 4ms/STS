/*
 * looping_delay.h
 */

#ifndef __audio__
#define __audio__

//#define ARM_MATH_CM4

#include <stm32f4xx.h>


/* Playback states */
enum PlayStates {
	SILENT,
	PLAY_FADEUP,
	PLAYING,
	PLAY_FADEDOWN,
	PREBUFFERING

};

void audio_buffer_init(void);

void write_buffer_to_storage(void);
void read_storage_to_buffer(void);


void increment_read_fade(uint8_t channel);
void process_audio_block_codec(int16_t *src, int16_t *dst);

void toggle_playing(uint8_t chan);
void toggle_recording(void);

#endif

