#include "uart.h"

USART_TypeDef _uart;

void UART_init() {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

  GPIO_InitTypeDef gpio;
  GPIO_StructInit(&gpio);
  gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6;
  gpio.GPIO_Speed = GPIO_Speed_100MHz;
  gpio.GPIO_Mode = GPIO_Mode_AF;
  gpio.GPIO_OType = GPIO_OType_PP;
  gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOD, &gpio);
  GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART2);
  GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2);

  USART_InitTypeDef _uart_init;
  USART_StructInit(&_uart_init);
  _uart_init.USART_BaudRate = 9600; // 115200;
  _uart_init.USART_WordLength = USART_WordLength_8b;
  _uart_init.USART_StopBits = USART_StopBits_1;
  _uart_init.USART_Parity = USART_Parity_No;
  _uart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
  _uart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;

  USART_Init(USART2, &_uart_init);

  USART_Cmd(USART2, ENABLE);
}

void UART_send(const char *str, uint32_t len) {
  while (len--) {
    USART_SendData(USART2, *str++);

    // wait until Transfer Complete flag is set
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) != SET)
      ;
	
    // wait until TX fifo empty flag is set
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) != SET)
      ;
  }
}
