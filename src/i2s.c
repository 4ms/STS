/*
 * i2s.c - I2S+DMA routines
 *
 * */

#include "globals.h"
#include "i2s.h"
#include "dig_pins.h"
#include "timekeeper.h"
#include "codec.h"
#include "audio_codec.h"


DMA_InitTypeDef dma_tx, dma_rx;

NVIC_InitTypeDef nvic_i2s2ext, NVIC_InitStructure;


volatile int16_t tx_buffer[codec_BUFF_LEN];
volatile int16_t rx_buffer[codec_BUFF_LEN];


uint32_t tx_buffer_start, rx_buffer_start;

static void (*audio_callback)(int16_t *, int16_t *);

void set_codec_callback(void (*cb)(int16_t *, int16_t *)) {
	audio_callback = cb;
}


void init_audio_dma(void)
{
	RCC_I2SCLKConfig(RCC_I2S2CLKSource_PLLI2S);
	RCC_PLLI2SCmd(ENABLE);

	Init_I2SDMA();

}


void Start_I2SDMA(void)
{
	NVIC_EnableIRQ(AUDIO_I2S2_EXT_DMA_IRQ);
}

void DeInit_I2S_Clock(void){

	RCC_I2SCLKConfig(RCC_I2S2CLKSource_PLLI2S);
	RCC_PLLI2SCmd(DISABLE);
}

void DeInit_I2SDMA(void)
{
	RCC_AHB1PeriphClockCmd(AUDIO_I2S2_DMA_CLOCK, DISABLE);
	DMA_Cmd(AUDIO_I2S2_DMA_STREAM, DISABLE);
	DMA_DeInit(AUDIO_I2S2_DMA_STREAM);

	DMA_Cmd(AUDIO_I2S2_EXT_DMA_STREAM, DISABLE);
	DMA_DeInit(AUDIO_I2S2_EXT_DMA_STREAM);
}


void Init_I2SDMA(void)
{
	uint32_t Size = codec_BUFF_LEN;

	/* Enable the DMA clock */
	RCC_AHB1PeriphClockCmd(AUDIO_I2S2_DMA_CLOCK, ENABLE);

	/* Configure the TX DMA Stream */
	DMA_Cmd(AUDIO_I2S2_DMA_STREAM, DISABLE);
	DMA_DeInit(AUDIO_I2S2_DMA_STREAM);

	dma_tx.DMA_Channel = AUDIO_I2S2_DMA_CHANNEL;
	dma_tx.DMA_PeripheralBaseAddr = AUDIO_I2S2_DMA_DREG;
	dma_tx.DMA_Memory0BaseAddr = (uint32_t)&tx_buffer;
	dma_tx.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	dma_tx.DMA_BufferSize = (uint32_t)Size;
	dma_tx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	dma_tx.DMA_MemoryInc = DMA_MemoryInc_Enable;
	dma_tx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	dma_tx.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma_tx.DMA_Mode = DMA_Mode_Circular;
	dma_tx.DMA_Priority = DMA_Priority_High;
	dma_tx.DMA_FIFOMode = DMA_FIFOMode_Disable;
	dma_tx.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
	dma_tx.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	dma_tx.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(AUDIO_I2S2_DMA_STREAM, &dma_tx);

	//Try TX error checking:
	DMA_ITConfig(AUDIO_I2S2_DMA_STREAM, DMA_IT_FE | DMA_IT_TE | DMA_IT_DME, ENABLE);

	/* Enable the I2S DMA request */
	SPI_I2S_DMACmd(CODEC_I2S, SPI_I2S_DMAReq_Tx, ENABLE);


	/* Configure the RX DMA Stream */
	DMA_Cmd(AUDIO_I2S2_EXT_DMA_STREAM, DISABLE);
	DMA_DeInit(AUDIO_I2S2_EXT_DMA_STREAM);

	/* Set the parameters to be configured */
	dma_rx.DMA_Channel = AUDIO_I2S2_EXT_DMA_CHANNEL;
	dma_rx.DMA_PeripheralBaseAddr = AUDIO_I2S2_EXT_DMA_DREG;
	dma_rx.DMA_Memory0BaseAddr = (uint32_t)&rx_buffer;
	dma_rx.DMA_DIR = DMA_DIR_PeripheralToMemory;
	dma_rx.DMA_BufferSize = (uint32_t)Size;
	dma_rx.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	dma_rx.DMA_MemoryInc = DMA_MemoryInc_Enable;
	dma_rx.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	dma_rx.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma_rx.DMA_Mode = DMA_Mode_Circular;
	dma_rx.DMA_Priority = DMA_Priority_High;
	dma_rx.DMA_FIFOMode = DMA_FIFOMode_Disable;
	dma_rx.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
	dma_rx.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	dma_rx.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(AUDIO_I2S2_EXT_DMA_STREAM, &dma_rx);

	DMA_ITConfig(AUDIO_I2S2_EXT_DMA_STREAM, DMA_IT_TC | DMA_IT_HT | DMA_IT_FE | DMA_IT_TE | DMA_IT_DME, ENABLE);

	/* I2S RX DMA IRQ Channel configuration */

	nvic_i2s2ext.NVIC_IRQChannel = AUDIO_I2S2_EXT_DMA_IRQ;
	nvic_i2s2ext.NVIC_IRQChannelPreemptionPriority = 1; //was 2
	nvic_i2s2ext.NVIC_IRQChannelSubPriority = 1;
	nvic_i2s2ext.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&nvic_i2s2ext);

	//NVIC_EnableIRQ(AUDIO_I2S2_EXT_DMA_IRQ);
	NVIC_DisableIRQ(AUDIO_I2S2_EXT_DMA_IRQ);

	SPI_I2S_DMACmd(CODEC_I2S_EXT, SPI_I2S_DMAReq_Rx, ENABLE);

	tx_buffer_start = (uint32_t)&tx_buffer;
	rx_buffer_start = (uint32_t)&rx_buffer;

	DMA_Init(AUDIO_I2S2_DMA_STREAM, &dma_tx);
	DMA_Init(AUDIO_I2S2_EXT_DMA_STREAM, &dma_rx);

	DMA_Cmd(AUDIO_I2S2_DMA_STREAM, ENABLE);
	DMA_Cmd(AUDIO_I2S2_EXT_DMA_STREAM, ENABLE);

	I2S_Cmd(CODEC_I2S, ENABLE);
	I2S_Cmd(CODEC_I2S_EXT, ENABLE);

}

/**
  * I2S2 DMA interrupt for RX data
  */
void AUDIO_I2S2_EXT_DMA_IRQHandler(void)
{
	int16_t *src, *dst, sz;
	uint32_t err=0;

	//DEBUG0_ON;

	if (DMA_GetFlagStatus(AUDIO_I2S2_EXT_DMA_STREAM, AUDIO_I2S2_EXT_DMA_FLAG_FE) != RESET)
		err=AUDIO_I2S2_EXT_DMA_FLAG_FE;

	if (DMA_GetFlagStatus(AUDIO_I2S2_EXT_DMA_STREAM, AUDIO_I2S2_EXT_DMA_FLAG_TE) != RESET)
		err=AUDIO_I2S2_EXT_DMA_FLAG_TE;

	if (DMA_GetFlagStatus(AUDIO_I2S2_EXT_DMA_STREAM, AUDIO_I2S2_EXT_DMA_FLAG_DME) != RESET)
		err=AUDIO_I2S2_EXT_DMA_FLAG_DME;

	//if (err)
	//	DEBUG3_ON; //debug breakpoint

	/* Transfer complete interrupt */
	if (DMA_GetFlagStatus(AUDIO_I2S2_EXT_DMA_STREAM, AUDIO_I2S2_EXT_DMA_FLAG_TC) != RESET)
	{
		/* Point to 2nd half of buffers */
		sz = codec_BUFF_LEN/2;
		src = (int16_t *)(rx_buffer_start) +sz;
		dst = (int16_t *)(tx_buffer_start) +sz;

		audio_callback(src, dst);

		DMA_ClearFlag(AUDIO_I2S2_EXT_DMA_STREAM, AUDIO_I2S2_EXT_DMA_FLAG_TC);
	}

	/* Half Transfer complete interrupt */
	if (DMA_GetFlagStatus(AUDIO_I2S2_EXT_DMA_STREAM, AUDIO_I2S2_EXT_DMA_FLAG_HT) != RESET)
	{
		/* Point to 1st half of buffers */
		sz = codec_BUFF_LEN/2;
		src = (int16_t *)(rx_buffer_start);
		dst = (int16_t *)(tx_buffer_start);

		audio_callback(src, dst);

		DMA_ClearFlag(AUDIO_I2S2_EXT_DMA_STREAM, AUDIO_I2S2_EXT_DMA_FLAG_HT);
	}
	//DEBUG0_OFF;

}

/*
 * I2S2 DMA interrupt for TX data
 */
void DMA1_Stream4_IRQHandler(void)
{
	uint32_t err=0;

	if (DMA_GetFlagStatus(AUDIO_I2S2_DMA_STREAM, AUDIO_I2S2_DMA_FLAG_FE) != RESET){
		err=AUDIO_I2S2_DMA_FLAG_FE;
		DMA_ClearFlag(AUDIO_I2S2_DMA_STREAM, AUDIO_I2S2_DMA_FLAG_FE);
	}

	if (DMA_GetFlagStatus(AUDIO_I2S2_DMA_STREAM, AUDIO_I2S2_DMA_FLAG_TE) != RESET){
		err=AUDIO_I2S2_DMA_FLAG_TE;
		DMA_ClearFlag(AUDIO_I2S2_DMA_STREAM, AUDIO_I2S2_DMA_FLAG_TE);
	}

	if (DMA_GetFlagStatus(AUDIO_I2S2_DMA_STREAM, AUDIO_I2S2_DMA_FLAG_DME) != RESET){
		err=AUDIO_I2S2_DMA_FLAG_DME;
		DMA_ClearFlag(AUDIO_I2S2_DMA_STREAM, AUDIO_I2S2_DMA_FLAG_DME);
	}
//	if (err)
//		DEBUG3_ON; //debug breakpoint


}

