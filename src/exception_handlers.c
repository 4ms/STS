#include "globals.h"
#include <stm32f4xx.h>

#ifdef DEBUG_ENABLED
void NMI_Handler(void) {
	while (1)
		;
}

void HardFault_Handler(void) {
	volatile uint8_t foobar;
	uint32_t hfsr, dfsr, afsr, bfar, mmfar, cfsr;

	volatile uint8_t pause = 1;

	foobar = 1;
	mmfar = SCB->MMFAR;
	bfar = SCB->BFAR;

	hfsr = SCB->HFSR;
	afsr = SCB->AFSR;
	dfsr = SCB->DFSR;
	cfsr = SCB->CFSR;

	__BKPT();
	if (foobar) {
		return;
	} else {
		while (pause) {
		};
	}
}

void MemManage_Handler(void) {
	while (1)
		;
}

void BusFault_Handler(void) {
	while (1)
		;
}

void UsageFault_Handler(void) {
	while (1)
		;
}

void SVC_Handler(void) {
	while (1)
		;
}

void DebugMon_Handler(void) {
	while (1)
		;
}

void PendSV_Handler(void) {
	while (1)
		;
}

#else

void NMI_Handler(void) {
	NVIC_SystemReset();
}

void HardFault_Handler(void) {
	NVIC_SystemReset();
}

void MemManage_Handler(void) {
	NVIC_SystemReset();
}

void BusFault_Handler(void) {
	NVIC_SystemReset();
}

void UsageFault_Handler(void) {
	NVIC_SystemReset();
}

void SVC_Handler(void) {
	NVIC_SystemReset();
}

void DebugMon_Handler(void) {
	NVIC_SystemReset();
}

void PendSV_Handler(void) {
	NVIC_SystemReset();
}
#endif
