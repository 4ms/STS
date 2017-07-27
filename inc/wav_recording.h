/*
 * wav_recording.h
 *
 *  Created on: Jan 6, 2017
 *      Author: design
 */

#ifndef INC_WAV_RECORDING_H_
#define INC_WAV_RECORDING_H_

#include <stm32f4xx.h>

enum RecStates {
	REC_OFF,
	CREATING_FILE,
	RECORDING,
	CLOSING_FILE,
	CLOSING_FILE_TO_REC_AGAIN

};

void stop_recording(void);
void toggle_recording(void);
void record_audio_to_buffer(int16_t *src);
void write_buffer_to_storage(void);
void init_rec_buff(void);
void create_new_recording(void);
FRESULT write_wav_size(FIL *wavfil, uint32_t samplebytes_recorded);


#endif /* INC_WAV_RECORDING_H_ */
