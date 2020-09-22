/*
 * i2s.h - I2S feeder routines
 */

#pragma once

#include <stm32f4xx.h>

void Init_I2SDMA(void);
void DeInit_I2S_Clock(void);
void DeInit_I2SDMA(void);
void Start_I2SDMA(void);
void init_audio_dma(void);
void set_codec_callback(void (*cb)(int16_t *, int16_t *));

