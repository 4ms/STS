/*
 * timekeeper.c - routines that run on timer-based interrupts
 *
 * Authors: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplo@gmail.com)
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

#include "globals.h"
#include "timekeeper.h"
#include "params.h"
#include "calibration.h"
#include "leds.h"
#include "dig_pins.h"

volatile uint32_t sys_tmr;

uint32_t get_fattime(void){
	uint32_t secs, mins, hours, days, month;

	secs = sys_tmr/ONE_SECOND;
	mins = 10;
	hours = 21;
	days = 22;
	month = 2;

	mins += secs / 60;
	secs = secs % 60;

	hours += mins / 60;
	mins = mins % 60;

	days += hours / 24;
	hours = hours % 24;

	month += days / 28;
	days = days % 28; //February

	return	  ((uint32_t)(2016 - 1980) << 25)
			| ((uint32_t)month << 21)
			| ((uint32_t)days << 16)
			| ((uint32_t)hours << 11)
			| ((uint32_t)mins << 5)
			| ((uint32_t)secs >> 1);
}

void SysTick_Handler(void)
{
	sys_tmr++;
}

void init_timekeeper(void){
	//Set Priority Grouping mode to 2-bits for priority and 2-bits for sub-priority
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	//Start SysTick
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
	SysTick_Config(SystemCoreClock / ONE_SECOND); //updates about the same as a 44.1kHz sample rate, so one sys_tmr value is one 22.6us
}


void init_adc_param_update_IRQ(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);

	nvic.NVIC_IRQChannel = TIM1_BRK_TIM9_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 2;
	nvic.NVIC_IRQChannelSubPriority = 2;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	//168MHz / prescale=3 --> 168/4 ---> 42MHz / 30000 ---> 1.4kHz

	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 30000;
	tim.TIM_Prescaler = 0x3;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM9, &tim);
	TIM_ITConfig(TIM9, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM9, ENABLE);
}


void init_LED_PWM_IRQ(void)
{
	TIM_TimeBaseInitTypeDef tim;
	NVIC_InitTypeDef nvic;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	nvic.NVIC_IRQChannel = TIM2_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 0;
	nvic.NVIC_IRQChannelSubPriority = 3;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 35000; //168MHz / 2 / 35000 = 4.8kHz (208.3us) ... / 32 =
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM2, &tim);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM2, ENABLE);
}

void init_ButtonDebounce_IRQ(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	nvic.NVIC_IRQChannel = TIM4_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 3;
	nvic.NVIC_IRQChannelSubPriority = 3;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	//30000 --> 2.8kHz
	tim.TIM_Period = 30000;
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM4, &tim);
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM4, ENABLE);
}

void init_ButtonLED_IRQ(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);

	nvic.NVIC_IRQChannel = TIM1_UP_TIM10_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 3;
	nvic.NVIC_IRQChannelSubPriority = 2;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);


	//Run every 2ms (500Hz)
	//Prescale = 11 --> 168MHz / 12 = 14MHz
	//Period = 27999 --> 14MHz / 28000 = 500Hz

	//Run every 5ms (200Hz)
	//Prescale = 31 --> 168MHz / 32 = 5.25MHz
	//Period = 26249 --> 5.25MHz / 26250 = 200Hz

	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 26249;
	tim.TIM_Prescaler = 31;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM10, &tim);
	TIM_ITConfig(TIM10, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM10, ENABLE);
}

void init_TrigJackDebounce_IRQ(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

	nvic.NVIC_IRQChannel = TIM5_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 0;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	//7500 --> 12.0kHz 0.083ms
	tim.TIM_Period = 7500;
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM5, &tim);
	TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM5, ENABLE);
}

void init_SDIO_read_IRQ(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);

	nvic.NVIC_IRQChannel = TIM7_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 1;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	//30000 --> 1.4kHz
	tim.TIM_Period = 30000; 
	tim.TIM_Prescaler = 2;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM7, &tim);
	TIM_ITConfig(TIM7, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM7, ENABLE);
}
