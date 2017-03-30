/*-----------------------------------------------------------------------*/
/* Mash up of diskio.c from fatfs v.12b and TM's project        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control module to the FatFs module with a defined API.        */
/*-----------------------------------------------------------------------*/
#include <string.h>
#include "diskio.h"		/* FatFs lower layer API */
#include "ff.h"

PARTITION VolToPart[] = {
    {0, 1},     /* "0:" ==> Physical drive 0, 1st partition */
    {0, 2},     /* "1:" ==> Physical drive 0, 2nd partition */
    {0, 3},     /* "2:" ==> Physical drive 0, 3rd partition */
    {0, 4}      /* "3:" ==> Physical drive 0, auto detection */
};


/* Set in defines.h file if you want it */
#ifndef TM_FATFS_CUSTOM_FATTIME
	#define TM_FATFS_CUSTOM_FATTIME		0
#endif


#ifndef FATFS_USE_SDIO
	#define FATFS_USE_SDIO			1
#endif

#include "stm32f4_discovery_sdio_sd.h"
//#include "fatfs_sd_sdio.h"

volatile uint32_t accessing=0;

/* Definitions of physical drive number for each media */
//#define ATA		   0
//#define USB		   1
//#define SDRAM      2
//#define SPI_FLASH  3


/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber (0..) */
)
{
	uint8_t res;

	if (accessing) return 99;
	accessing |= (1<<0);
	res = TM_FATFS_SD_SDIO_disk_initialize();
	accessing &= ~(1<<0);

	return res;
}

/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber (0..) */
)
{
	return(0);

//	uint8_t res;
//
//	if (accessing) return 99;
//	accessing |= (1<<1);
//	res = TM_FATFS_SD_SDIO_disk_status();
//	accessing &= ~(1<<1);
//
//	return res;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
	uint8_t res;

	/* Check count */
	if (!count) {
		return RES_PARERR;
	}
	
	if (accessing)
		return 99;
	accessing |= (1<<2);
	res = DG_disk_read(buff,sector,count);
	//	res = TM_FATFS_SD_SDIO_disk_read(buff, sector, count);
	accessing &= ~(1<<2);

	return res;

}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
	uint8_t res;

	/* Check count */
	if (!count) {
		return RES_PARERR;
	}
	
	if (accessing) return 99;
	accessing |= (1<<3);
	res = TM_FATFS_SD_SDIO_disk_write(buff,sector,count);
	accessing &= ~(1<<3);

	return res;

}


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	uint8_t res;

	if (accessing) return 99;
	accessing |= (1<<4);
	res = TM_FATFS_SD_SDIO_disk_ioctl(cmd, buff);
	accessing &= ~(1<<4);

	return res;

}



/*-----------------------------------------------------------------------*/
/* Get time for fatfs for files                                          */
/*-----------------------------------------------------------------------*/
__weak DWORD get_fattime(void) {
	/* Returns current time packed into a DWORD variable */
	return	  ((DWORD)(2016 - 1980) << 25)	/* Year 2016 */
			| ((DWORD)2 << 21)				/* Month 2 */
			| ((DWORD)22 << 16)				/* Mday 22 */
			| ((DWORD)21 << 11)				/* Hour 21 */
			| ((DWORD)10 << 5)				/* Min 10 */
			| ((DWORD)0 >> 1);				/* Sec 0 */
}


static volatile DSTATUS TM_FATFS_SD_SDIO_Stat = STA_NOINIT;	/* Physical drive status */

#define BLOCK_SIZE            512

uint8_t TM_FATFS_SDIO_WriteEnabled(void) {
#if FATFS_USE_WRITEPROTECT_PIN > 0
	return !((FATFS_USE_WRITEPROTECT_PIN_PORT)->IDR & (FATFS_USE_WRITEPROTECT_PIN_PIN));
//	return !TM_GPIO_GetInputPinValue(FATFS_USE_WRITEPROTECT_PIN_PORT, FATFS_USE_WRITEPROTECT_PIN_PIN);
#else
	return 1;
#endif
}

DSTATUS TM_FATFS_SD_SDIO_disk_initialize(void) {
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef gpio;
	GPIO_StructInit(&gpio);


	/* Detect pin */
#if FATFS_USE_DETECT_PIN > 0
	//TM_GPIO_Init(FATFS_USE_DETECT_PIN_PORT, FATFS_USE_DETECT_PIN_PIN, GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP, GPIO_Low_Speed);
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	gpio.GPIO_Pin = FATFS_USE_DETECT_PIN_PIN;
	GPIO_Init(FATFS_USE_DETECT_PIN_PORT, &gpio);
#endif

	/* Write protect pin */
#if FATFS_USE_WRITEPROTECT_PIN > 0
	//TM_GPIO_Init(FATFS_USE_WRITEPROTECT_PIN_PORT, FATFS_USE_WRITEPROTECT_PIN_PIN, GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP, GPIO_Low_Speed);
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	gpio.GPIO_Pin = FATFS_USE_WRITEPROTECT_PIN_PIN;
	GPIO_Init(FATFS_USE_WRITEPROTECT_PIN_PORT, &gpio);
#endif

	// Configure the NVIC Preemption Priority Bits
	NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init (&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = SD_SDIO_DMA_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init (&NVIC_InitStructure);

	SD_LowLevel_DeInit();
	SD_LowLevel_Init();

	//Check disk initialized
	if (SD_Init() == SD_OK) {
		TM_FATFS_SD_SDIO_Stat &= ~STA_NOINIT;	/* Clear STA_NOINIT flag */
	} else {
		TM_FATFS_SD_SDIO_Stat |= STA_NOINIT;
	}
	//Check write protected
	if (!TM_FATFS_SDIO_WriteEnabled()) {
		TM_FATFS_SD_SDIO_Stat |= STA_PROTECT;
	} else {
		TM_FATFS_SD_SDIO_Stat &= ~STA_PROTECT;
	}

	return TM_FATFS_SD_SDIO_Stat;
}

DSTATUS TM_FATFS_SD_SDIO_disk_status(void) {
	if (SD_Detect() != SD_PRESENT) { //always returns SD_PRESENT
		return STA_NOINIT;			//never
	}

	if (!TM_FATFS_SDIO_WriteEnabled()) { //always returns 1
		TM_FATFS_SD_SDIO_Stat |= STA_PROTECT; //never
	} else {
		TM_FATFS_SD_SDIO_Stat &= ~STA_PROTECT;
	}

	return TM_FATFS_SD_SDIO_Stat;
}

DRESULT DG_disk_read(BYTE *data, DWORD addr, UINT count)
{
	SD_Error err=0;
	uint16_t i;
	int16_t a, b,c;


	err = SD_ReadMultiBlocksFIXED(data, addr, 512, count);

	//err=SD_ReadBlock(data, addr*512, 512);
	if (err==SD_OK)
	{
		err = SD_WaitReadOperation();

		for(i=5;i<512;i+=2)
		{
			a = (data[i-4] << 8) + data[i-5];
			b = (data[i] << 8) + data[i-1];

			if (a > b)
				c = a - b;
			else
				c = b - a;
			if (c > 0x2000)
				c = 0xFFFF;
			
		}

		if (err)
			return(err);

		while(SD_GetStatus() != SD_TRANSFER_OK);
	}
	else
	{
		return(err);
	}

//	while (--count)
//	{
//		data += 512;
//		addr++;
//
//		err=SD_ReadBlock(data, addr*512, 512);
//		if (err==SD_OK){
//			err = SD_WaitReadOperation();
//			while(SD_GetStatus() != SD_TRANSFER_OK);
//		}
//
//	}

	return(err);
}

DRESULT TM_FATFS_SD_SDIO_disk_read(BYTE *buff, DWORD sector, UINT count) {
	SD_Error Status = SD_OK;

	if ((TM_FATFS_SD_SDIO_Stat & STA_NOINIT)) {
		return RES_NOTRDY;
	}

	if ((DWORD)buff & 3) {
		DRESULT res = RES_OK;
		DWORD scratch[BLOCK_SIZE / 4];

		while (count--) {
			res = TM_FATFS_SD_SDIO_disk_read((void *)scratch, sector++, 1);

			if (res != RES_OK) {
				break;
			}

			memcpy(buff, scratch, BLOCK_SIZE);

			buff += BLOCK_SIZE;
		}

		return res;
	}

//	Status = SD_ReadMultiBlocks(buff, sector << 9, BLOCK_SIZE, count);
	Status = SD_ReadMultiBlocksFIXED(buff, sector << 9, BLOCK_SIZE, count);

	if (Status == SD_OK) {
		SDTransferState State;

		Status = SD_WaitReadOperation();

		while ((State = SD_GetStatus()) == SD_TRANSFER_BUSY);

		if ((State == SD_TRANSFER_ERROR) || (Status != SD_OK)) {
			return RES_ERROR;
		} else {
			return RES_OK;
		}
	} else {
		return RES_ERROR;
	}
}

DRESULT TM_FATFS_SD_SDIO_disk_write(const BYTE *buff, DWORD sector, UINT count) {
	SD_Error Status = SD_OK;

	if (!TM_FATFS_SDIO_WriteEnabled()) {
		return RES_WRPRT;
	}

	if (SD_Detect() != SD_PRESENT) {
		return RES_NOTRDY;
	}

	if ((DWORD)buff & 3) {
		DRESULT res = RES_OK;
		DWORD scratch[BLOCK_SIZE / 4];

		while (count--) {
			memcpy(scratch, buff, BLOCK_SIZE);
			res = TM_FATFS_SD_SDIO_disk_write((void *)scratch, sector++, 1);

			if (res != RES_OK) {
				break;
			}

			buff += BLOCK_SIZE;
		}

		return(res);
	}

	Status = SD_WriteMultiBlocks((uint8_t *)buff, sector << 9, BLOCK_SIZE, count); // 4GB Compliant

	if (Status == SD_OK) {
		SDTransferState State;

		Status = SD_WaitWriteOperation(); // Check if the Transfer is finished

		while ((State = SD_GetStatus()) == SD_TRANSFER_BUSY); // BUSY, OK (DONE), ERROR (FAIL)

		if ((State == SD_TRANSFER_ERROR) || (Status != SD_OK)) {
			return RES_ERROR;
		} else {
			return RES_OK;
		}
	} else {
		return RES_ERROR;
	}
}

DRESULT TM_FATFS_SD_SDIO_disk_ioctl(BYTE cmd, void *buff) {
	switch (cmd) {
		case GET_SECTOR_SIZE :     // Get R/W sector size (WORD)
			*(WORD *) buff = 512;
		break;
		case GET_BLOCK_SIZE :      // Get erase block size in unit of sector (DWORD)
			*(DWORD *) buff = 32;
		break;
		case GET_SECTOR_COUNT:
			*(DWORD *) buff = 4096; //2GB x 1024MB/GB x 1024B/MB / 512B/sector
		break;
		case CTRL_SYNC :
//		case CTRL_ERASE_SECTOR :
		case CTRL_TRIM :
		break;
	}

	return RES_OK;
}


