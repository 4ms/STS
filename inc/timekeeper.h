/*
 * timekeeper.h
 *
 *  Created on: Mar 29, 2015
 *      Author: design
 */

#ifndef TIMEKEEPER_H_
#define TIMEKEEPER_H_
#include <stm32f4xx.h>


void init_timekeeper(void);

inline void inc_tmrs(void);
inline uint32_t get_sys_tmr(void);

void init_adc_param_update_IRQ(void);
void init_LED_PWM_IRQ(void);
void init_inputread_IRQ(void);



#endif /* TIMEKEEPER_H_ */
