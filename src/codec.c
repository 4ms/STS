/*
 * codec.c - CS4271 setup and initialization
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#include "codec.h"
#include "globals.h"
#include "i2s.h"

#define CODEC_IS_SLAVE 0
#define CODEC_IS_MASTER 1

#define MCLK_SRC_STM 0
#define MCLK_SRC_EXTERNAL 1
//#define MCLK_SRC_CODEC 2

#define CODEC_MODE CODEC_IS_SLAVE
#define CODEC_MCLK_SRC MCLK_SRC_STM

/* Codec audio Standards */
#ifdef I2S_STANDARD_PHILLIPS
#define CODEC_STANDARD 0x04
#define I2S_STANDARD I2S_Standard_Phillips
#elif defined(I2S_STANDARD_MSB)
#define CODEC_STANDARD 0x00
#define I2S_STANDARD I2S_Standard_MSB
#elif defined(I2S_STANDARD_LSB)
#define CODEC_STANDARD 0x08
#define I2S_STANDARD I2S_Standard_LSB
#else
#error "Error: No audio communication standard selected !"
#endif /* I2S_STANDARD */

#define CS4271_ADDR_0 0b0010000
#define CS4271_ADDR_1 0b0010001

/*
 * The 7 bits Codec address (sent through I2C interface)
 * The 8th bit (LSB) is Read /Write
 */
#define CODEC_ADDRESS (CS4271_ADDR_0 << 1)

#define CS4271_NUM_REGS                                                                                                \
	6 /* we only initialize the first 6 registers, the 7th is for pre/post-init and the 8th is read-only */

#define CS4271_REG_MODECTRL1 1
#define CS4271_REG_DACCTRL 2
#define CS4271_REG_DACMIX 3
#define CS4271_REG_DACAVOL 4
#define CS4271_REG_DACBVOL 5
#define CS4271_REG_ADCCTRL 6
#define CS4271_REG_MODELCTRL2 7
#define CS4271_REG_CHIPID 8 /*Read-only*/

//Reg 1 (MODECTRL1):
#define SINGLE_SPEED (0b00 << 6) /* 4-50kHz */
#define DOUBLE_SPEED (0b10 << 6) /* 50-100kHz */
#define QUAD_SPEED (0b11 << 6)	 /* 100-200kHz */
#define RATIO0 (0b00 << 4)		 /* See table page 28 and 29 of datasheet */
#define RATIO1 (0b01 << 4)
#define RATIO2 (0b10 << 4)
#define RATIO3 (0b11 << 4)
#define MASTER (1 << 3)
#define SLAVE (0 << 3)
#define DIF_LEFTJUST_24b (0b000)
#define DIF_I2S_24b (0b001)
#define DIF_RIGHTJUST_16b (0b010)
#define DIF_RIGHTJUST_24b (0b011)
#define DIF_RIGHTJUST_20b (0b100)
#define DIF_RIGHTJUST_18b (0b101)

//Reg 2 (DACCTRL)
#define AUTOMUTE (1 << 7)
#define SLOW_FILT_SEL (1 << 6)
#define FAST_FILT_SEL (0 << 6)
#define DEEMPH_OFF (0 << 4)
#define DEEMPH_44 (1 << 4)
#define DEEMPH_48 (2 << 4)
#define DEEMPH_32 (3 << 4)
#define SOFT_RAMPUP                                                                                                    \
	(1                                                                                                                 \
	 << 3) /*An un-mute will be performed after executing a filter mode change, after a MCLK/LRCK ratio change or error, and after changing the Functional Mode.*/
#define SOFT_RAMPDOWN (1 << 2) /*A mute will be performed prior to executing a filter mode change.*/
#define INVERT_SIGA_POL                                                                                                \
	(1 << 1) /*When set, this bit activates an inversion of the signal polarity for the appropriate channel*/
#define INVERT_SIGB_POL (1 << 0)

//Reg 3 (DACMIX)
#define BEQA                                                                                                           \
	(1                                                                                                                 \
	 << 6) /*If set, ignore AOUTB volume setting, and instead make channel B's volume equal channel A's volume as set by AOUTA */
#define SOFTRAMP                                                                                                       \
	(1                                                                                                                 \
	 << 5) /*Allows level changes, both muting and attenuation, to be implemented by incrementally ramping, in 1/8 dB steps, from the current level to the new level at a rate of 1 dB per 8 left/right clock periods */
#define ZEROCROSS                                                                                                      \
	(1                                                                                                                 \
	 << 4) /*Dictates that signal level changes, either by attenuation changes or muting, will occur on a signal zero crossing to minimize audible artifacts*/
#define ATAPI_aLbR (0b1001) /*channel A==>Left, channel B==>Right*/

//Reg 4: DACAVOL
//Reg 5: DACBVOL
//decimal value 0 to 90 represents +0dB to -90dB output volume

//Reg 6 (ADCCTRL)
#define DITHER16 (1 << 5)	   /*activates the Dither for 16-Bit Data feature*/
#define ADC_DIF_I2S (1 << 4)   /*I2S, up to 24-bit data*/
#define ADC_DIF_LJUST (0 << 4) /*Left Justified, up to 24-bit data (default)*/
#define MUTEA (1 << 3)
#define MUTEB (1 << 2)
#define HPFDisableA (1 << 1)
#define HPFDisableB (1 << 0)

//Reg 7 (MODECTRL2)
#define PDN (1 << 0)	 /* Power Down Enable */
#define CPEN (1 << 1)	 /* Control Port Enable */
#define FREEZE (1 << 2)	 /* Freezes effects of register changes */
#define MUTECAB (1 << 3) /* Internal AND gate on AMUTEC and BMUTEC */
#define LOOP (1 << 4)	 /* Digital loopback (ADC->DAC) */

//Reg 8 (CHIPID) (Read-only)
#define PART_mask (0b11110000)
#define REV_mask (0b00001111)

uint8_t codec_init_data_base[] = {RATIO0 | DIF_I2S_24b, //MODECTRL1

								  SLOW_FILT_SEL | DEEMPH_OFF, //DACCTRL

								  ATAPI_aLbR, //DACMIX

								  0b00000000, //DACAVOL
								  0b00000000, //DACBVOL

								  ADC_DIF_I2S};

// Private functions:
uint32_t codec_write_one_register(uint8_t RegisterAddr, uint8_t RegisterValue, I2C_TypeDef *CODEC);
uint32_t
codec_write_all_registers(I2C_TypeDef *CODEC, uint8_t master_slave, uint8_t enable_DCinput, uint32_t sample_rate);
uint32_t codec_timeout_callback(void);
void set_i2s_samplerate(uint32_t sample_rate);

__IO uint32_t CODECTimeout = CODEC_LONG_TIMEOUT;

uint32_t codec_timeout_callback(void) {
	return 1; //debug breakpoint or error handling here
}

//   	//...and bring it all back up
// init_SAI_clock(sample_rate);

// codec_GPIO_init();
// codec_SAI_init(sample_rate);
// init_audio_DMA();

// codec_I2C_init();
// codec_register_setup(0, sample_rate);

// start_audio();

void codec_reboot_new_samplerate(uint32_t sample_rate) {
	static uint32_t last_sample_rate;

	if (sample_rate != 44100 && sample_rate != 48000 && sample_rate != 88200 && sample_rate != 96000)
		sample_rate = 44100;

	//Do nothing if the sample_rate did not change
	if (last_sample_rate != sample_rate) {
		last_sample_rate = sample_rate;

		codec_powerdown(); //issue I2C command to power codec down
		//RCC_APB1PeriphClockCmd(CODEC_I2C_CLK, DISABLE);	//Disable I2C clock
		codec_resetpin_low(); //Pull codec RESET pin low
		delay();

		stop_audio_clock_source(); //Disable PLLI2S
		deinit_audio_dma();		   //Disable DMA stream, interrupts
		delay();

		codec_init_gpio();
		codec_init_i2s(sample_rate);
		init_audio_dma();
		codec_setup_registers(0, sample_rate);

		start_audio_stream();
	}
}

void codec_resetpin_low(void) {
	GPIO_InitTypeDef gpio;

	//Enable GPIO for RESET pin
	RCC_AHB1PeriphClockCmd(CODEC_RESET_RCC, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

	gpio.GPIO_Pin = CODEC_RESET_pin;
	GPIO_Init(CODEC_RESET_GPIO, &gpio);

	//Pull the RESET pin low
	CODEC_RESET_LOW;
}

void codec_powerdown(void) {
	codec_write_one_register(CS4271_REG_MODELCTRL2, PDN, CODEC_I2C); //Control Port Enable and Power Down Enable
}

uint32_t codec_setup_registers(uint8_t enable_DCinput, uint32_t sample_rate) {
	uint32_t err = 0;

	codec_init_i2c();

	CODEC_RESET_HIGH;
	delay_ms(2);

	err += codec_write_all_registers(CODEC_I2C, CODEC_MODE, enable_DCinput, sample_rate);

	return err;
}

uint32_t
codec_write_all_registers(I2C_TypeDef *CODEC, uint8_t master_slave, uint8_t enable_DCinput, uint32_t sample_rate) {
	uint8_t i;
	uint32_t err = 0;

	err +=
		codec_write_one_register(CS4271_REG_MODELCTRL2, CPEN | PDN, CODEC); //Control Port Enable and Power Down Enable

	if (master_slave == CODEC_IS_MASTER)
		codec_init_data_base[0] |= MASTER;
	else
		codec_init_data_base[0] |= SLAVE;

	if (sample_rate <= 50000)
		codec_init_data_base[0] |= SINGLE_SPEED;
	else if (sample_rate <= 100000)
		codec_init_data_base[0] |= DOUBLE_SPEED;
	else
		codec_init_data_base[0] |= QUAD_SPEED;

	if (enable_DCinput)
		codec_init_data_base[4] |= HPFDisableA | HPFDisableB;

	for (i = 0; i < CS4271_NUM_REGS; i++)
		err += codec_write_one_register(i + 1, codec_init_data_base[i], CODEC);

	err += codec_write_one_register(CS4271_REG_MODELCTRL2, CPEN, CODEC); //Power Down disable

	return err;
}

uint32_t codec_write_one_register(uint8_t RegisterAddr, uint8_t RegisterValue, I2C_TypeDef *CODEC) {
	uint32_t result = 0;

	uint8_t Byte1 = RegisterAddr;
	uint8_t Byte2 = RegisterValue;

	/*!< While the bus is busy */
	CODECTimeout = CODEC_LONG_TIMEOUT;
	while (I2C_GetFlagStatus(CODEC, I2C_FLAG_BUSY)) {
		if ((CODECTimeout--) == 0)
			return codec_timeout_callback();
	}

	/* Start the config sequence */
	I2C_GenerateSTART(CODEC, ENABLE);

	/* Test on EV5 and clear it */
	CODECTimeout = CODEC_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(CODEC, I2C_EVENT_MASTER_MODE_SELECT)) {
		if ((CODECTimeout--) == 0)
			return codec_timeout_callback();
	}

	/* Transmit the slave address and enable writing operation */
	I2C_Send7bitAddress(CODEC, CODEC_ADDRESS, I2C_Direction_Transmitter);

	/* Test on EV6 and clear it */
	CODECTimeout = CODEC_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(CODEC, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
		if ((CODECTimeout--) == 0)
			return codec_timeout_callback();
	}

	/* Transmit the first address for write operation */
	I2C_SendData(CODEC, Byte1);

	/* Test on EV8 and clear it */
	CODECTimeout = CODEC_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(CODEC, I2C_EVENT_MASTER_BYTE_TRANSMITTING)) {
		if ((CODECTimeout--) == 0)
			return codec_timeout_callback();
	}

	/* Prepare the register value to be sent */
	I2C_SendData(CODEC, Byte2);

	/*!< Wait till all data have been physically transferred on the bus */
	CODECTimeout = CODEC_LONG_TIMEOUT;
	while (!I2C_GetFlagStatus(CODEC, I2C_FLAG_BTF)) {
		if ((CODECTimeout--) == 0)
			return codec_timeout_callback();
	}

	/* End the configuration sequence */
	I2C_GenerateSTOP(CODEC, ENABLE);

	/* Return the verifying value: 0 (Passed) or 1 (Failed) */
	return result;
}

/*
 * Initializes I2C peripheral for both codecs
 */
void codec_init_i2c(void) {
	I2C_InitTypeDef I2C_InitStructure;

	RCC_APB1PeriphClockCmd(CODEC_I2C_CLK, ENABLE);

	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x33;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;

	I2C_DeInit(CODEC_I2C);
	I2C_Init(CODEC_I2C, &I2C_InitStructure);
	I2C_Cmd(CODEC_I2C, ENABLE);
}

void set_i2s_samplerate(uint32_t sample_rate) {
	uint32_t app_PLLI2S_N = 271; //default value
	uint32_t app_PLLI2S_R = 2;	 //default value
	uint32_t app_I2SODD = 0;	 //default value
	uint32_t app_I2SDIV = 6;	 //default value

	uint32_t app_PLLI2S_Q = 4; //not used for I2S, but must be set

	// PLLI2S_VCO = (HSE_VALUE Or HSI_VALUE / PLL_M) * PLLI2S_N   = 1MHz * PLLI2S_N
	// I2SCLK = PLLI2S_VCO / PLLI2S_R
	// FS = I2SCLK / [(16*2)*((2*I2SDIV)+ODD)*8)] when the channel frame is 16-bit wide
	// FS = I2SCLK / [(32*2)*((2*I2SDIV)+ODD)*4)] when the channel frame is 32-bit wide

	if (sample_rate == 44100) {
		app_PLLI2S_N = 271;
		app_PLLI2S_R = 2;
		app_I2SDIV = 6;
		app_I2SODD = 0;
	} else if (sample_rate == 48000) {
		app_PLLI2S_N = 258;
		app_PLLI2S_R = 3;
		app_I2SDIV = 3;
		app_I2SODD = 1;
	} else if (sample_rate == 88200) {
		app_PLLI2S_N = 271;
		app_PLLI2S_R = 2;
		app_I2SDIV = 3;
		app_I2SODD = 0;
	} else if (sample_rate == 96000) {
		app_PLLI2S_N = 344;
		app_PLLI2S_R = 2;
		app_I2SDIV = 3;
		app_I2SODD = 1;
	}

	RCC_PLLI2SConfig(app_PLLI2S_N, app_PLLI2S_Q, app_PLLI2S_R);

	if (app_I2SODD)
		CODEC_I2S->I2SPR |= SPI_I2SPR_ODD;
	else
		CODEC_I2S->I2SPR &= ~SPI_I2SPR_ODD;

	CODEC_I2S->I2SPR &= ~SPI_I2SPR_I2SDIV;
	CODEC_I2S->I2SPR |= app_I2SDIV;
}

/*
 * Initializes I2S peripheral
 */
void codec_init_i2s(uint32_t sample_rate) {
	I2S_InitTypeDef I2S_InitStructure;

	// stop_audio_clock_source();
	// set_i2s_samplerate(sample_rate);

	start_audio_clock_source();

	// Enable the CODEC_I2S peripheral clock
	RCC_APB1PeriphClockCmd(CODEC_I2S_CLK, ENABLE);

	// CODEC_I2S peripheral configuration for master TX
	SPI_I2S_DeInit(CODEC_I2S);
	I2S_InitStructure.I2S_AudioFreq = sample_rate;
	I2S_InitStructure.I2S_Standard = I2S_STANDARD;
	I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_24b;
	I2S_InitStructure.I2S_CPOL = I2S_CPOL_High;

	if (CODEC_MODE == CODEC_IS_MASTER)
		I2S_InitStructure.I2S_Mode = I2S_Mode_SlaveTx;
	else
		I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;

	if (CODEC_MODE == CODEC_IS_MASTER)
		I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
	else {
		if (CODEC_MCLK_SRC == MCLK_SRC_STM)
			I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Enable;
		else
			I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
	}

	// Initialize the I2S main channel for TX
	I2S_Init(CODEC_I2S, &I2S_InitStructure);

	// Initialize the I2S extended channel for RX
	I2S_FullDuplexConfig(CODEC_I2S_EXT, &I2S_InitStructure);

	set_i2s_samplerate(sample_rate);

	I2S_Cmd(CODEC_I2S, ENABLE);
	I2S_Cmd(CODEC_I2S_EXT, ENABLE);
}

void codec_init_gpio(void) {
	GPIO_InitTypeDef gpio;

	/* Enable I2S and I2C GPIO clocks */
	RCC_AHB1PeriphClockCmd(CODEC_I2C_GPIO_CLOCK | CODEC_I2S_GPIO_CLOCK, ENABLE);

	/* CODEC_I2C SCL and SDA pins configuration -------------------------------------*/
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_OType = GPIO_OType_OD;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

	gpio.GPIO_Pin = CODEC_I2C_SCL_PIN | CODEC_I2C_SDA_PIN;
	GPIO_Init(CODEC_I2C_GPIO, &gpio);

	/* Connect pins to I2C peripheral */
	GPIO_PinAFConfig(CODEC_I2C_GPIO, CODEC_I2C_SCL_PINSRC, CODEC_I2C_GPIO_AF);
	GPIO_PinAFConfig(CODEC_I2C_GPIO, CODEC_I2C_SDA_PINSRC, CODEC_I2C_GPIO_AF);

	/* CODEC_I2S output pins configuration: WS, SCK SD0 SDI MCK pins ------------------*/
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_Speed = GPIO_Speed_100MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

	gpio.GPIO_Pin = CODEC_I2S_WS_PIN;
	GPIO_Init(CODEC_I2S_GPIO_WS, &gpio);
	gpio.GPIO_Pin = CODEC_I2S_SDI_PIN;
	GPIO_Init(CODEC_I2S_GPIO_SDI, &gpio);
	gpio.GPIO_Pin = CODEC_I2S_SCK_PIN;
	GPIO_Init(CODEC_I2S_GPIO_SCK, &gpio);
	gpio.GPIO_Pin = CODEC_I2S_SDO_PIN;
	GPIO_Init(CODEC_I2S_GPIO_SDO, &gpio);

	GPIO_PinAFConfig(CODEC_I2S_GPIO_WS, CODEC_I2S_WS_PINSRC, CODEC_I2S_GPIO_AF);
	GPIO_PinAFConfig(CODEC_I2S_GPIO_SCK, CODEC_I2S_SCK_PINSRC, CODEC_I2S_GPIO_AF);
	GPIO_PinAFConfig(CODEC_I2S_GPIO_SDO, CODEC_I2S_SDO_PINSRC, CODEC_I2S_GPIO_AF);
	GPIO_PinAFConfig(CODEC_I2S_GPIO_SDI, CODEC_I2S_SDI_PINSRC, CODEC_I2Sext_GPIO_AF);

	if (CODEC_MCLK_SRC == MCLK_SRC_STM) {

		gpio.GPIO_Mode = GPIO_Mode_AF;
		gpio.GPIO_Speed = GPIO_Speed_100MHz;
		gpio.GPIO_OType = GPIO_OType_PP;
		gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

		gpio.GPIO_Pin = CODEC_I2S_MCK_PIN;
		GPIO_Init(CODEC_I2S_MCK_GPIO, &gpio);
		GPIO_PinAFConfig(CODEC_I2S_MCK_GPIO, CODEC_I2S_MCK_PINSRC, CODEC_I2S_GPIO_AF);

	} else if (CODEC_MCLK_SRC == MCLK_SRC_EXTERNAL) {

		gpio.GPIO_Mode = GPIO_Mode_IN;
		gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

		gpio.GPIO_Pin = CODEC_I2S_MCK_PIN;
		GPIO_Init(CODEC_I2S_MCK_GPIO, &gpio);
	}
}
