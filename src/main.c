/*
Stereo Triggered Sampler

main.c
 */
#include "globals.h"
#include <stm32f4xx.h>

#include "sampler.h"
#include "sdram_driver.h"

#include "ITM.h"
#include "adc.h"
#include "audio_codec.h"
#include "audio_sdram.h"
#include "bank.h"
#include "buttons.h"
#include "calibration.h"
#include "codec.h"
#include "dig_pins.h"
#include "edit_mode.h"
#include "ff.h"
#include "flash_user.h"
#include "hardware_tests.h"
#include "i2s.h"
#include "leds.h"
#include "params.h"
#include "pca9685_driver.h"
#include "rgb_leds.h"
#include "sampler.h"
#include "stm32f4_discovery_sdio_sd.h"
#include "sts_filesystem.h"
#include "sts_fs_index.h"
#include "system_mode.h"
#include "timekeeper.h"
#include "user_settings.h"
#include "wav_recording.h"

#define HAS_BOOTLOADER

// These must match what's in startup_stm32f4xx.s:
#define BOOTLOADER_MAGIC_CODE 0xBADDCAFE
#define BOOTLOADER_MAGIC_ADDRESS *((uint32_t *)(0x2001FFF0))

uint8_t PCB_version = 0;

FATFS FatFs;

enum g_Errors g_error = 0;

__IO uint16_t potadc_buffer[NUM_POT_ADCS];
__IO uint16_t cvadc_buffer[NUM_CV_ADCS];

extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint8_t flags[NUM_FLAGS];
extern uint32_t flags32[NUM_FLAGS];

extern SystemCalibrations *system_calibrations;

void check_errors(void);
void check_errors(void) {
  if (g_error > 0) {
  }
}

uint8_t check_bootloader_keys(void) {
  uint32_t dly;
  uint32_t button_debounce = 0;

  dly = 32000;
  while (dly--) {
    if (BOOTLOADER_BUTTONS)
      button_debounce++;
    else
      button_debounce = 0;
  }
  return (button_debounce > 15000);
}

void deinit_all(void);
void deinit_all(void) {
  f_mount(0, "", 0);
  SD_DeInit();
  Deinit_CV_ADC();
  Deinit_Pot_ADC();
  Codec_Deinit();
  DeInit_I2S_Clock();
  DeInit_I2SDMA();
  deinit_dig_inouts();
}

int main(void) {
  uint32_t do_factory_reset = 0;
  uint32_t timeout_boot;
  uint32_t valid_fw_version;
  FRESULT res;

  SD_DeInit();

  LEDDRIVER_OUTPUTENABLE_OFF;

#ifndef HAS_BOOTLOADER
  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0000);
#endif

  TRACE_init();
  // ITM_Init(6000000);

  // Codec and I2S/DMA should be disabled before they can properly start up
  Codec_Deinit();
  DeInit_I2S_Clock();
  DeInit_I2SDMA();
  delay();

#ifdef HAS_BOOTLOADER
  if (check_bootloader_keys()) {
    BOOTLOADER_MAGIC_ADDRESS = BOOTLOADER_MAGIC_CODE;
    NVIC_SystemReset();
  }
  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x8000);
#endif

  init_dig_inouts();

  PCB_version = get_PCB_version();

  if (PCB_version ==
      0xFF) // invalid version: display error code: all LEDs on for a second
  {
    timeout_boot = 0x08000000;
    PLAYLED1_ON;
    PLAYLED2_ON;
    SIGNALLED_ON;
    BUSYLED_ON;
    while (timeout_boot--) {
      ;
    }
    PLAYLED1_OFF;
    PLAYLED2_OFF;
    SIGNALLED_OFF;
    BUSYLED_OFF;
  }

  // Turn on middle lights, for debugging
  SIGNALLED_ON;
  BUSYLED_ON;

  init_timekeeper();

  SDRAM_Init();

  if (HARDWARETEST_BUTTONS)
    do_hardware_test();
  delay();

  timeout_boot = 0x00800000;
  PLAYLED1_ON;
  while (timeout_boot--) {
    ;
  }
  PLAYLED1_OFF;

  // Turn off middle lights, for debugging
  SIGNALLED_OFF;
  BUSYLED_OFF;

  reload_sdcard();

  // Turn on the lights
  LEDDriver_Init(2); // 2 = # of LED driver chips
  all_buttonLEDs_off();

  global_mode[CALIBRATE] = 0;

  valid_fw_version = load_flash_params();

  // Check for calibration buttons on boot
  if (ENTER_CALIBRATE_BUTTONS) {
    flags[SkipProcessButtons] = 1;
    global_mode[CALIBRATE] = 1;
  }

  // First boot: Run a hardware test, calibrate, and do a factory reset
  else if (!valid_fw_version ||
           (system_calibrations->major_firmware_version <
            FORCE_CAL_UNDER_FW_MAJOR_VERSION) ||
           ((system_calibrations->major_firmware_version ==
             FORCE_CAL_UNDER_FW_MAJOR_VERSION) &&
            system_calibrations->minor_firmware_version <
                FORCE_CAL_UNDER_FW_MINOR_VERSION)) {
    LEDDRIVER_OUTPUTENABLE_ON;

    if (do_hardware_test()) {
      global_mode[CALIBRATE] = 1;
      do_factory_reset = 960000; // run normally for about 6 seconds before
                                 // calibrating the CV jacks
    } else {
      while (1)
        ; // halt
    }
  }

  if (TEST_LED_BUTTONS)
    fade_all_buttonLEDs();

  init_buttonLEDs();
  init_ButtonLED_IRQ();
  init_LED_PWM_IRQ();

  // Initialize ADCs
  Deinit_Pot_ADC();
  Deinit_CV_ADC();
  init_LowPassCoefs();
  Init_Pot_ADC((uint16_t *)potadc_buffer, NUM_POT_ADCS);
  Init_CV_ADC((uint16_t *)cvadc_buffer, NUM_CV_ADCS);

  // Initialize Codec
  Codec_GPIO_Init();
  Codec_AudioInterface_Init(BASE_SAMPLE_RATE);
  init_audio_dma();
  Codec_Register_Setup(0);

  // Initialize parameters/modes
  init_adc_param_update_IRQ();

  set_default_user_settings();
  read_user_settings();
  init_params();
  init_modes();

  // If we detect a different version, update the firmware version in FLASH
  if (system_calibrations->major_firmware_version != FW_MAJOR_VERSION ||
      system_calibrations->minor_firmware_version != FW_MINOR_VERSION) {
    LEDDRIVER_OUTPUTENABLE_ON;

    apply_firmware_specific_adjustments();
    copy_system_calibrations_into_staging();
    set_firmware_version();
    write_all_system_calibrations_to_FLASH();
  }

  flags[SystemModeButtonsDown] = 0;

  // Begin reading inputs
  init_buttons();
  init_ButtonDebounce_IRQ();
  init_TrigJackDebounce_IRQ();

  // Begin audio DMA
  audio_buffer_init();

  // request unaltered backup of index @ boot
  flags[BootBak] = 1;

  // Turn on button LEDs for bank loading
  LEDDRIVER_OUTPUTENABLE_ON;

  // Load all banks
  init_banks(); // calls load_all_banks which calls index_write_wrapper()

  // Backup index file (unless we are booting for the first time)
  if (!do_factory_reset)
    backup_sampleindex_file();

  set_codec_callback(process_audio_block_codec);
  Start_I2SDMA();

  delay();

  init_SDIO_read_IRQ();

  // Main loop
  // All routines accessing the SD card should run here
  while (1) {

    check_errors();

    write_buffer_to_storage();

    if (flags[TimeToReadStorage]) {
      flags[TimeToReadStorage] = 0;
      read_storage_to_buffer();
    }

    if (flags[FindNextSampleToAssign]) {
      do_assignment(flags[FindNextSampleToAssign]);
      flags[FindNextSampleToAssign] = 0;
    }

    if (flags32[SaveUserSettingsLater]) {
      flags32[SaveUserSettingsLater]--;

      if (flags32[SaveUserSettingsLater] == 0)
        flags[SaveUserSettings] = 1;
    }

    if (flags[SaveUserSettings]) {
      flags[SaveUserSettings] = 0;
      save_user_settings();
    }

    process_mode_flags();

    if (flags[RewriteIndex]) {
      res = index_write_wrapper();
      if (res) {
        flags[RewriteIndexFail] = 255;
        g_error |= CANNOT_WRITE_INDEX;
      } else
        flags[RewriteIndexSucess] = 255;

      flags[RewriteIndex] = 0;
    }

    if (flags[LoadBackupIndex]) {
      load_sampleindex_file(USE_BACKUP_FILE, flags[LoadBackupIndex] - 1);
      flags[LoadBackupIndex] = 0;
      flags[ForceFileReload1] = 1;
      flags[ForceFileReload2] = 1;
    }

    if (flags[ShutdownAndBootload]) {
      flags[ShutdownAndBootload] = 0;
      deinit_all();

      BOOTLOADER_MAGIC_ADDRESS = BOOTLOADER_MAGIC_CODE;
      NVIC_SystemReset();
    }

    if (do_factory_reset)
      if (!(--do_factory_reset))
        factory_reset();
  }

  return (1);
}

//#define USE_FULL_ASSERT

#ifdef USE_FULL_ASSERT

#define assert_param(expr)                                                     \
  ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

/**
 * @brief  Reports the name of the source file and the source line number
 *   where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* User can add his own implementation to report the file name and line
     number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1) {
  }
}
#endif

#if 1
/* exception handlers - so we know what's failing */
void NMI_Handler(void) {
  NVIC_SystemReset();
  while (1) {
  };
}

void HardFault_Handler(void) {
  NVIC_SystemReset();

  // volatile uint8_t foobar;
  // uint32_t hfsr,dfsr,afsr,bfar,mmfar,cfsr;

  // volatile uint8_t pause=1;

  // foobar=1;
  // mmfar=SCB->MMFAR;
  // bfar=SCB->BFAR;

  // hfsr=SCB->HFSR;
  // afsr=SCB->AFSR;
  // dfsr=SCB->DFSR;
  // cfsr=SCB->CFSR;

  // if (foobar){
  // 	return;
  // } else {
  // 	while(pause){};
  // }
}

void MemManage_Handler(void) {
  NVIC_SystemReset();
  while (1) {
  };
}

void BusFault_Handler(void) {
  NVIC_SystemReset();
  while (1) {
  };
}

void UsageFault_Handler(void) {
  NVIC_SystemReset();
  while (1) {
  };
}

void SVC_Handler(void) {
  NVIC_SystemReset();
  while (1) {
  };
}

void DebugMon_Handler(void) {
  NVIC_SystemReset();
  while (1) {
  };
}

void PendSV_Handler(void) {
  NVIC_SystemReset();
  while (1) {
  };
}
#endif
