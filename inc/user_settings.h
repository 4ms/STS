/*
 * user_settings.h
 *
 *  Created on: Aug 4, 2017
 *      Author: Dan Green
 */

#ifndef USER_SETTINGS_H_
#define USER_SETTINGS_H_

#include <stm32f4xx.h>
#include "ff.h"

enum Settings
{
	NoSetting,
	StereoMode,
	RecordSampleBits,
	AutoStopSampleChange,

	NUM_SETTINGS_ENUM
};


void set_default_user_settings(void);
FRESULT save_user_settings(void);
FRESULT read_user_settings(void);


#endif /* USER_SETTINGS_H_ */
