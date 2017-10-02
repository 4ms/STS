/*
 * audio_util.h
 *
 *  Created on: Oct 26, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

// uint32_t offset_addr(uint32_t addr, uint8_t channel, int32_t offset);
// uint32_t inc_addr(uint32_t addr, uint8_t channel, uint8_t direction);
// uint8_t in_between(uint32_t mid, uint32_t beg, uint32_t end, uint8_t reverse);
// uint32_t abs_diff(uint32_t a1, uint32_t a2);

// uint32_t inc_addr_within_limits(uint32_t addr, uint32_t low_limit, uint32_t high_limit, uint8_t direction);
//uint32_t diff_circular(uint32_t leader, uint32_t follower, uint32_t wrap_size);
//uint32_t diff_wrap(uint32_t leader, uint32_t follower, uint8_t wrapping, uint32_t wrap_size);
uint32_t align_addr(uint32_t addr, uint32_t blockAlign);



