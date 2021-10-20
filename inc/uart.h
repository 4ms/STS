#pragma once
#include "stm32f4xx.h"

void UART_init();
void UART_send(const char *str, uint32_t len);
//
//
