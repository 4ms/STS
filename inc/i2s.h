/*
 * i2s.h - I2S feeder routines
 */

#pragma once

#include <stm32f4xx.h>

void init_audio_dma(void);
void deinit_audio_dma(void);
void set_codec_callback(void (*cb)(int16_t *, int16_t *));

void start_audio_clock_source(void);
void stop_audio_clock_source(void);

void start_audio_stream(void);
void stop_audio_stream(void);

void deinit_i2s(void);

