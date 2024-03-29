/*
 * dig_pins.h - Digital (GPIO) pin setup
 *
 * Author: Dan Green (danngreen1@gmail.com)
 * Original idea for algoritm from https://www.kvraudio.com/forum/viewtopic.php?t=195315
 * Optimized and translated for int32_t by Dan Green
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

#pragma once

#include <stm32f4xx.h>

#define ALL_GPIO_RCC                                                                                                   \
	(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD |                       \
	 RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOG)
//INPUTS

//Buttons
#define PLAY1BUT_pin GPIO_Pin_14
#define PLAY1BUT_GPIO GPIOG
#define PLAY1BUT (!(PLAY1BUT_GPIO->IDR & PLAY1BUT_pin))

#define PLAY2BUT_pin GPIO_Pin_11
#define PLAY2BUT_GPIO GPIOG
#define PLAY2BUT (!(PLAY2BUT_GPIO->IDR & PLAY2BUT_pin))

#define RECBUT_pin GPIO_Pin_10
#define RECBUT_GPIO GPIOG
#define RECBUT (!(RECBUT_GPIO->IDR & RECBUT_pin))

#define BANK1BUT_pin GPIO_Pin_13
#define BANK1BUT_GPIO GPIOG
#define BANK1BUT (!(BANK1BUT_GPIO->IDR & BANK1BUT_pin))

#define BANK2BUT_pin GPIO_Pin_12
#define BANK2BUT_GPIO GPIOG
#define BANK2BUT (!(BANK2BUT_GPIO->IDR & BANK2BUT_pin))

#define BANKRECBUT_pin GPIO_Pin_9
#define BANKRECBUT_GPIO GPIOG
#define BANKRECBUT (!(BANKRECBUT_GPIO->IDR & BANKRECBUT_pin))

#define REV1BUT_pin GPIO_Pin_1
#define REV1BUT_GPIO GPIOC
#define REV1BUT (!(REV1BUT_GPIO->IDR & REV1BUT_pin))

#define REV2BUT_pin GPIO_Pin_15
#define REV2BUT_GPIO GPIOA
#define REV2BUT (!(REV2BUT_GPIO->IDR & REV2BUT_pin))

#define EDIT_BUTTON_pin GPIO_Pin_3
#define EDIT_BUTTON_GPIO GPIOD
#define EDIT_BUTTON (!(EDIT_BUTTON_GPIO->IDR & EDIT_BUTTON_pin))

#define EDIT_BUTTONREF_pin GPIO_Pin_4
#define EDIT_BUTTONREF_GPIO GPIOD
#define EDIT_BUTTONREF_OFF EDIT_BUTTONREF_GPIO->BSRRH = EDIT_BUTTONREF_pin

//Trigger input jacks

#define PLAY1JACK_pin GPIO_Pin_13
#define PLAY1JACK_GPIO GPIOD
#define PLAY1JACK (PLAY1JACK_GPIO->IDR & PLAY1JACK_pin)

#define PLAY2JACK_pin GPIO_Pin_12
#define PLAY2JACK_GPIO GPIOD
#define PLAY2JACK (PLAY2JACK_GPIO->IDR & PLAY2JACK_pin)

#define RECTRIGJACK_pin GPIO_Pin_11
#define RECTRIGJACK_GPIO GPIOD
#define RECTRIGJACK (RECTRIGJACK_GPIO->IDR & RECTRIGJACK_pin)

#define REV1JACK_pin GPIO_Pin_6
#define REV1JACK_GPIO GPIOG
#define REV1JACK (REV1JACK_GPIO->IDR & REV1JACK_pin)

#define REV2JACK_pin GPIO_Pin_3
#define REV2JACK_GPIO GPIOG
#define REV2JACK (REV2JACK_GPIO->IDR & REV2JACK_pin)

//Trigger Out jacks

#define ENDOUT1_pin GPIO_Pin_3
#define ENDOUT1_GPIO GPIOA
#define ENDOUT1_ON ENDOUT1_GPIO->BSRRL = ENDOUT1_pin
#define ENDOUT1_OFF ENDOUT1_GPIO->BSRRH = ENDOUT1_pin

#define ENDOUT2_pin GPIO_Pin_4
#define ENDOUT2_GPIO GPIOA
#define ENDOUT2_ON ENDOUT2_GPIO->BSRRL = ENDOUT2_pin
#define ENDOUT2_OFF ENDOUT2_GPIO->BSRRH = ENDOUT2_pin

//LEDs (direct control)
#define PLAYLED1_pin GPIO_Pin_11
#define PLAYLED1_GPIO GPIOA
#define PLAYLED1_ON PLAYLED1_GPIO->BSRRL = PLAYLED1_pin
#define PLAYLED1_OFF PLAYLED1_GPIO->BSRRH = PLAYLED1_pin

#define PLAYLED2_pin GPIO_Pin_12
#define PLAYLED2_GPIO GPIOA
#define PLAYLED2_ON PLAYLED2_GPIO->BSRRL = PLAYLED2_pin
#define PLAYLED2_OFF PLAYLED2_GPIO->BSRRH = PLAYLED2_pin

#define SIGNALLED_pin GPIO_Pin_9
#define SIGNALLED_GPIO GPIOA
#define SIGNALLED_ON SIGNALLED_GPIO->BSRRL = SIGNALLED_pin
#define SIGNALLED_OFF SIGNALLED_GPIO->BSRRH = SIGNALLED_pin

#define BUSYLED_pin GPIO_Pin_10
#define BUSYLED_GPIO GPIOA
#define BUSYLED_ON BUSYLED_GPIO->BSRRL = BUSYLED_pin
#define BUSYLED_OFF BUSYLED_GPIO->BSRRH = BUSYLED_pin

//Same pin as WS clock of I2S2 (LRCLK), as defined in codec.h
#define EXTI_CLOCK_GPIO EXTI_PortSourceGPIOB
#define EXTI_CLOCK_pin EXTI_PinSource12
#define EXTI_CLOCK_line EXTI_Line12
#define EXTI_CLOCK_IRQ EXTI15_10_IRQn
#define EXTI_Handler EXTI15_10_IRQHandler

#define PCBVERSION_A_pin GPIO_Pin_13
#define PCBVERSION_A_GPIO GPIOC
#define PCBVERSION_A (PCBVERSION_A_GPIO->IDR & PCBVERSION_A_pin)

#define DEBUG0 GPIO_Pin_5
#define DEBUG0_GPIO GPIOD
#define DEBUG1 GPIO_Pin_6
#define DEBUG1_GPIO GPIOD
#define DEBUG2 GPIO_Pin_8
#define DEBUG2_GPIO GPIOA
#define DEBUG3 GPIO_Pin_7
#define DEBUG3_GPIO GPIOC

//DEBUG pins
#ifdef DEBUG_ENABLED

#define DEBUG0_ON DEBUG0_GPIO->BSRRL = DEBUG0
#define DEBUG0_OFF DEBUG0_GPIO->BSRRH = DEBUG0
#define DEBUG1_ON DEBUG1_GPIO->BSRRL = DEBUG1
#define DEBUG1_OFF DEBUG1_GPIO->BSRRH = DEBUG1
#define DEBUG2_ON DEBUG2_GPIO->BSRRL = DEBUG2
#define DEBUG2_OFF DEBUG2_GPIO->BSRRH = DEBUG2
#define DEBUG3_ON DEBUG3_GPIO->BSRRL = DEBUG3
#define DEBUG3_OFF DEBUG3_GPIO->BSRRH = DEBUG3

#else

#define DEBUG0_ON
#define DEBUG0_OFF
#define DEBUG1_ON
#define DEBUG1_OFF
#define DEBUG2_ON
#define DEBUG2_OFF
#define DEBUG3_ON
#define DEBUG3_OFF

#endif

void init_dig_inouts(void);
void test_dig_inouts(void);
void test_noise(void);
void deinit_dig_inouts(void);
uint8_t get_PCB_version(void);
