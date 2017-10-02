/*
 * sts_filesystem.h
 *
 *  Created on: Jan 9, 2017
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

uint32_t map_cache_to_buffer(uint32_t cache_point,  uint8_t sampleByteSize, uint32_t ref_cachepos, uint32_t ref_bufferpos, CircularBuffer *b);
uint32_t map_buffer_to_cache(uint32_t buffer_point, uint8_t sampleByteSize, uint32_t cache_start, uint32_t buffer_start, CircularBuffer *b);



