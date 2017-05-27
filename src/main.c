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
#include "buttons.h"
#include "dig_pins.h"
#include "pca9685_driver.h"
#include "ITM.h"
#include "wav_recording.h"
#include "rgb_leds.h"
#include "ff.h"
#include "sampler.h"

uint32_t WATCH0;
uint32_t WATCH1;
uint32_t WATCH2;
uint32_t WATCH3;

enum g_Errors g_error=0;

__IO uint16_t potadc_buffer[NUM_POT_ADCS];
__IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint8_t 	flags[NUM_FLAGS];

//extern uint8_t ButLED_state[NUM_RGBBUTTONS];
//extern uint8_t play_led_state[NUM_PLAY_CHAN];
//extern uint8_t clip_led_state[NUM_PLAY_CHAN];

void check_errors(void);

void check_errors(void){
	if (g_error>0)
	{
		//SIGNALLED_ON;
		//BUSYLED_ON;

	//while(1){
 		//SIGNALLED_OFF;
		//BUSYLED_OFF;
 
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
	uint32_t timeout_boot;
	uint32_t firmware_version;
	
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
	if (check_bootloader_keys())
		JumpTo(0x08000000);
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x8000);

	//
	// No bootloader:
	//
	// NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0000);

	TRACE_init();
	//ITM_Init(6000000);
//	ITM_Print_int(0,1243);

	//Codec and I2S/DMA should be disabled before they can properly start up
    Codec_Deinit();
    DeInit_I2S_Clock();
	DeInit_I2SDMA();
	delay();

    //Initialize digital in/out pins
    init_dig_inouts();
 //   test_dig_inouts();
	init_timekeeper();


	//Initialize SDRAM memory
	SDRAM_Init();
	if (RAMTEST_BUTTONS)
		RAM_startup_test();
	delay();


	timeout_boot = 0x00800000;
	PLAYLED1_OFF;
	while(timeout_boot--){;}
	PLAYLED1_OFF;

	//Initialize SD Card
	//err=init_sdcard();
	//if (!err)
	//err=test_sdcard();
//	if (err){
//		while(1){
//			SIGNALLED_ON;
//			SIGNALLED_OFF;
//		}
//	}

	SIGNALLED_OFF;
	BUSYLED_OFF;


	res = f_mount(&FatFs, "", 1);
	if (res != FR_OK)
	{
		//can't mount
		SIGNALLED_ON;
		BUSYLED_ON;
		res = f_mount(&FatFs, "", 0);
	}



	//Turn on the lights
	LEDDriver_Init(2);
	LEDDRIVER_OUTPUTENABLE_ON;
	if (PLAY2BUT && BANK2BUT) test_all_buttonLEDs();

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
	
	firmware_version = load_flash_params();

	//Check for boot modes (calibration, first boot after upgrade)
    if (ENTER_CALIBRATE_BUTTONS)
    {
    	flags[skip_process_buttons] = 1;
    	global_mode[CALIBRATE] = 1;
    }
    else if (firmware_version < FW_VERSION ) //If we detect a recently upgraded firmware version
    {
    	copy_system_calibrations_into_staging();
    	set_firmware_version();
    	write_all_system_calibrations_to_FLASH();
    }


    //Begin reading inputs
    init_buttons();
    init_ButtonDebounce_IRQ();
    init_TrigJackDebounce_IRQ();

	//Begin audio DMA
	audio_buffer_init();
	Start_I2SDMA();

	delay();

	init_SDIO_read_IRQ();

	//Main loop
	while(1){

		//DEBUG0_ON;
		check_errors();
		
		write_buffer_to_storage();

		//DEBUG0_OFF;

		if (flags[TimeToReadStorage])
		{
			flags[TimeToReadStorage]=0;
			read_storage_to_buffer();
		}

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
