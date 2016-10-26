/*
 * timekeeper.c
 *
 *  Created on: Mar 29, 2015
 *      Author: design
 */

#include "globals.h"
#include "timekeeper.h"
#include "params.h"
#include "calibration.h"
#include "system_settings.h"
#include "leds.h"
#include "dig_pins.h"


volatile uint32_t sys_tmr;

extern uint8_t global_mode[NUM_GLOBAL_MODES];

inline void inc_tmrs(void)
{
	sys_tmr++; //at 48kHz, it resets after 24 hours, 51 minutes, 18.4853333 seconds
}


void init_timekeeper(void){
	NVIC_InitTypeDef nvic;
	EXTI_InitTypeDef   EXTI_InitStructure;

	//Set Priority Grouping mode to 2-bits for priority and 2-bits for sub-priority
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	SYSCFG_EXTILineConfig(EXTI_CLOCK_GPIO, EXTI_CLOCK_pin);
	EXTI_InitStructure.EXTI_Line = EXTI_CLOCK_line;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);


	nvic.NVIC_IRQChannel = EXTI_CLOCK_IRQ;
	nvic.NVIC_IRQChannelPreemptionPriority = 0;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&nvic);
}



// Sample Clock EXTI line (I2S2 LRCLK)
void EXTI_Handler(void)
{
	if(EXTI_GetITStatus(EXTI_CLOCK_line) != RESET)
	{
		inc_tmrs();

		EXTI_ClearITPendingBit(EXTI_CLOCK_line);
	}
}



void init_adc_param_update_timer(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);

	nvic.NVIC_IRQChannel = TIM1_BRK_TIM9_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 3;
	nvic.NVIC_IRQChannelSubPriority = 2;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	//168MHz / prescale=3 ---> 42MHz / 30000 ---> 1.4kHz
	//20000 and 0x1 ==> works well

	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 30000;
	tim.TIM_Prescaler = 0x3;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM9, &tim);

	TIM_ITConfig(TIM9, TIM_IT_Update, ENABLE);

	TIM_Cmd(TIM9, ENABLE);
}

void adc_param_update_IRQHandler(void)
{

	if (TIM_GetITStatus(TIM9, TIM_IT_Update) != RESET) {

		process_adc();

		if (global_mode[CALIBRATE])
		{
			//update_calibration();
			//update_calibrate_leds();
		}
		else
			update_params();


		if (global_mode[SYSTEM_SETTINGS])
		{
			//update_system_settings();
			//update_system_settings_leds();
		}


		//check_entering_system_mode();


		TIM_ClearITPendingBit(TIM9, TIM_IT_Update);

	}
}

