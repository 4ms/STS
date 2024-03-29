/*
 * globals.h
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

// #define DEBUG_ENABLED
#define ENABLE_VARIABLE_RECORD_SAMPLE_RATE

// 128 bytes per DMA transfer
// 64 bytes/half-transfer @ 16 bits/sample = 32 samples per half-transfer
// 32 samples/half-transfer = 16 samples per channel per half-transfer
// 16/interrupt @ 44100Hz = interrupt runs every 0.36ms

#define codec_BUFF_LEN 128
#define HT16_BUFF_LEN (codec_BUFF_LEN >> 2)		/*32*/
#define HT16_CHAN_BUFF_LEN (HT16_BUFF_LEN >> 1) /*16*/
#define FramesPerBlock 16.0f

#define ONE_SECOND 44100

//Update the FW Version anytime FLASH RAM settings format is changed
#define FW_MAJOR_VERSION 1
#define FW_MINOR_VERSION 5

// Flags are used for sections of code with different interrupt priorities
// to communicate that an event occured and/or to schedule an action to occur
enum Flags {
	PlaySample1Changed, //0
	PlaySample2Changed,
	PlayBank1Changed,
	PlayBank2Changed,
	RecSampleChanged,
	RecBankChanged, //5
	Play1But,
	Play2But,
	RecTrig,
	Rev1Trig,
	Rev2Trig, //10
	PlayBuff1_Discontinuity,
	PlayBuff2_Discontinuity,
	ToggleMonitor,
	ToggleLooping1,
	ToggleLooping2, //15
	PlaySample1Changed_valid,
	PlaySample2Changed_valid,
	PlaySample1Changed_empty,
	PlaySample2Changed_empty,
	RecSampleChanged_light, //20 //NOT USED
	ForceFileReload1,
	ForceFileReload2,
	AssignModeRefused,
	AssigningEmptySample,
	TimeToReadStorage, //25
	Play1Trig,
	Play2Trig,
	StereoModeTurningOn,
	StereoModeTurningOff,
	SkipProcessButtons, //30
	ViewBlinkBank1,
	ViewBlinkBank2,
	PlayBankHover1Changed,
	PlayBankHover2Changed,
	RecBankHoverChanged, //35
	RevertSample,
	RevertBank1,
	RevertBank2,
	RevertAll,
	RewriteIndex, //40
	RewriteIndexFail,
	RewriteIndexSucess,
	AssignedNextSample,
	AssignedPrevBank,
	FindNextSampleToAssign, //45
	SaveUserSettings,
	UndoSampleExists,
	UndoSampleDiffers,
	LoadBackupIndex,
	LoadIndex, //50
	LatchVoltOctCV1,
	LatchVoltOctCV2,
	Play1TrigDelaying,
	Play2TrigDelaying,
	BootBak, // 55
	SystemModeButtonsDown,
	ShutdownAndBootload,
	ChangedTrigDelay,
	SaveUserSettingsLater,
	ChangePlaytoPerc1, //60
	ChangePlaytoPerc2,
	PercEnvModeChanged,
	FadeEnvModeChanged,
	FadeUpDownTimeChanged,
	RecSampleChanged_valid, //65
	RecSampleChanged_empty,
	RecStartedWithTrigger,
	NUM_FLAGS
};

//Error codes for g_error
enum g_Errors {
	READ_BUFF1_OVERRUN = (1 << 0),
	READ_BUFF2_OVERRUN = (1 << 1),
	READ_BUFF1_UNDERRUN = (1 << 2),
	READ_BUFF2_UNDERRUN = (1 << 3),
	DMA_OVR_ERROR = (1 << 4),
	sFLASH_BAD_ID = (1 << 5),
	WRITE_BUFF_OVERRUN = (1 << 6),
	OUT_OF_MEM = (1 << 7),
	SDCARD_CANT_MOUNT = (1 << 8),
	READ_MEM_ERROR = (1 << 9),
	WRITE_SDCARD_ERROR = (1 << 10),
	WRITE_BUFF_UNDERRUN = (1 << 11),

	FILE_OPEN_FAIL = (1 << 12),
	FILE_READ_FAIL_1 = (1 << 13),
	FILE_READ_FAIL_2 = (1 << 14),
	FILE_WAVEFORMATERR = (1 << 15),
	FILE_UNEXPECTEDEOF = (1 << 16),
	FILE_SEEK_FAIL = (1 << 17),
	READ_BUFF_OVERRUN_RESAMPLE = (1 << 18),
	FILE_CANNOT_CREATE_CLTBL = (1 << 19),
	CANNOT_OPEN_ROOT_DIR = (1 << 20),
	FILE_REC_OPEN_FAIL = (1 << 21),
	FILE_WRITE_FAIL = (1 << 22),
	FILE_UNEXPECTEDEOF_WRITE = (1 << 23),
	CANNOT_WRITE_INDEX = (1 << 24),
	LSEEK_FPTR_MISMATCH = (1 << 25),
	HEADER_READ_FAIL = (1 << 26)

};

//Number of channels
#define NUM_PLAY_CHAN 2
#define NUM_REC_CHAN 1
#define NUM_ALL_CHAN 3

#define NUM_SAMPLES_PER_BANK 10
#define MAX_NUM_BANKS 60

#define REC (NUM_ALL_CHAN - 1) /* =2 a shortcut to the REC channel */
#define CHAN1 0
#define CHAN2 1

//About 45ms delay
#define delay()                                                                                                        \
	do {                                                                                                               \
		register unsigned int i;                                                                                       \
		for (i = 0; i < 1000000; ++i)                                                                                  \
			__asm__ __volatile__("nop\n\t" ::: "memory");                                                              \
	} while (0)

#define delay_ms(x)                                                                                                    \
	do {                                                                                                               \
		volatile unsigned int i;                                                                                       \
		for (i = 0; i < (25000 * x); ++i)                                                                              \
			__asm__ __volatile__("nop\n\t" ::: "memory");                                                              \
	} while (0)

#define CCMDATA __attribute__((section(".ccmdata")))

void check_errors(void);
