/*
 * circular_buffer.h - routines for handling circular buffers 
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

#include <stm32f4xx.h>

typedef struct CircularBuffer {
	uint32_t in;
	uint32_t out;
	uint32_t min;
	uint32_t max;
	uint32_t size; //should always be max-min
	uint8_t wrapping;
} CircularBuffer;

uint8_t CB_offset_in_address(CircularBuffer *b, uint32_t amt, uint8_t subtract);
uint8_t CB_offset_out_address(CircularBuffer *b, uint32_t amt, uint8_t subtract);
uint32_t CB_distance(CircularBuffer *b, uint8_t reverse);
uint32_t CB_distance_points(uint32_t leader, uint32_t follower, uint32_t size, uint8_t reverse);

void CB_init(CircularBuffer *b, uint8_t rev);
