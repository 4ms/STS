#include <stm32f4xx.h>
#include "ITM.h"

char *itoa (int value, char *result, int base)
{
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}


void ITM_Print_int(int port, uint32_t v)
{
	char *p;
	const char *q;

	q = itoa(v,p,10);

	ITM_Print(port, q);
}



// Print a given string to ITM.
// This uses 8 bit writes, as that seems to be the most common way to write text
// through ITM. Otherwise there is no good way for the PC software to know what
// is text and what is some other data.
void ITM_Print(int port, const char *p)
{
    if ((ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & (1UL << port)))
    {
        while (*p)
        {
            while (ITM->PORT[port].u32 == 0);
            ITM->PORT[port].u8 = *p++;
        }
    }
}

// Write a 32-bit value to ITM.
// This can be used as a fast way to log important values from code.
void ITM_SendValue (int port, uint32_t value)
{
    if ((ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & (1UL << port)))
    {
        while (ITM->PORT[port].u32 == 0);
        ITM->PORT[port].u32 = value;
    }
}

#define ITM_ENA (*(volatile unsigned int*)0xE0000E00) // ITM Enable
#define ITM_TPR (*(volatile unsigned int*)0xE0000E40) // Trace Privilege Register
#define ITM_TCR (*(volatile unsigned int*)0xE0000E80) // ITM Trace Control Reg.
#define ITM_LSR (*(volatile unsigned int*)0xE0000FB0) // ITM Lock Status Register
#define DHCSR (*(volatile unsigned int*)0xE000EDF0) // Debug register
//#define DEMCR (*(volatile unsigned int*)0xE000EDFC) // Debug register
#define TPIU_ACPR (*(volatile unsigned int*)0xE0040010) // Async Clock presacler register
#define TPIU_SPPR (*(volatile unsigned int*)0xE00400F0) // Selected Pin Protocol Register
#define DWT_CTRL (*(volatile unsigned int*)0xE0001000) // DWT Control Register
//#define FFCR (*(volatile unsigned int*)0xE0040304) // Formatter and flus

void TRACE_init(void)
{

	  uint32_t SWOPrescaler;


	  // DWT->CYCCNT = 0;
	  // DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	//  SWOPrescaler = (180000000 / SWOSpeed) - 1;  // SWOSpeed in Hz
	 //  *((volatile unsigned *)0xE0040010) = SWOPrescaler; // "Async Clock Prescaler Register". Scale the baud rate of the asynchronous output

       //0xE000EDFC = 0x01000000;   // "Debug Exception and Monitor Control Register (DEMCR)"
       CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

       //0xE0040004
	   TPI->CSPSR = (1<<3); //Current Parallel Port Size Register: port size =4 (bit 3)

       //0xE0040304
	   TPI->FFCR = TPI_FFCR_TrigIn_Msk | TPI_FFCR_EnFCont_Msk; // Formatter and Flush Control Register: EnFCont and TrigIn (bits 1 and 8)

       //0xE00400F0
       TPI->SPPR = 0; //Selected Pin Protocol = 0: sync Trace Port Mode, 1: SWO - manchester, 2: SWO - NRZ

       //0xE0042004
	   DBGMCU->CR = DBGMCU_CR_TRACE_IOEN | DBGMCU_CR_TRACE_MODE_0 | DBGMCU_CR_TRACE_MODE_1;

       //0xE0042008
       DBGMCU->APB1FZ = 0x000001FF \
                        | DBGMCU_APB1_FZ_DBG_RTC_STOP | DBGMCU_APB1_FZ_DBG_WWDG_STOP | DBGMCU_APB1_FZ_DBG_IWDG_STOP \
                        | DBGMCU_APB1_FZ_DBG_I2C1_SMBUS_TIMEOUT | DBGMCU_APB1_FZ_DBG_I2C2_SMBUS_TIMEOUT | DBGMCU_APB1_FZ_DBG_I2C3_SMBUS_TIMEOUT \
                        | DBGMCU_APB1_FZ_DBG_CAN1_STOP | DBGMCU_APB1_FZ_DBG_CAN2_STOP;

       //0xE004200C
       DBGMCU->APB2FZ = 0x00070003; //DBGMCU_APB1_FZ_DBG_TIM1_STOP | DBGMCU_APB1_FZ_DBG_TIM8_STOP | DBGMCU_APB1_FZ_DBG_TIM9_STOP | DBGMCU_APB1_FZ_DBG_TIM10_STOP | DBGMCU_APB1_FZ_DBG_TIM11_STOP;

       /*
        #define  DBGMCU_APB1_FZ_DBG_TIM2_STOP            ((uint32_t)0x00000001)
        #define  DBGMCU_APB1_FZ_DBG_TIM3_STOP            ((uint32_t)0x00000002)
        #define  DBGMCU_APB1_FZ_DBG_TIM4_STOP            ((uint32_t)0x00000004)
        #define  DBGMCU_APB1_FZ_DBG_TIM5_STOP            ((uint32_t)0x00000008)
        #define  DBGMCU_APB1_FZ_DBG_TIM6_STOP            ((uint32_t)0x00000010)
        #define  DBGMCU_APB1_FZ_DBG_TIM7_STOP            ((uint32_t)0x00000020)
        #define  DBGMCU_APB1_FZ_DBG_TIM12_STOP           ((uint32_t)0x00000040)
        #define  DBGMCU_APB1_FZ_DBG_TIM13_STOP           ((uint32_t)0x00000080)
        #define  DBGMCU_APB1_FZ_DBG_TIM14_STOP           ((uint32_t)0x00000100)
        #define  DBGMCU_APB1_FZ_DBG_RTC_STOP             ((uint32_t)0x00000400)
        #define  DBGMCU_APB1_FZ_DBG_WWDG_STOP            ((uint32_t)0x00000800)
        #define  DBGMCU_APB1_FZ_DBG_IWDG_STOP            ((uint32_t)0x00001000)
        #define  DBGMCU_APB1_FZ_DBG_I2C1_SMBUS_TIMEOUT   ((uint32_t)0x00200000)
        #define  DBGMCU_APB1_FZ_DBG_I2C2_SMBUS_TIMEOUT   ((uint32_t)0x00400000)
        #define  DBGMCU_APB1_FZ_DBG_I2C3_SMBUS_TIMEOUT   ((uint32_t)0x00800000)
        #define  DBGMCU_APB1_FZ_DBG_CAN1_STOP            ((uint32_t)0x02000000)
        #define  DBGMCU_APB1_FZ_DBG_CAN2_STOP            ((uint32_t)0x04000000)
        // Old IWDGSTOP bit definition, maintained for legacy purpose 
        #define  DBGMCU_APB1_FZ_DBG_IWDEG_STOP           DBGMCU_APB1_FZ_DBG_IWDG_STOP

        //These should be APB2 ???
        #define  DBGMCU_APB1_FZ_DBG_TIM1_STOP        ((uint32_t)0x00000001)
        #define  DBGMCU_APB1_FZ_DBG_TIM8_STOP        ((uint32_t)0x00000002)
        #define  DBGMCU_APB1_FZ_DBG_TIM9_STOP        ((uint32_t)0x00010000)
        #define  DBGMCU_APB1_FZ_DBG_TIM10_STOP       ((uint32_t)0x00020000)
        #define  DBGMCU_APB1_FZ_DBG_TIM11_STOP       ((uint32_t)0x00040000)
        */


        //0xE0000FB0    // ITM Lock Access Register, C5ACCE55 enables more write access to Control Register 0xE00 :: 0xFFC
        ITM->LAR = 0xC5ACCE55;

        //0xE0000E80 = 0x0001000D;   // ITM Trace Control Register
        ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_TSENA_Msk | ITM_TCR_SYNCENA_Msk | ITM_TCR_DWTENA_Msk  | (1<<ITM_TCR_TraceBusID_Pos);

        //0xE0000E40 = 0x0000000F;   // ITM Trace Privilege Register
        ITM->TPR = 0x0000000F; //enable ports 0-7

        //0xE0000E00 = 0x00000001;   // ITM Trace Enable Register. Enabled tracing on stimulus ports. One bit per stimulus port.
        ITM -> TER = 0x00000001;

        //ETM->LAR //Lock Access
        *((volatile unsigned *)0xE0041FB0) = 0xC5ACCE55;

        //ETM->CR //Control
        *((volatile unsigned *)0xE0041000) = 0x00001D1E; //from Reference Manual: Configure the trace: but no explanation for why!!

        //ETM->TRIGEVENT //Trigger Event 
        *((volatile unsigned *)0xE0041008) = 0x0000406F; //from Reference Manual: Define the trigger event: but no explanation for why!!

        //ETM->TRENEVENT //Trace Enable Event 
        *((volatile unsigned *)0xE0041020) = 0x0000006F; //from Reference Manual: define an event to start/stop: but no explanation for why!!

        //ETM->TRSS //Trace Start/stop
        *((volatile unsigned *)0xE0041024) = 0x02000000; //from Reference Manual: enable the trace: but no explanation for why!!
        *((volatile unsigned *)0xE0041024) = 0x00000001; //from Reference Manual: but no explanation for why!!

        //ETM->CR //Control
        *((volatile unsigned *)0xE0041000) = 0x0000191E; //from Reference Manual: end of configuration: but no explanation for why!!
 

//DWT BASE
	//   *((volatile unsigned *)0xE0001000) = 0x400003FE;   // DWT_CTRL




}



void ITM_Disable(void)
{
	ITM_ENA &= ~(1 << 0);
	ITM_TCR = 0;
}
