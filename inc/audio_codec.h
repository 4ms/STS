/*
 * audio_codec.h
 */

#ifndef __audio_codec_h__
#define __audio_codec_h__

#include <stm32f4xx.h>


void process_audio_block_codec(int16_t *src, int16_t *dst);


#endif