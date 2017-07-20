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
#include "dig_pins.h"

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
	BUSYLED_ON;
	accessing |= (1<<0);
	res = sdio_disk_initialize();
	accessing &= ~(1<<0);
	BUSYLED_OFF;

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
//	res = sdio_disk_status();
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
	uint32_t timeout=0x00FFFFFF;

	/* Check count */
	if (!count) {
		return RES_PARERR;
	}
	
	while (accessing)
	{
		if (timeout--==0)
			return 99;
	}
	BUSYLED_ON;
	accessing |= (1<<2);
	res = sdio_disk_read(buff,sector,count);
	accessing &= ~(1<<2);
	BUSYLED_OFF;

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
	uint32_t timeout=0x00FFFFFF;

	/* Check count */
	if (!count) {
		return RES_PARERR;
	}
	
	if (sector == 0) //<8192?
		return RES_PARERR;

	while (accessing)
	{
		if (timeout--==0)
			return 99;
	}
	BUSYLED_ON;
	accessing |= (1<<3);
	res = sdio_disk_write(buff,sector,count);
	accessing &= ~(1<<3);
	BUSYLED_OFF;
	
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
	res = sdio_disk_ioctl(cmd, buff);
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


static volatile DSTATUS sdio_status = STA_NOINIT;	/* Physical drive status */

#define BLOCK_SIZE            		512

#define FATFS_USE_DETECT_PIN 		0
//#define FATFS_USE_DETECT_PIN_PORT
//#define FATFS_USE_DETECT_PIN_PIN

#define FATFS_USE_WRITEPROTECT_PIN 	0
//#define FATFS_USE_WRITEPROTECT_PIN_PORT
//#define FATFS_USE_WRITEPROTECT_PIN_PIN


uint8_t sdio_write_enabled(void) {
#if FATFS_USE_WRITEPROTECT_PIN > 0
	return !((FATFS_USE_WRITEPROTECT_PIN_PORT)->IDR & (FATFS_USE_WRITEPROTECT_PIN_PIN));
#else
	return 1;
#endif
}


//
// sdio_disk_initialize()
//
DSTATUS sdio_disk_initialize(void) {
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef gpio;
	GPIO_StructInit(&gpio);


	/* Detect pin */
#if FATFS_USE_DETECT_PIN > 0
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	gpio.GPIO_Pin = FATFS_USE_DETECT_PIN_PIN;
	GPIO_Init(FATFS_USE_DETECT_PIN_PORT, &gpio);
#endif

	/* Write protect pin */
#if FATFS_USE_WRITEPROTECT_PIN > 0
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
	if (SD_Init() == SD_OK)	sdio_status &= ~STA_NOINIT;
	else 					sdio_status |= STA_NOINIT;
	
	//Check write protected
#if FATFS_USE_WRITEPROTECT_PIN > 0
	if (!sdio_write_enabled())	sdio_status |= STA_PROTECT;
	else 
#endif
								sdio_status &= ~STA_PROTECT;
	
	return sdio_status;
}

//
// sdio_disk_status()
//
DSTATUS sdio_disk_status(void) {
	if (SD_Detect() != SD_PRESENT) {
		return STA_NOINIT;
	}

#if FATFS_USE_WRITEPROTECT_PIN > 0
	if (!sdio_write_enabled())	sdio_status |= STA_PROTECT;
	else 
#endif
								sdio_status &= ~STA_PROTECT;

	return sdio_status;
}


//
// sdio_disk_read()
//
DRESULT sdio_disk_read(BYTE *data, DWORD addr, UINT count)
{
	SD_Error 		err=0;
	SDTransferState State;

	err = SD_ReadMultiBlocksFIXED(data, addr, BLOCK_SIZE, count);

	if (err==SD_OK)
	{
		err = SD_WaitReadOperation();

		//while(SD_GetStatus() != SD_TRANSFER_OK);
		while ((State = SD_GetStatus()) == SD_TRANSFER_BUSY);

		if ((State == SD_TRANSFER_ERROR) || (err != SD_OK))
			return RES_ERROR;
		else
			return RES_OK;
	}

	return(err);
}

//
// sdio_disk_write()
//
DRESULT sdio_disk_write(const BYTE *buff, DWORD sector, UINT count)
{
	SD_Error 		err = 0;
	SDTransferState State;

#if FATFS_USE_WRITEPROTECT_PIN > 0
	if (!sdio_write_enabled()) {
		return RES_WRPRT;
	}
#endif

#if FATFS_USE_DETECT_PIN > 0
	if (SD_Detect() != SD_PRESENT) {
		return RES_NOTRDY;
	}
#endif

//	err = SD_WriteMultiBlocksFIXED((uint8_t *)buff, sector << 9, BLOCK_SIZE, count);
	err = SD_WriteMultiBlocksFIXED((uint8_t *)buff, sector, BLOCK_SIZE, count);

	if (err == SD_OK) {

		err = SD_WaitWriteOperation();

		while ((State = SD_GetStatus()) == SD_TRANSFER_BUSY); // BUSY, OK (DONE), ERROR (FAIL)

		if ((State == SD_TRANSFER_ERROR) || (err != SD_OK))
			return RES_ERROR;
		else
			return RES_OK;
	}
		
	return(err);
}


//
// sdio_disk_ioctl()
//
DRESULT sdio_disk_ioctl(BYTE cmd, void *buff) {
	uint32_t timeout=0xFFFFFFFF;

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
			while (accessing & (1<<3)){ //wait until bit 3 goes low, indicating a write operation is done
				if (!timeout--)
					return(RES_NOTRDY);
			}
		break;
//		case CTRL_ERASE_SECTOR :
		case CTRL_TRIM :
		break;
	}

	return RES_OK;
}


