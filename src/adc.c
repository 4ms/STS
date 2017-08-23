/*
 * adc.c - adc setup
 */
#include "globals.h"
#include "adc.h"

void Deinit_Pot_ADC(void)
{

	ADC_Cmd(ADC1, DISABLE);
	ADC_DMACmd(ADC1, DISABLE);
	ADC_DMARequestAfterLastTransferCmd(ADC1, DISABLE);
	DMA_Cmd(DMA2_Stream4, DISABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, DISABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, DISABLE);
}

void Deinit_CV_ADC(void)
{

	ADC_Cmd(ADC3, DISABLE);
	ADC_DMACmd(ADC3, DISABLE);
	ADC_DMARequestAfterLastTransferCmd(ADC3, DISABLE);
	DMA_Cmd(DMA2_Stream0, DISABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3, DISABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, DISABLE);

}

void Init_Pot_ADC(uint16_t *ADC_Buffer, uint8_t num_adcs)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	/* enable clocks for DMA2, ADC1 ----------------------------------*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	/* DMA2 stream4 channel0 configuration ----------------------------------*/
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;  
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)ADC_Buffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = num_adcs;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;         
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream4, &DMA_InitStructure);
	DMA_Cmd(DMA2_Stream4, ENABLE);

	/* ADC Common Init ------------------------------------------------------*/
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div8;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC1 Init ------------------------------------------------------------*/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = num_adcs;
	ADC_Init(ADC1, &ADC_InitStructure);
	
	/* Configure analog input pins ------------------------------------------*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7; //PA5, PA6, PA7 = Channel 5, 6, 7
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; //PB0, PB1 = Channel 8, 9
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5; //PC2, PC3 = Channel 12, 13, 14, 15
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/* ADC1 regular channel configuration -----------------------------------*/ 
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, PITCH1_POT+1, ADC_SampleTime_144Cycles); //[0]: PA6
	ADC_RegularChannelConfig(ADC1, ADC_Channel_7, PITCH2_POT+1, ADC_SampleTime_144Cycles); //[1]: PA7
	ADC_RegularChannelConfig(ADC1, ADC_Channel_12, START1_POT+1, ADC_SampleTime_144Cycles); //[2]: PC2
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, START2_POT+1, ADC_SampleTime_144Cycles); //[3]: PC3
	ADC_RegularChannelConfig(ADC1, ADC_Channel_14, SAMPLE1_POT+1, ADC_SampleTime_144Cycles); //[4]: PC4
	ADC_RegularChannelConfig(ADC1, ADC_Channel_15, SAMPLE2_POT+1, ADC_SampleTime_144Cycles); //[5]: PC5
	ADC_RegularChannelConfig(ADC1, ADC_Channel_9, LENGTH1_POT+1, ADC_SampleTime_144Cycles); //[6]: PB1
	ADC_RegularChannelConfig(ADC1, ADC_Channel_8, LENGTH2_POT+1, ADC_SampleTime_144Cycles); //[7]: PB0
	ADC_RegularChannelConfig(ADC1, ADC_Channel_5, RECSAMPLE_POT+1, ADC_SampleTime_144Cycles); //[8]: PA5


	//DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);
   	//NVIC_EnableIRQ(DMA2_Stream0_IRQn);
	
	ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
	ADC_DMACmd(ADC1, ENABLE);
	ADC_Cmd(ADC1, ENABLE);
	ADC_SoftwareStartConv(ADC1);
}

/*
void DMA2_Stream4_IRQHandler(void)
{ 
	if (DMA_GetFlagStatus(DMA2_Stream4, DMA_FLAG_TCIF0) != RESET)
	{
		DMA_ClearFlag(DMA2_Stream4, DMA_FLAG_TCIF0);
		//ADC_SoftwareStartConv(ADC1);
	}
}
*/

void Init_CV_ADC(uint16_t *ADC_Buffer, uint8_t num_adcs)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	/* enable clocks for DMA2, ADC3  ----------------------------------*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3, ENABLE);

	/* DMA2 stream0 channel2 configuration ----------------------------------*/
	DMA_InitStructure.DMA_Channel = DMA_Channel_2;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC3->DR;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)ADC_Buffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = num_adcs;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_Low;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream0, &DMA_InitStructure);
	DMA_Cmd(DMA2_Stream0, ENABLE);

	/* ADC Common Init ------------------------------------------------------*/
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;

	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC3 Init ------------------------------------------------------------*/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = num_adcs;
	ADC_Init(ADC3, &ADC_InitStructure);

	/* Configure analog input pins ------------------------------------------*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOF, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2; //PA0, PA1, PA2 = Channel 0, 1, 2
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10; //PF6, PF7, PF8, PF9, PF10 = Channel 4, 5, 6, 7, 8
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	/* ADC3 regular channel configuration -----------------------------------*/
	ADC_RegularChannelConfig(ADC3, ADC_Channel_4, PITCH1_CV+1, ADC_SampleTime_480Cycles); //PF6
	ADC_RegularChannelConfig(ADC3, ADC_Channel_5, PITCH2_CV+1, ADC_SampleTime_480Cycles); //PF7
	ADC_RegularChannelConfig(ADC3, ADC_Channel_8, START1_CV+1, ADC_SampleTime_3Cycles); //PF10
	ADC_RegularChannelConfig(ADC3, ADC_Channel_0, START2_CV+1, ADC_SampleTime_3Cycles); //PA0
	ADC_RegularChannelConfig(ADC3, ADC_Channel_6, LENGTH1_CV+1, ADC_SampleTime_3Cycles); //PF8
	ADC_RegularChannelConfig(ADC3, ADC_Channel_7, LENGTH2_CV+1, ADC_SampleTime_3Cycles); //PF9
	ADC_RegularChannelConfig(ADC3, ADC_Channel_1, SAMPLE1_CV+1, ADC_SampleTime_3Cycles); //PA1
	ADC_RegularChannelConfig(ADC3, ADC_Channel_2, SAMPLE2_CV+1, ADC_SampleTime_480Cycles); //PA2

	//DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);
	//NVIC_EnableIRQ(DMA2_Stream0_IRQn);

	ADC_DMARequestAfterLastTransferCmd(ADC3, ENABLE);
	ADC_DMACmd(ADC3, ENABLE);
	ADC_Cmd(ADC3, ENABLE);
	ADC_SoftwareStartConv(ADC3);
}


/*
void DMA2_Stream0_IRQHandler(void)
{

	if (DMA_GetFlagStatus(DMA2_Stream0, DMA_FLAG_TCIF2) != RESET)
	{
		DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF2);

		//ADC_SoftwareStartConv(ADC3);
	}

}
*/
