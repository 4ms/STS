/*
 * circular_buffer.h
 *
 *  Created on: Dec 28, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

typedef struct CircularBuffer {
	uint32_t in;
	uint32_t out;
	uint32_t min;
	uint32_t max;
	uint32_t size; //should always be max-min
	uint8_t  wrapping;
} CircularBuffer;


uint8_t CB_offset_in_address(CircularBuffer *b, uint32_t amt, uint8_t subtract);
uint8_t CB_offset_out_address(CircularBuffer *b, uint32_t amt, uint8_t subtract);
uint32_t CB_distance(CircularBuffer *b, uint8_t reverse);
uint32_t CB_distance_points(uint32_t leader, uint32_t follower, uint32_t size, uint8_t reverse);

void CB_init(CircularBuffer *b, uint8_t rev);



