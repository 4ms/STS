/*  * globals.h
 *
 *  Created on: Jun 8, 2014
 *      Author: design
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_

//codec_BUFF_LEN = Size (in samples) of the DMA rx/tx buffers:
//Each channel = codec_BUFF_LEN/2 samples
//We process the rx buffer when it's half-full and 100% full, so codec_BUFF_LEN/4 samples are processed at one time

//2048 codec_BUFF_LEN
//1024 x 16-bits each Half Transfer
//256 x samples per channel (32-bit samples even if only using 16-bits)
//matches 256 samples per SDIO block (512 bytes/block = 256 x 16bit samples/block)
#define codec_BUFF_LEN 2048

#define FW_VERSION 0

enum Flags {
	PlaySample1Changed,
	PlayBank1Changed,
	PlaySample2Changed,
	PlayBank2Changed,
	RecSampleChanged,
	RecBankChanged,
	Play1Trig,
	Play2Trig,
	RecTrig,
	Rev1Trig,
	Rev2Trig,
	NUM_FLAGS
};


//Error codes for g_error
#define OUT_OF_MEM 1
#define OUT_OF_SAMPLES 2
#define SPIERROR_1 4
#define SPIERROR_2 8
#define DMA_OVR_ERROR 16
#define sFLASH_BAD_ID 32
#define WRITE_BUFF_OVERRUN 64
#define READ_BUFF1_OVERRUN 128
#define READ_BUFF2_OVERRUN 256
#define READ_MEM_ERROR 512
#define WRITE_SDCARD_ERROR 1024
#define WRITE_BUFF_UNDERRUN 2048

//Number of channels
#define NUM_PLAY_CHAN 2
#define NUM_REC_CHAN 1
#define NUM_ALL_CHAN 3

#define REC (NUM_ALL_CHAN-1) /* =2 a shortcut to the REC channel */

#define TRIG_TIME 400


//About 45ms delay
#define delay()						\
do {							\
  register unsigned int i;				\
  for (i = 0; i < 1000000; ++i)				\
    __asm__ __volatile__ ("nop\n\t":::"memory");	\
} while (0)


#define delay_ms(x)						\
do {							\
  register unsigned int i;				\
  for (i = 0; i < (25000*x); ++i)				\
    __asm__ __volatile__ ("nop\n\t":::"memory");	\
} while (0)


//extern volatile uint32_t sys_time;
//#define delay_sys(x) do{register uint32_t donetime=x+systime;__asm__ __volatile__ ("nop\n\t":::"memory");}while(sys_time!=donetime;)


#endif /* GLOBALS_H_ */
