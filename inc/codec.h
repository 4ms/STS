/*
 * codec.h
 *
 */

#pragma once

#include <stm32f4xx.h>

/* I2C clock speed configuration (in Hz)  */
#define I2C_SPEED                       50000

// Uncomment defines below to select standard for audio communication between Codec and I2S peripheral
#define I2S_STANDARD_PHILLIPS
//#define I2S_STANDARD_MSB
//#define I2S_STANDARD_LSB

//#define USE_DEFAULT_TIMEOUT_CALLBACK


// For CS4271
#define CODEC_RESET_RCC RCC_AHB1Periph_GPIOB
#define CODEC_RESET_pin GPIO_Pin_4
#define CODEC_RESET_GPIO GPIOB
#define CODEC_RESET_HIGH CODEC_RESET_GPIO->BSRRL = CODEC_RESET_pin
#define CODEC_RESET_LOW CODEC_RESET_GPIO->BSRRH = CODEC_RESET_pin

/*-----------------------------------
Hardware Configuration defines parameters
-----------------------------------------*/                 
/* I2S peripheral configuration defines */

#define CODEC_I2S                      SPI2
#define CODEC_I2S_EXT                  I2S2ext
#define CODEC_I2S_CLK                  RCC_APB1Periph_SPI2
#define CODEC_I2S_ADDRESS              0x4000380C
#define CODEC_I2S_EXT_ADDRESS          0x4000340C
#define CODEC_I2S_GPIO_AF              GPIO_AF_SPI2
#define CODEC_I2Sext_GPIO_AF			GPIO_AF_SPI2
#define CODEC_I2S_IRQ                  SPI2_IRQn
#define CODEC_I2S_EXT_IRQ              SPI2_IRQn
#define CODEC_I2S_GPIO_CLOCK           (RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOB)


#define CODEC_I2S_MCK_GPIO             GPIOC
#define CODEC_I2S_MCK_PIN              GPIO_Pin_6
#define CODEC_I2S_MCK_PINSRC           GPIO_PinSource6

#define CODEC_I2S_GPIO_WS              GPIOB
#define CODEC_I2S_WS_PIN               GPIO_Pin_12
#define CODEC_I2S_WS_PINSRC            GPIO_PinSource12

#define CODEC_I2S_GPIO_SDO             GPIOB
#define CODEC_I2S_SDO_PIN              GPIO_Pin_15
#define CODEC_I2S_SDO_PINSRC           GPIO_PinSource15

#define CODEC_I2S_GPIO_SCK             GPIOB
#define CODEC_I2S_SCK_PIN              GPIO_Pin_13
#define CODEC_I2S_SCK_PINSRC           GPIO_PinSource13

#define CODEC_I2S_GPIO_SDI             GPIOB
#define CODEC_I2S_SDI_PIN              GPIO_Pin_14
#define CODEC_I2S_SDI_PINSRC           GPIO_PinSource14

#define AUDIO_I2S2_IRQHandler           SPI2_IRQHandler
#define AUDIO_I2S2_EXT_IRQHandler       SPI2_IRQHandler

//SPI2_TX: S4 C0
#define AUDIO_I2S2_DMA_CLOCK            RCC_AHB1Periph_DMA1
#define AUDIO_I2S2_DMA_STREAM           DMA1_Stream4
#define AUDIO_I2S2_DMA_DREG             CODEC_I2S_ADDRESS
#define AUDIO_I2S2_DMA_CHANNEL          DMA_Channel_0
#define AUDIO_I2S2_DMA_IRQ              DMA1_Stream4_IRQn
#define AUDIO_I2S2_DMA_FLAG_TC          DMA_FLAG_TCIF0
#define AUDIO_I2S2_DMA_FLAG_HT          DMA_FLAG_HTIF0
#define AUDIO_I2S2_DMA_FLAG_FE          DMA_FLAG_FEIF0
#define AUDIO_I2S2_DMA_FLAG_TE          DMA_FLAG_TEIF0
#define AUDIO_I2S2_DMA_FLAG_DME         DMA_FLAG_DMEIF0

//I2S2_EXT_RX: S3 C3
#define AUDIO_I2S2_EXT_DMA_STREAM       DMA1_Stream3
#define AUDIO_I2S2_EXT_DMA_DREG         CODEC_I2S_EXT_ADDRESS
#define AUDIO_I2S2_EXT_DMA_CHANNEL      DMA_Channel_3
#define AUDIO_I2S2_EXT_DMA_IRQ          DMA1_Stream3_IRQn
#define AUDIO_I2S2_EXT_DMA_IRQHandler	DMA1_Stream3_IRQHandler
#define AUDIO_I2S2_EXT_DMA_FLAG_TC      DMA_FLAG_TCIF3
#define AUDIO_I2S2_EXT_DMA_FLAG_HT      DMA_FLAG_HTIF3
#define AUDIO_I2S2_EXT_DMA_FLAG_FE      DMA_FLAG_FEIF3
#define AUDIO_I2S2_EXT_DMA_FLAG_TE      DMA_FLAG_TEIF3
#define AUDIO_I2S2_EXT_DMA_FLAG_DME     DMA_FLAG_DMEIF3


#define AUDIO_MAL_DMA_PERIPH_DATA_SIZE DMA_PeripheralDataSize_HalfWord
#define AUDIO_MAL_DMA_MEM_DATA_SIZE    DMA_MemoryDataSize_HalfWord
#define DMA_MAX_SZE                    0xFFFF

/* I2S DMA Stream definitions */

/* I2C peripheral configuration defines (control interface of the audio codec) */
#define CODEC_I2C                      I2C2
#define CODEC_I2C_CLK                  RCC_APB1Periph_I2C2
#define CODEC_I2C_GPIO_CLOCK           RCC_AHB1Periph_GPIOB
#define CODEC_I2C_GPIO_AF              GPIO_AF_I2C2
#define CODEC_I2C_GPIO                 GPIOB
#define CODEC_I2C_SCL_PIN              GPIO_Pin_10
#define CODEC_I2C_SDA_PIN              GPIO_Pin_11
#define CODEC_I2C_SCL_PINSRC           GPIO_PinSource10
#define CODEC_I2C_SDA_PINSRC           GPIO_PinSource11


/* Maximum Timeout values for flags and events waiting loops. These timeouts are
   not based on accurate values, they just guarantee that the application will 
   not remain stuck if the I2C communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */   
#define CODEC_FLAG_TIMEOUT             ((uint32_t)0x1000)
#define CODEC_LONG_TIMEOUT             ((uint32_t)(300 * CODEC_FLAG_TIMEOUT))


uint32_t Codec_Register_Setup(uint8_t enable_DCinput);

void Codec_CtrlInterface_Init(void);

void Codec_AudioInterface_Init(uint32_t AudioFreq);

uint32_t Codec_Reset(I2C_TypeDef *CODEC, uint8_t master_slave, uint8_t enable_DCinput);

uint32_t Codec_WriteRegister(uint8_t RegisterAddr, uint8_t RegisterValue, I2C_TypeDef *CODEC);

void Codec_Deinit(void);

void Codec_GPIO_Init(void);

void init_i2s_clkin(void);
