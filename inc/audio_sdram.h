/*
 * audio_sdram.h
 *
 *  Created on: Apr 6, 2015
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>
#include "sdram_driver.h"
#include "circular_buffer.h"

//Play buffer 1:  	0xD0000000 - 0xD011FFFF
//Play buffer 2:  	0xD0120000 - 
//Play buffer 3:  	0xD0240000 - 
//Play buffer 4:  	0xD0360000 - 
//..
//Play buffer 11: 	0xD0B40000 - 
//...
//Play buffer 20: 	0xD1560000 - 0xD167FFFF
//	unused 		   [0xD1680000 - 0xD17FFFFF] = 0xB0000 = 600kB unused
//Record buffer: 	0xD1800000 - 0xD1FFFFF8

#define PLAY_BUFF_START			(0x00000000 + SDRAM_BASE)
#define PLAY_BUFF_SLOT_SIZE		 0x00120000

#define	REC_BUFF_START			(0x01800000 + SDRAM_BASE)
#define REC_BUFF_SIZE			 0x007FFFF8


#define REC_CHAN 2

void memory_clear(void);

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


