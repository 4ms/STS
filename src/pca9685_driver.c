/*
 * pca9685_driver.c - Driver for PCA9685 16-channel LED driver chips over I2C bus
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

#include <stm32f4xx.h>
#include "dig_pins.h"
#include "pca9685_driver.h"
#include "globals.h"


#define delay_cycles(x)						\
do {							\
  register unsigned int i;				\
  for (i = 0; i < x; ++i)				\
    __asm__ __volatile__ ("nop\n\t":::"memory");	\
} while (0)

//These arrays map the rgbled_number (which is a unique number assigned to each RGB LED component)
//to the driver chip's led number of the first element (red) of the RGB LED.
//It's assumed blue is driverRGBBaseElement[x]+1, and green is driverRGBBaseElement[i]+2.
const uint8_t driverRGBBaseAddress[8] =	{0, 0, 0, 1, 1, 1, 1, 1};
const uint8_t driverRGBBaseElement[8] =	{0, 3, 6, 0, 3, 6, 9, 12};

//If we are using discrete LEDs or if one RGB LED spans two driver chips, then we need use these arrays instead:
const uint8_t driverAddress[24] =	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1,  1,  1,  1 };
const uint8_t driverElement[24] =	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };


__IO uint32_t  LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;

void LEDDriver_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Enable I2S and I2C GPIO clocks */
	RCC_AHB1PeriphClockCmd(LEDDRIVER_I2C_GPIO_CLOCK, ENABLE);

	/* LEDDRIVER_I2C SCL and SDA pins configuration -------------------------------------*/
	GPIO_InitStructure.GPIO_Pin = LEDDRIVER_I2C_SCL_PIN | LEDDRIVER_I2C_SDA_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(LEDDRIVER_I2C_GPIO, &GPIO_InitStructure);

	/* Connect pins to I2C peripheral */
	GPIO_PinAFConfig(LEDDRIVER_I2C_GPIO, LEDDRIVER_I2S_SCL_PINSRC, LEDDRIVER_I2C_GPIO_AF);
	GPIO_PinAFConfig(LEDDRIVER_I2C_GPIO, LEDDRIVER_I2S_SDA_PINSRC, LEDDRIVER_I2C_GPIO_AF);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = LEDDRIVER_OUTPUTENABLE_pin;	GPIO_Init(LEDDRIVER_OUTPUTENABLE_GPIO, &GPIO_InitStructure);

}

void LEDDriver_I2C_Init(void)
{
	I2C_InitTypeDef I2C_InitStructure;

	/* Enable the LEDDRIVER_I2C peripheral clock */
	RCC_APB1PeriphClockCmd(LEDDRIVER_I2C_CLK, ENABLE);

	/* LEDDRIVER_I2C peripheral configuration */
	I2C_DeInit(LEDDRIVER_I2C);
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x34;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = I2C1_SPEED;

	/* Enable the I2C peripheral */
	I2C_Cmd(LEDDRIVER_I2C, ENABLE);
	I2C_Init(LEDDRIVER_I2C, &I2C_InitStructure);
}


uint32_t LEDDRIVER_TIMEOUT_UserCallback(void)
{
	/* nothing */
	return 1;
}

uint32_t LEDDriver_writeregister(uint8_t driverAddr, uint8_t RegisterAddr, uint8_t RegisterValue){
	uint32_t result = 0;

	driverAddr = PCA9685_I2C_BASE_ADDRESS | (driverAddr << 1);
	//driverAddr = 0b10000000;

	/*!< While the bus is busy */
	LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;
	while(I2C_GetFlagStatus(LEDDRIVER_I2C, I2C_FLAG_BUSY))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Start the config sequence */
	I2C_GenerateSTART(LEDDRIVER_I2C, ENABLE);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Transmit the slave address */
	I2C_Send7bitAddress(LEDDRIVER_I2C, driverAddr, I2C_Direction_Transmitter);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Transmit the address for write operation */
	I2C_SendData(LEDDRIVER_I2C, RegisterAddr);

	/* Test on EV8 and clear it */
	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Prepare the register value to be sent */
	I2C_SendData(LEDDRIVER_I2C, RegisterValue);

	/*!< Wait till all data have been physically transferred on the bus */
	LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;
	while(!I2C_GetFlagStatus(LEDDRIVER_I2C, I2C_FLAG_BTF))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* End the configuration sequence */
	I2C_GenerateSTOP(LEDDRIVER_I2C, ENABLE);

	/* Return the verifying value: 0 (Passed) or 1 (Failed) */
	return result;
}


inline uint32_t LEDDriver_startxfer(uint8_t driverAddr){
	uint32_t result=0;

	driverAddr = PCA9685_I2C_BASE_ADDRESS | (driverAddr << 1);

	/*!< While the bus is busy */
	LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;
	while(I2C_GetFlagStatus(LEDDRIVER_I2C, I2C_FLAG_BUSY))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Start the config sequence */
	I2C_GenerateSTART(LEDDRIVER_I2C, ENABLE);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Transmit the slave address */
	I2C_Send7bitAddress(LEDDRIVER_I2C, driverAddr, I2C_Direction_Transmitter);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	return result;
}

inline uint32_t LEDDriver_senddata(uint8_t data){
	uint32_t result=0;

	/* Transmit the data for write operation */
	I2C_SendData(LEDDRIVER_I2C, data);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}
	return result;

}

inline void LEDDriver_endxfer(void){
	/* End the configuration sequence */
	I2C_GenerateSTOP(LEDDRIVER_I2C, ENABLE);

}



/*
 * Sets one LED element's 10-bit brightness.
 */
void LEDDriver_set_one_LED(uint8_t led_number, uint16_t brightness) //sets one LED element
{
	uint8_t driverAddr;

	//element_number is 0..(NUM_LEDS*3-1)

	driverAddr = driverAddress[led_number];
	led_number = driverElement[led_number];

	LEDDriver_startxfer(driverAddr); //20us
	LEDDriver_senddata(PCA9685_LED0 + (led_number*4)); //4 registers per LED element
	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(brightness & 0xFF); //off-time = brightness
	LEDDriver_senddata(brightness >> 8);
	LEDDriver_endxfer();

}

/*
 * Sets one RGB LED with a 10+10+10 bit color value
 */
void LEDDriver_setRGBLED(uint8_t rgbled_number, uint32_t rgb){
	uint8_t driverAddr;
	uint8_t led_number;

	uint16_t c_red= (rgb >> 20) & 0b1111111111;
	uint16_t c_green= (rgb >> 10) & 0b1111111111;
	uint16_t c_blue= rgb & 0b1111111111;

	driverAddr = driverRGBBaseAddress[rgbled_number];
	led_number = driverRGBBaseElement[rgbled_number];

	LEDDriver_startxfer(driverAddr);

	LEDDriver_senddata(PCA9685_LED0 + (led_number*4)); //4 registers per LED element
	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_red & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_red >> 8);

	LEDDriver_senddata(0); //on-time = 0 //four senddata commands take 140us
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_green & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_green >> 8);

	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_blue & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_blue >> 8);

	LEDDriver_endxfer();

}

/*
 * Sets one RGB LED with a 10+10+10 bit color value, shifted to 12+12+12bit values
 */
void LEDDriver_setRGBLED_12bit(uint8_t rgbled_number, uint32_t rgb){
	uint8_t driverAddr;
	uint8_t led_number;

	uint16_t c_red= (rgb >> 20) & 0b1111111111;
	uint16_t c_green= (rgb >> 10) & 0b1111111111;
	uint16_t c_blue= rgb & 0b1111111111;

	c_red *= 4;
	c_green *= 4;
	c_blue *= 4;

	driverAddr = driverRGBBaseAddress[rgbled_number];
	led_number = driverRGBBaseElement[rgbled_number];

	LEDDriver_startxfer(driverAddr);

	LEDDriver_senddata(PCA9685_LED0 + (led_number*4)); //4 registers per LED element
	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_red & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_red >> 8);

	LEDDriver_senddata(0); //on-time = 0 //four senddata commands take 140us
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_green & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_green >> 8);

	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_blue & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_blue >> 8);

	LEDDriver_endxfer();

}

void LEDDriver_setRGBLED_RGB(uint8_t rgbled_number, int16_t c_red, int16_t c_green, int16_t c_blue)
{
	uint8_t driverAddr;
	uint8_t led_number;


	driverAddr = driverRGBBaseAddress[rgbled_number];
	led_number = driverRGBBaseElement[rgbled_number];

	LEDDriver_startxfer(driverAddr);

	LEDDriver_senddata(PCA9685_LED0 + (led_number*4)); //4 registers per LED element
	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_red & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_red >> 8);

	LEDDriver_senddata(0); //on-time = 0 //four senddata commands take 140us
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_green & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_green >> 8);

	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(c_blue & 0xFF); //off-time = brightness
	LEDDriver_senddata(c_blue >> 8);

	LEDDriver_endxfer();


}

void LEDDriver_Reset(uint8_t driverAddr){


	LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b00000000); // clear sleep mode

	delay_cycles(20);

	//LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b10100000);	// set up for auto increment

	LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b10000000); //start reset mode

	delay_cycles(20);

	LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b00100000);	//auto increment

	LEDDriver_writeregister(driverAddr, PCA9685_MODE2, 0b00010001);	// INVERT=1, OUTDRV=0, OUTNE=01



}

void LEDDriver_Init(uint8_t numdrivers){

	uint8_t i;

	LEDDriver_GPIO_Init();

	LEDDRIVER_OUTPUTENABLE_OFF;

	LEDDriver_I2C_Init();

	for (i=0;i<numdrivers;i++){
		LEDDriver_Reset(i);
	}

}





