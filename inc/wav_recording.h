/*
 * wav_recording.h - wav file recording routines
 *
 * Authors: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplo@gmail.com)
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

#define WAV_COMMENT "Recorded on a 4ms Stereo Triggered Sampler" // goes into wav info chunk and id3 tag
#define WAV_SOFTWARE "4ms Stereo Triggered Sampler firmware v"	 //	goes into wav info chunk and id3 tag

#include <stm32f4xx.h>

enum RecStates {
	REC_OFF,
	CREATING_FILE,
	RECORDING,
	CLOSING_FILE,
	CLOSING_FILE_TO_REC_AGAIN

};

void stop_recording(void);
void toggle_recording(void);
void record_audio_to_buffer(int16_t *src);
void write_buffer_to_storage(void);
void init_rec_buff(void);
void create_new_recording(uint8_t bitsPerSample, uint8_t numChannels, uint32_t sample_rate);
FRESULT write_wav_info_chunk(FIL *wavfil, uint32_t *total_written);
FRESULT write_wav_size(FIL *wavfil, uint32_t data_chunk_bytes, uint32_t file_size_bytes);
