/*
 * trigger_jacks.h
 *
 *  Created on: Dec 19, 2016
 *      Author: design
 */

#pragma once

#include <stm32f4xx.h>

enum TriggerJacks {
	TrigJack_Play1,
	TrigJack_Play2,
	TrigJack_Rec,
	TrigJack_Rev1,
	TrigJack_Rev2,
	NUM_TRIG_JACKS
};

enum TriggerStates {
	TrigJack_UP,
	TrigJack_DOWN
};



#define Trigger_Jack_Debounce_IRQHandler TIM5_IRQHandler
#define TrigJack_TIM TIM5


