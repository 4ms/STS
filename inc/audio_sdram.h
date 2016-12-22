/*
 * audio_sdram.h
 *
 *  Created on: Apr 6, 2015
 *      Author: design
 */

#ifndef AUDIO_MEMORY_H_
#define AUDIO_MEMORY_H_
#include <stm32f4xx.h>
#include "sdram_driver.h"

#define MEM_SIZE (SDRAM_SIZE>>2)

#define RECCHAN 2

uint32_t memory_read_32bword(uint32_t addr);

uint32_t memory_read_channel(uint32_t *addr, uint8_t channel, int32_t *rd_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);
uint32_t memory_write_channel(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint8_t decrement);

uint32_t memory_read(uint32_t *addr, uint8_t channel, int32_t *rd_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);
uint32_t memory_read16(uint32_t *addr, uint8_t channel, int16_t *rd_buff, uint32_t num_samples, uint32_t stop_at_addr, uint8_t decrement);

uint32_t memory_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);
uint32_t memory_write16(uint32_t *addr, uint8_t channel, int16_t *wr_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);

uint32_t memory_fade_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint8_t decrement, float fade);

void memory_clear(uint8_t channel);

uint32_t RAM_test(void);
void RAM_startup_test(void);

#endif /* AUDIO_MEMORY_H_ */
