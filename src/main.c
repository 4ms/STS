/*
Stereo Triggered Sampler

main.c
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
#include "bank.h"
#include "sts_fs_index.h"
#include "sts_filesystem.h"
#include "system_settings.h"

uint32_t WATCH0;
uint32_t WATCH1;
uint32_t WATCH2;
uint32_t WATCH3;

FATFS FatFs;

enum g_Errors g_error=0;

__IO uint16_t potadc_buffer[NUM_POT_ADCS];
__IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint8_t 	flags[NUM_FLAGS];

extern SystemCalibrations *system_calibrations;



void check_errors(void);
void check_errors(void){
	if (g_error>0)
	{
		
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
	uint32_t do_factory_reset=0;
	uint32_t timeout_boot;
	uint32_t valid_fw_version;
	FRESULT res;

	
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

	//Codec and I2S/DMA should be disabled before they can properly start up
    Codec_Deinit();
    DeInit_I2S_Clock();
	DeInit_I2SDMA();
	delay();

    //Initialize digital in/out pins
    init_dig_inouts();
 // test_dig_inouts();
	init_timekeeper();


	//Initialize SDRAM memory
	SDRAM_Init();
	if (RAMTEST_BUTTONS)
		RAM_startup_test();
	delay();


	timeout_boot = 0x00800000;
	PLAYLED1_ON;
	while(timeout_boot--){;}
	PLAYLED1_OFF;


	SIGNALLED_OFF;
	BUSYLED_OFF;

	reload_sdcard();

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
	
	//Read the FLASH memory for user params, system calibration settings, etc
	valid_fw_version = load_flash_params();

	//Check for calibration buttons on boot
	if (ENTER_CALIBRATE_BUTTONS)
    {
    	flags[skip_process_buttons] = 1;
    	global_mode[CALIBRATE] = 1;
    }

 	//Check the RAM chip and do a factory reset if we detect a pre-production version of firmware
    else if (!valid_fw_version \
    	|| (system_calibrations->major_firmware_version  < FIRST_PRODUCTION_FW_MAJOR_VERSION) \
    	|| (	(system_calibrations->major_firmware_version == FIRST_PRODUCTION_FW_MAJOR_VERSION) \
    	 	&& 	 system_calibrations->minor_firmware_version <= FIRST_PRODUCTION_FW_MINOR_VERSION)	) 
    {
    	if (RAM_test()==0)
    	{
    		global_mode[CALIBRATE] = 1;
    		do_factory_reset = 960000; //run normally for about 6 seconds before calibrating the CV jacks
    	}
    	else
    		while(1) blink_all_lights(100); //RAM Test failed: It's on the fritz!

    }

    // If we detect a different version, update the firmware version in FLASH
   	if ( 	system_calibrations->major_firmware_version != FW_MAJOR_VERSION
   		|| 	system_calibrations->minor_firmware_version != FW_MINOR_VERSION	)
    {
    	copy_system_calibrations_into_staging();
    	set_firmware_version();
    	write_all_system_calibrations_to_FLASH();
    }

    // Backup index file (unless we are booting for the first time)
    if (!do_factory_reset) backup_sampleindex_file();

    //Begin reading inputs
    init_buttons();
    init_ButtonDebounce_IRQ();
    init_TrigJackDebounce_IRQ();

	//Begin audio DMA
	audio_buffer_init();
	flags[RewriteIndex]=1;
	init_banks();
	flags[RewriteIndex]=0;
	Start_I2SDMA();

	set_default_system_settings();
	read_system_settings();

	delay();

	init_SDIO_read_IRQ();
	
	flags[skip_process_buttons] = 2;

	//Main loop
	while(1){

		check_errors();
		
		write_buffer_to_storage();

		if (flags[TimeToReadStorage])
		{
			flags[TimeToReadStorage]=0;
			read_storage_to_buffer();
		}

		process_mode_flags();

		if (flags[RewriteIndex])
		{
			flags[RewriteIndex] = 0;

			// res = write_sampleindex_file();
			res = index_write_wrapper();
			if (res) {
				g_error |= CANNOT_WRITE_INDEX;
				flags[RewriteIndexFail] = 10;
			}
			else
				flags[RewriteIndexSucess] = 10;
		}

    	if (do_factory_reset)
    		if (!(--do_factory_reset))
    			factory_reset();



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
	JumpTo(0x08008000);

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
	JumpTo(0x08008000);
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

	JumpTo(0x08008000);

	if (foobar){
		return;
	} else {
		while(pause){};
	}
}

void MemManage_Handler(void)
{ 
	JumpTo(0x08008000);
	while(1){};
}

void BusFault_Handler(void)
{ 
	JumpTo(0x08008000);
	while(1){};
}

void UsageFault_Handler(void)
{ 
	JumpTo(0x08008000);
	while(1){};
}

void SVC_Handler(void)
{ 
	JumpTo(0x08008000);
	while(1){};
}

void DebugMon_Handler(void)
{ 
	JumpTo(0x08008000);
	while(1){};
}

void PendSV_Handler(void)
{ 
	JumpTo(0x08008000);
	while(1){};
}
#endif
