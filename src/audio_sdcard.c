/*
 * audio_sdcard.c

 *
 *  Created on: Jun 8, 2014
 *      Author: design
 */

/*
#include "audio_sdcard.h"
#include "dig_pins.h"
#include "globals.h"
#include "stm32f4_discovery_sdio_sd.h"
//#include "fatfs_sd_sdio.h"


//__IO uint16_t MEMORY[SPIRAM_END_ADDR];


uint8_t write_sdcard(uint16_t *data, uint32_t addr){
	SD_Error err=0;

	//SD_WaitTransmissionEnd();
	//err=SD_WriteSingleBlock((uint8_t *)data, addr);


	err = SD_WriteBlock((uint8_t *)data, addr*512,512);
	if (err==SD_OK){
		err = SD_WaitWriteOperation();
		while(SD_GetStatus() != SD_TRANSFER_OK);
	}
	return(err);

}

uint8_t read_sdcard(uint16_t *data, uint32_t addr){
	SD_Error err=0;


	//err=SD_ReadSingleBlock((uint8_t *)data, addr);

	err=SD_ReadBlock((uint8_t *)data, addr*512, 512);
	if (err==SD_OK){
		err = SD_WaitReadOperation();
		while(SD_GetStatus() != SD_TRANSFER_OK);
	}

	return(err);
}


NVIC_InitTypeDef NVIC_InitStructure;

uint8_t init_sdcard(void){
	SD_Error err=0;

	//SD_LowLevel_Init();
	err=SD_Init();

   // SDIO Interrupt ENABLE
   NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
   NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
   NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
   NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
   NVIC_Init(&NVIC_InitStructure);

   // DMA2 STREAMx Interrupt ENABLE
   NVIC_InitStructure.NVIC_IRQChannel = SD_SDIO_DMA_IRQn;
   NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
   NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
   NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

   NVIC_Init(&NVIC_InitStructure);

	return(err);
}

uint8_t test_sdcard(void){
	uint16_t num_bytes=256;
	uint16_t test[256], test2[256];
	uint32_t i;
	uint8_t err=0;


	for (i=0;i<num_bytes;i++){
		test[i]=(i<<6) + 19;
		test2[i]=0;
	}

	err=write_sdcard(test, 0);
	err+=read_sdcard(test2, 0);

	i=0;
	//i=180;
	//i=90;


	for (i=0;i<num_bytes;i++){
		if (test[i]!=test2[i])
		{
			return(99);
		}
	}

	for (i=0;i<num_bytes;i++){
		test[i]=(i<<4) + 13;
		test2[i]=0;
	}

	err=write_sdcard(test, 1);
	err+=read_sdcard(test2, 1);
	for (i=0;i<num_bytes;i++){
		if (test[i]!=test2[i])
		{
			return(98);
		}
	}


	return(err);
}

*/

