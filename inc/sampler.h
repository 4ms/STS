/*
 * sampler.c - Main functionality for audio playback
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#pragma once

#include "circular_buffer.h"
#include "ff.h"
#include "sample_file.h"
#include <stm32f4xx.h>

/* Playback states */
enum PlayStates {
	SILENT,
	PREBUFFERING,
	PLAY_FADEUP,
	PERC_FADEUP,
	PLAYING,
	PLAYING_PERC,
	PLAY_FADEDOWN,
	RETRIG_FADEDOWN,
	REV_PERC_FADEDOWN,
	PAD_SILENCE,
	HOLD
};

// Note: normal state changes go like this:
// All start with SILENT -> PREBUFFERING if needed
// Playing with Length > 50%:
// 		PLAY_FADEUP -> PLAYING -> PLAY_FADEDOWN
// Playing with Length < 50% (not reversed):
//		PERC_FADEUP -> PLAYING_PERC
// Playing with Length < 50% (reversed):
//		PLAYING_PERC (fades up) -> PLAYING_PERC_FADEDOWN

enum PlayLoadTriage { NO_PRIORITY, PRIORITIZE_PLAYING };
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

//READ_BLOCK_SIZE must be a multiple of all possible sample file block sizes
//1(8m), 2(16m), 3(24m), 4(32m), 6(24s), 8(32s) ---> 24 is the lowest value
//It also should be a multiple of 512, since the SD Card is arranged by 512 byte sectors
//9216 = 512 * 18 = 24 * 384
#define READ_BLOCK_SIZE 9216

#define PERC_ENV_FACTOR 40000.0f

#define MAX_RS 20 /* over 4 octaves at 44.1k */

void audio_buffer_init(void);
void read_storage_to_buffer(void);
void play_audio_from_buffer(int32_t *outL, int32_t *outR, uint8_t chan);
void toggle_playing(uint8_t chan);
void start_playing(uint8_t chan);
void toggle_reverse(uint8_t chan);
void SDIO_read_IRQHandler(void);
