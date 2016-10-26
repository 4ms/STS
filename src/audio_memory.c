/*
 * audio_memory.c
 *
 *  Created on: Apr 6, 2015
 *      Author: design
 */

#include "globals.h"
#include "audio_memory.h"
#include "looping_delay.h"
#include "params.h"

extern const uint32_t AUDIO_MEM_BASE[NUM_CHAN];

extern uint8_t SAMPLESIZE;

void memory_clear(uint8_t channel)
{
	uint32_t i;

	//Takes 700ms to clear the channel buffer in 32-bit chunks, roughly 83ns per write
	for(i = AUDIO_MEM_BASE[channel]; i < (AUDIO_MEM_BASE[channel] + MEM_SIZE); i += 4)
			*((uint32_t *)i) = 0x00000000;

}

// Reads from SDRAM memory starting at address addr[channel], for a length of num_samples words (16 or 32, depending on SAMPLESIZE)
// loop_addr sets a threshold address value, and if memory_read reads from that address, the function returns 1.
// otherwise the function returns 0
// if decrement is set,
uint32_t memory_read(uint32_t *addr, uint8_t channel, int32_t *rd_buff, uint8_t num_samples, uint32_t loop_addr, uint8_t decrement){
	uint8_t i;
	uint32_t heads_crossed=0;

	//Loop of 8 takes 2.5us
	//read from SDRAM. first one takes 200us, subsequent reads take 50ns
	for (i=0;i<num_samples;i++){

		//Enforce valid addr range
		if ((addr[channel]<SDRAM_BASE) || (addr[channel] > (SDRAM_BASE + SDRAM_SIZE)))
		addr[channel]=SDRAM_BASE;

		//even addresses only
		addr[channel] = (addr[channel] & 0xFFFFFFFE);

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (SAMPLESIZE==2)
			rd_buff[i] = *((int16_t *)(addr[channel]));
		else
			rd_buff[i] = *((int32_t *)(addr[channel]));

		addr[channel] = inc_addr(addr[channel], channel, decrement);

		if (addr[channel]==loop_addr) heads_crossed=1;

	}

	return(heads_crossed);
}


uint32_t memory_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint8_t num_samples, uint8_t decrement)
{
	uint8_t i;

	for (i=0;i<num_samples;i++){

		//Enforce valid addr range
		if ((addr[channel]<SDRAM_BASE) || (addr[channel] > (SDRAM_BASE + SDRAM_SIZE)))
			addr[channel]=SDRAM_BASE;

		//even addresses only
		addr[channel] = (addr[channel] & 0xFFFFFFFE);

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (SAMPLESIZE==2)
			*((int16_t *)addr[channel]) = wr_buff[i];
		else
			*((int32_t *)addr[channel]) = wr_buff[i];

		addr[channel] = inc_addr(addr[channel], channel, decrement);

	}

	return(0);

}

//
// reads from the addr, and mixes that value with the value in wr_buff
// fade=1.0 means write 100% wr_buff and 0% read.
// fade=0.5 means write 50% wr_buff and 50% read.
// fade=0.0 means write 0% wr_buff and 100% read.
//
uint32_t memory_fade_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint8_t num_samples, uint8_t decrement, float fade){
	uint8_t i;
	int32_t rd;
	int32_t mix;

	for (i=0;i<num_samples;i++){

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		//Enforce valid addr range
		if ((addr[channel]<SDRAM_BASE) || (addr[channel] > (SDRAM_BASE + SDRAM_SIZE)))
			addr[channel]=SDRAM_BASE;

		//even addresses only
		addr[channel] = (addr[channel] & 0xFFFFFFFE);

		//read from address
		if (SAMPLESIZE==2)
			rd = *((int16_t *)(addr[channel]));
		else
			rd = *((int32_t *)(addr[channel]));

		mix = ((float)wr_buff[i] * fade) + ((float)rd * (1.0-fade));

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (SAMPLESIZE==2)
			*((int16_t *)addr[channel]) = mix;
		else
			*((int32_t *)addr[channel]) = mix;

		addr[channel] = inc_addr(addr[channel], channel, decrement);

	}

	return 0;

}

