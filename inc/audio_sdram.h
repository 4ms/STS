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
#include "circular_buffer.h"

//#define MEM_SAMPLE_PRELOAD_BASE		0xD0000000
//#define MEM_SAMPLE_PRELOAD_SIZE		0x0002EE00
//
//#define MEM_PLAYBACK_BUFFER_1 		0xD0400000
//#define MEM_PLAYBACK_BUFFER_2 		0xD0800000
//#define MEM_PLAYBACK_BUFFER_SIZE 	0x003FFFFF
//
//#define MEM_RECORD_BUFFER_1 		0xD1000000
//#define MEM_RECORD_BUFFER_2			0xD1800000
//#define MEM_RECORD_BUFFER_SIZE 		0x007FFFFF

#define MEM_SIZE (SDRAM_SIZE>>2) /* 0x02000000 / 4 = 0x00800000 */


//#define PRE_BUFF_SIZE 32
//#define PRE_BUFF_SIZE 1024
//#define PRE_BUFF_SIZE 256
#define PRE_BUFF_SIZE 64

#define RECCHAN 2

uint32_t memory_read_32bword(uint32_t addr);

uint32_t memory_read_channel(uint32_t *addr, uint8_t channel, int32_t *rd_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);
uint32_t memory_write_channel(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint8_t decrement);

uint32_t memory_read(uint32_t *addr, uint8_t channel, int32_t *rd_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);
uint32_t memory_read16(uint32_t *addr, uint8_t channel, int16_t *rd_buff, uint32_t num_samples, uint32_t stop_at_addr, uint8_t decrement);

uint32_t memory_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);
uint32_t memory_write16(uint32_t *addr, uint8_t channel, int16_t *wr_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement);

uint32_t memory_fade_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint8_t decrement, float fade);


uint32_t memory_read16_cb(CircularBuffer* b, int16_t *rd_buff, uint32_t num_samples, uint8_t decrement);

uint32_t memory_write16_cb(CircularBuffer* b, int16_t *wr_buff, uint32_t num_samples, uint8_t decrement);


void memory_clear(uint8_t channel);

uint32_t RAM_test(void);
void RAM_startup_test(void);

#endif /* AUDIO_MEMORY_H_ */
