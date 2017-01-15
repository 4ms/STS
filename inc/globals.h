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

//=2048B codec_BUFF_LEN
//=1024B each Half Transfer
//=256 samples (32-bit samples from codec, even if in 16bit mode)
#define codec_BUFF_LEN 2048
#define HT16_BUFF_LEN (codec_BUFF_LEN>>2)
#define HT16_CHAN_BUFF_LEN (HT16_BUFF_LEN>>1)

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
	PlayBuff1_Discontinuity,
	PlayBuff2_Discontinuity,
	ToggleMonitor,
	ToggleLooping1,
	ToggleLooping2,
	PlaySample1Changed_valid,
	PlaySample2Changed_valid,
	PlaySample1Changed_empty,
	PlaySample2Changed_empty,
	RecSampleChanged_light,
	ForceFileReload1,
	ForceFileReload2,
	AssignModeRefused1,
	AssignModeRefused2,
	AssigningEmptySample1,
	AssigningEmptySample2,

	NUM_FLAGS
};


//Error codes for g_error
enum g_Errors{
	OUT_OF_MEM=1,
	OUT_OF_SAMPLES=2,
	SPIERROR_1=4,
	WRITE_SDRAM_ERROR=8,
	DMA_OVR_ERROR=16,
	sFLASH_BAD_ID=32,
	WRITE_BUFF_OVERRUN=64,
	READ_BUFF1_OVERRUN=128,
	READ_BUFF2_OVERRUN=256,
	READ_MEM_ERROR=512,
	WRITE_SDCARD_ERROR=1024,
	WRITE_BUFF_UNDERRUN=2048,

	FILE_OPEN_FAIL=(1<<12),
	FILE_READ_FAIL=(1<<13),
	FILE_WAVEFORMATERR=(1<<14),
	FILE_UNEXPECTEDEOF=(1<<15),
	FILE_SEEK_FAIL=(1<<16),
	READ_BUFF_OVERRUN_RESAMPLE=(1<<17),
	FILE_CANNOT_CREATE_CLTBL=(1<<18),
	CANNOT_OPEN_ROOT_DIR=(1<<19),
	FILE_REC_OPEN_FAIL=(1<<20),
	FILE_WRITE_FAIL=(1<<21),
	FILE_UNEXPECTEDEOF_WRITE=(1<<22),
	CANNOT_WRITE_INDEX=(1<<23)


};
/*
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
*/

//Number of channels
#define NUM_PLAY_CHAN 2
#define NUM_REC_CHAN 1
#define NUM_ALL_CHAN 3

#define NUM_SAMPLES_PER_BANK 10
#define MAX_NUM_BANKS 16
#define MAX_NUM_REC_BANKS 8

#define REC (NUM_ALL_CHAN-1) /* =2 a shortcut to the REC channel */

#define TRIG_TIME 400

#define READ_BLOCK_SIZE 8192


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


void check_errors(void);

//extern volatile uint32_t sys_time;
//#define delay_sys(x) do{register uint32_t donetime=x+systime;__asm__ __volatile__ ("nop\n\t":::"memory");}while(sys_time!=donetime;)


#endif /* GLOBALS_H_ */
