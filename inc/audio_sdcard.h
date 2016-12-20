/*
* memory.h
 *
 *  Created on: Jun 8, 2014
 *      Author: design
 */

#ifndef MEMORY_H_
#define MEMORY_H_

#include <stm32f4xx.h>

/* PAGE_SIZE: number of 8-bit elements in a SDIO page */
#define PAGE_SIZE 512

/* BUFF_LEN: size of the mono 16-bit buffers. Should be PAGE_SIZE/2 since each element is 2 bytes */
#define BUFF_LEN 256

/* STEREO_BUFF_LEN: number of elements in the buffer coming direct to/from the codec (not being used)*/
#define STEREO_BUFF_LEN 512

/* WRITE_BUFF_SIZE: number of elements in circular buffer array (each element has BUFF_LEN elements) */
#define WRITE_BUFF_SIZE 32
//was 128

/* READ_BUFF_SIZE: number of elements in circular buffer array (each element has BUFF_LEN elements) */
#define READ_BUFF_SIZE 96

//#define PRE_BUFF_SIZE 32
//#define PRE_BUFF_SIZE 1024
#define PRE_BUFF_SIZE 256

#define MAXSAMPLES 19

//#define MEMORY_SIZE 0x00400000 /* 2GB / 512 Bytes per block = 4M blocks */
//#define BANK_SIZE 0x30000 /* BANKs a  re over 1100 seconds, almost 20 minutes: 0x35CF0 x 512B/block = 106MBytes or 1175 seconds @48k/16bit*/

#define MEMORY_SIZE 0x003FE5D0 /* 2GB / 512 Bytes per block = 4M blocks is about 6 hours*/
#define BANK_SIZE 0x35CF0 /* =220400 BANKs are over 1100 seconds, almost 20 minutes: 0x35CF0 x 512B/block = 106MBytes or 1175 seconds @48k/16bit*/
#define SAMPLE_SIZE 0x2D50 /* =11600 SAMPLEs are 0x2D50 = 11600 blocks = 5800kB = 61.8seconds @48k/16bit */
//leaves 0x1A30 blocks for directory map, other fun stuff if we need it.

uint8_t read_sdcard(uint16_t *data, uint32_t addr);
uint8_t write_sdcard(uint16_t *data, uint32_t addr);
uint8_t init_sdcard(void);
uint8_t test_sdcard(void);

#endif /* MEMORY_H_ */
