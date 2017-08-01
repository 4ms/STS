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

//#define MEM_SIZE (SDRAM_SIZE>>2) /* 0x02000000 / 4 = 0x00800000 */
#define MEM_SIZE 0x600000
//#define MEM_SIZE 0xAAAAAA //this would be closest to a max buffer size, equal sizes for play and record
//#define MEM_SIZE 0xAAAA98 //this would be the largest size that's a multiple of 24
//#define MEM_SIZE 0xAAA000 //this would be the largest size that's a multiple of 24 and ends in 000 (8192 B unsued)
//#define MEM_SIZE 0x900000 //this would be the largest size that's a multiple of 24 and ends in 00000. (5 MB unused)



//Play buffer 1: 0xD0000000 - 0xD05FFFFF
//Play buffer 2: 0xD0600000 - 0xD0BFFFFF
//Record buffer: 0xD0C00000 - 0xD11FFFFF
//unused.......: 0xD1200000 - 0xD1FFFFFF (0x00E00000 space = 14MB of the 32MB chip)
//Todo: maximum buffer sizes!


//#define PRE_BUFF_SIZE (64*256)

#define REC_CHAN 2

void memory_clear(uint8_t channel);

uint32_t memory_read16_cb(CircularBuffer* b, int16_t *rd_buff, uint32_t num_samples, uint8_t decrement);
uint32_t memory_read24_cb(CircularBuffer* b, uint8_t *rd_buff, uint32_t num_samples, uint8_t decrement);

uint32_t memory_write16_cb(CircularBuffer* b, int16_t *wr_buff, uint32_t num_samples, uint8_t decrement);

uint32_t memory_write_8as16(CircularBuffer* b, uint8_t *wr_buff, uint32_t num_bytes, uint8_t decrement);
uint32_t memory_write_16as16(CircularBuffer* b, uint32_t *wr_buff, uint32_t num_words, uint8_t decrement);
uint32_t memory_write_24as16(CircularBuffer* b, uint8_t *wr_buff, uint32_t num_bytes, uint8_t decrement);
uint32_t memory_write_32ias16(CircularBuffer* b, uint8_t *wr_buff, uint32_t num_bytes, uint8_t decrement);
uint32_t memory_write_32fas16(CircularBuffer* b, float *wr_buff, uint32_t num_floats, uint8_t decrement);


uint32_t RAM_test(void);
void RAM_startup_test(void);

#endif /* AUDIO_MEMORY_H_ */
