/*
 * audio_codec.h
 */

#pragma once

#include <stm32f4xx.h>

void process_audio_block_codec(int16_t *src, int16_t *dst);
