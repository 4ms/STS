#include "globals.h"
#include "adc.h"
#include "params.h"
#include "looping_delay.h"
#include "timekeeper.h"
#include "audio_memory.h"
#include "dig_pins.h"
#include "buttons.h"

void init_inputread_timer(void){
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB1PeriphClockCmd(DIGINPUT_TIM_RCC, ENABLE);

	nvic.NVIC_IRQChannel = DIGINPUT_TIM_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 3;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	//3000 --> 28kHz
	tim.TIM_Period = 3000;
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(DIGINPUT_TIM, &tim);

	TIM_ITConfig(DIGINPUT_TIM, TIM_IT_Update, ENABLE);

	TIM_Cmd(DIGINPUT_TIM, ENABLE);


/*	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);

	nvic.NVIC_IRQChannel = TIM1_UP_TIM10_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 0;
	nvic.NVIC_IRQChannelSubPriority = 1;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 5000; //every 30uS
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM10, &tim);

	TIM_ITConfig(TIM10, TIM_IT_Update, ENABLE);

	TIM_Cmd(TIM10, ENABLE);
	*/
}

/*

void TIM1_UP_TIM10_IRQHandler(void)
{
	static uint16_t State = 0; // Current debounce status
	uint16_t t;

	if (TIM_GetITStatus(TIM10, TIM_IT_Update) != RESET)
	{

		//if (PINGJACK) t=0xe000;
		//else t=0xe001;

		State=(State<<1) | t;

		if (State==0xfffe) //jack low for 12 times (1ms), then detected high 1 time
		{



		}


		TIM_ClearITPendingBit(TIM10, TIM_IT_Update);
	}
}
*/

void DIGINPUT_IRQHandler(void)
{
	static uint16_t State[10] = {0,0,0,0xFFFF,0xFFFF,0,0,0xFFFF,0xFFFF,0}; // Current debounce status
	uint16_t t;

	// Clear TIM update interrupt
	TIM_ClearITPendingBit(DIGINPUT_TIM, TIM_IT_Update);

	// Check Ping Button

	if (REV1BUT) {
		t=0xe000;
	} else
		t=0xe001;

	State[0]=(State[0]<<1) | t;
	if (State[0]==0xff00){ 	//1111 1111 0000 0000 = not pressed for 8 cycles , then pressed for 8 cycles

	}


}

