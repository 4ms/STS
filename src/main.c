/*
 */
#include <stm32f4xx.h>
#include "globals.h"

#include <sampler.h>
#include <sdram_driver.h>

#include "audio_sdcard.h"
#include "audio_sdram.h"
#include "codec.h"
#include "i2s.h"
#include "adc.h"
#include "params.h"
#include "timekeeper.h"
#include "calibration.h"
#include "flash_user.h"
#include "leds.h"
#include "system_settings.h"
#include "buttons.h"
#include "dig_pins.h"
#include "pca9685_driver.h"
#include "ITM.h"
#include "wav_recording.h"
#include "rgb_leds.h"
#include "ff.h"


enum g_Errors g_error=0;

__IO uint16_t potadc_buffer[NUM_POT_ADCS];
__IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];

//extern uint8_t ButLED_state[NUM_RGBBUTTONS];
//extern uint8_t play_led_state[NUM_PLAY_CHAN];
//extern uint8_t clip_led_state[NUM_PLAY_CHAN];

extern uint32_t flash_firmware_version;

void check_errors(void);

void check_errors(void){
	if (g_error>0)
	{
		CLIPLED1_ON;
		CLIPLED2_ON;

	//while(1){
 		CLIPLED1_ON;
		CLIPLED2_ON;
 /*
			if (g_error & 1) ButLED_state[0]=1;
			if (g_error & 2) ButLED_state[1]=1;
			if (g_error & 4) ButLED_state[2]=1;
			if (g_error & 8) ButLED_state[3]=1;
			if (g_error & 16) ButLED_state[4]=1;
			if (g_error & 32) ButLED_state[5]=1;
			if (g_error & 64) ButLED_state[6]=1;
			if (g_error & 128) ButLED_state[7]=1;
			if (g_error & 256) play_led_state[0]=1;
			if (g_error & 512) clip_led_state[0]=1;
			if (g_error & 1024) clip_led_state[1]=1;
			if (g_error & 2048) play_led_state[1]=1;

			delay_ms(100);

			ButLED_state[0]=0;
			ButLED_state[1]=0;
			ButLED_state[2]=0;
			ButLED_state[3]=0;
			ButLED_state[4]=0;
			ButLED_state[5]=0;
			ButLED_state[6]=0;
			ButLED_state[7]=0;
			play_led_state[0]=0;
			clip_led_state[0]=0;
			clip_led_state[1]=0;
			play_led_state[1]=0;

			delay_ms(100);
			*/

		//}

	}
}

uint8_t check_bootloader_keys(void)
{
	uint32_t dly;
	uint32_t button_debounce=0;

	dly=32000;
	while(dly--){
		if (BOOTLOADER_BUTTONS) button_debounce++;
		else button_debounce=0;
	}
	return (button_debounce>15000);

}

typedef void (*EntryPoint)(void);

void JumpTo(uint32_t address) {
  uint32_t application_address = *(__IO uint32_t*)(address + 4);
  EntryPoint application = (EntryPoint)(application_address);
  __set_MSP(*(__IO uint32_t*)address);
  application();
}




int main(void)
{
	uint8_t i;
	uint8_t err=0;
	uint32_t do_factory_reset=0;

	FATFS FatFs;
	FIL fil;
	FRESULT res;
	uint32_t total, free;
	uint32_t bw;
	BYTE work[_MAX_SS];
    DWORD plist[] = {100, 0, 0, 0};



	//
	// Bootloader:
	//
//    if (check_bootloader_keys())
//    	JumpTo(0x08000000);
//    NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x8000);

	//
	// No bootloader:
	//
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0000);

	ITM_Init(6000000);
//	ITM_Print_int(0,1243);

	//Codec and I2S/DMA should be disabled before they can properly start up
    Codec_Deinit();
    DeInit_I2S_Clock();
	DeInit_I2SDMA();
	delay();

    //Initialize digital in/out pins
    init_dig_inouts();
    test_dig_inouts();
	init_timekeeper();


	//Initialize SDRAM memory
	SDRAM_Init();
	if (RAMTEST_BUTTONS)
		RAM_startup_test();
	delay();


	PLAYLED1_OFF;
	while(!REV1BUT && !REV2BUT  && !PLAY1BUT  && !PLAY2BUT && !BANK1BUT && !BANK2BUT){;}
	PLAYLED1_OFF;

	//Initialize SD Card
	//err=init_sdcard();
	//if (!err)
	//err=test_sdcard();
//	if (err){
//		while(1){
//			CLIPLED1_ON;
//			CLIPLED1_OFF;
//		}
//	}

	CLIPLED1_OFF;
	CLIPLED2_OFF;


	res = f_mount(&FatFs, "", 1);
	if (res != FR_OK)
	{
		//can't mount
		CLIPLED1_ON;
		CLIPLED2_ON;
		res = f_mount(&FatFs, "", 0);

		//while(1){;}
	}


//	res = f_open(&fil, "hello.txt", FA_CREATE_NEW | FA_WRITE);
//	if (res==FR_OK)
//	{
//		res = f_write(&fil, "Hello, World!\r\n", 15, &bw);
//		if (bw==15)
//		{
//			f_close(&fil);
//			f_mount(0,"",0);
//		}
//		else CLIPLED1_ON; //write failed
//	}
//	else CLIPLED2_ON; //file open failed




	//Turn on the lights
	LEDDriver_Init(2);
	LEDDRIVER_OUTPUTENABLE_ON;
	if (PLAY1BUT && BANK1BUT) test_all_buttonLEDs();

	init_buttonLEDs();
	init_ButtonLED_IRQ();
	init_LED_PWM_IRQ();

	//Initialize ADCs
	Deinit_Pot_ADC();
	Deinit_CV_ADC();
	init_LowPassCoefs();
	Init_Pot_ADC((uint16_t *)potadc_buffer, NUM_POT_ADCS);
	Init_CV_ADC((uint16_t *)cvadc_buffer, NUM_CV_ADCS);




	//Initialize Codec
	Codec_GPIO_Init();
	Codec_AudioInterface_Init(I2S_AudioFreq_44k);
	init_audio_dma();
	Codec_Register_Setup(0);

	//Initialize parameters/modes
	global_mode[CALIBRATE] = 0;
	init_adc_param_update_IRQ();
	init_params();
	init_modes();
	flash_firmware_version = load_flash_params();

	//Check for boot modes (calibration, first boot after upgrade)
    if (ENTER_CALIBRATE_BUTTONS)
    {
    	global_mode[CALIBRATE] = 1;
    }
    else if (flash_firmware_version < FW_VERSION ) //If we detect a recently upgraded firmware version
    {
    	set_firmware_version();
    	store_params_into_sram();
    	write_all_params_to_FLASH();
    }


    //Begin reading inputs
    init_ButtonDebounce_IRQ();
    init_TrigJackDebounce_IRQ();

	//Begin audio DMA
	audio_buffer_init();
	Start_I2SDMA();

	delay();



	//Main loop
	while(1){

		//DEBUG0_ON;
		check_errors();
		
		write_buffer_to_storage();

		//DEBUG0_OFF;

		read_storage_to_buffer();

		process_mode_flags();

    	if (do_factory_reset)
    		if (!(--do_factory_reset))
    			factory_reset(1);


	}

	return(1);
}

//#define USE_FULL_ASSERT

#ifdef  USE_FULL_ASSERT

#define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

#if 1
/* exception handlers - so we know what's failing */
void NMI_Handler(void)
{ 
	while(1){};
}

void HardFault_Handler(void)
{ 
	volatile uint8_t foobar;
	uint32_t hfsr,dfsr,afsr,bfar,mmfar,cfsr;

	volatile uint8_t pause=1;

	foobar=0;
	mmfar=SCB->MMFAR;
	bfar=SCB->BFAR;

	hfsr=SCB->HFSR;
	afsr=SCB->AFSR;
	dfsr=SCB->DFSR;
	cfsr=SCB->CFSR;


	if (foobar){
		return;
	} else {
		while(pause){};
	}
}

void MemManage_Handler(void)
{ 
	while(1){};
}

void BusFault_Handler(void)
{ 
	while(1){};
}

void UsageFault_Handler(void)
{ 
	while(1){};
}

void SVC_Handler(void)
{ 
	while(1){};
}

void DebugMon_Handler(void)
{ 
	while(1){};
}

void PendSV_Handler(void)
{ 
	while(1){};
}
#endif
