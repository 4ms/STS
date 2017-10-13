/*
 * user_settings.h
 *
 *  Created on: Aug 4, 2017
 *      Author: Dan Green
 */

#pragma once

#include <stm32f4xx.h>
#include "ff.h"

enum Settings
{
	NoSetting,
	StereoMode,
	RecordSampleBits,
	AutoStopSampleChange,
	PlayStopsLengthFull,
	QuantizeChannel1,
	QuantizeChannel2,
	PercEnvelope,
	FadeEnvelope,
	StartUpBank_ch1,
	StartUpBank_ch2,
	TrigDelay,

	NUM_SETTINGS_ENUM
};


void set_default_user_settings(void);
FRESULT save_user_settings(void);
FRESULT read_user_settings(void);



