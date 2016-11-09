/*
 * audio_memory.c
 *
 *  Created on: Apr 6, 2015
 *      Author: design
 */

#include "globals.h"
#include "audio_sdram.h"
#include "sdram_driver.h"
#include "audio_util.h"
#include "sampler.h"
#include "params.h"
#include "dig_pins.h"

const uint32_t AUDIO_MEM_BASE[4] = {SDRAM_BASE, SDRAM_BASE + MEM_SIZE, SDRAM_BASE + MEM_SIZE*2, SDRAM_BASE + MEM_SIZE*3};

extern uint8_t SAMPLINGBYTES;

void memory_clear(uint8_t channel)
{
	uint32_t i;

	//Takes 700ms to clear the channel buffer in 32-bit chunks, roughly 83ns per write
	for(i = AUDIO_MEM_BASE[channel]; i < (AUDIO_MEM_BASE[channel] + MEM_SIZE); i += 4)
			*((uint32_t *)i) = 0x00000000;

}

uint32_t memory_read_sample(uint32_t addr)
{
	// Enforce valid addr range
	if ((addr<SDRAM_BASE) || (addr > (SDRAM_BASE + SDRAM_SIZE)))
	addr=SDRAM_BASE;

	// even addresses only
	addr &= 0xFFFFFFFE;

	while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

	if (SAMPLINGBYTES==2)
		return (*((int16_t *)addr));
	else
		return (*((int32_t *)addr));

}

//
// Reads from SDRAM memory starting at address addr[channel], for a length of num_samples words (16 or 32, depending on SAMPLINGBYTES)
// Read a 0 at the stop_at_addr, and every address after that.
// Returns the number of 0's filled (0 if we never hit stop_at_addr)
//
uint32_t memory_read(uint32_t *addr, uint8_t channel, int32_t *rd_buff, uint32_t num_samples, uint32_t stop_at_addr, uint8_t decrement)
{
	uint32_t i;
	uint32_t num_filled=0;

	//Loop of 8 takes 2.5us
	//read from SDRAM. first one takes 200us, subsequent reads take 50ns
	for (i=0;i<num_samples;i++){

		// Enforce valid addr range
		if ((*addr<SDRAM_BASE) || (*addr > (SDRAM_BASE + SDRAM_SIZE)))
		*addr=SDRAM_BASE;

		// even addresses only
		*addr = (*addr & 0xFFFFFFFE);

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (*addr==stop_at_addr)	num_filled=1;
		else if (num_filled)		num_filled++;

		if (num_filled)				rd_buff[i] = 0;
		else
		{
			if (SAMPLINGBYTES==2)
				rd_buff[i] = *((int16_t *)(*addr));
			else
				rd_buff[i] = *((int32_t *)(*addr));
		}

		*addr = inc_addr_within_limits(*addr, AUDIO_MEM_BASE[channel], AUDIO_MEM_BASE[channel] + MEM_SIZE, decrement);

	}

	return(num_filled);
}

//
// Reads from SDRAM memory starting at address addr[channel], for a length of num_samples words (16 or 32, depending on SAMPLINGBYTES)
//
uint32_t memory_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement)
{
	uint32_t i;
	uint32_t heads_crossed=0;

	for (i=0;i<num_samples;i++){

		//Enforce valid addr range
		if ((*addr<SDRAM_BASE) || (*addr > (SDRAM_BASE + SDRAM_SIZE)))
			*addr=SDRAM_BASE;

		//even addresses only
		*addr &= 0xFFFFFFFE;

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (SAMPLINGBYTES==2)
			*((int16_t *)*addr) = wr_buff[i];
		else
			*((int32_t *)*addr) = wr_buff[i];

		*addr = inc_addr_within_limits(*addr, AUDIO_MEM_BASE[channel], AUDIO_MEM_BASE[channel] + MEM_SIZE, decrement);

		if (*addr==detect_crossing_addr) heads_crossed=1;

	}

	return(heads_crossed);

}


uint32_t memory_read_channel(uint32_t *addr, uint8_t channel, int32_t *rd_buff, uint32_t num_samples, uint32_t detect_crossing_addr, uint8_t decrement){
	uint32_t i;
	uint32_t num_detected=0;

	//Loop of 8 takes 2.5us
	//read from SDRAM. first one takes 200us, subsequent reads take 50ns
	for (i=0;i<num_samples;i++){

		// Enforce valid addr range
		if ((addr[channel]<SDRAM_BASE) || (addr[channel] > (SDRAM_BASE + SDRAM_SIZE)))
		addr[channel]=SDRAM_BASE;

		// even addresses only
		addr[channel] = (addr[channel] & 0xFFFFFFFE);

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (SAMPLINGBYTES==2)
			rd_buff[i] = *((int16_t *)(addr[channel]));
		else
			rd_buff[i] = *((int32_t *)(addr[channel]));

		// Count the number of addresses read from past, and including, the detect_crossing_addr
		if (num_detected) num_detected++;
		else if (addr[channel]==detect_crossing_addr) num_detected=1;

		addr[channel] = inc_addr(addr[channel], channel, decrement);

	}

	return(num_detected);
}


uint32_t memory_write_channel(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint8_t decrement)
{
	uint32_t i;

	for (i=0;i<num_samples;i++){

		//Enforce valid addr range
		if ((addr[channel]<SDRAM_BASE) || (addr[channel] > (SDRAM_BASE + SDRAM_SIZE)))
			addr[channel]=SDRAM_BASE;

		//even addresses only
		addr[channel] = (addr[channel] & 0xFFFFFFFE);

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (SAMPLINGBYTES==2)
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
uint32_t memory_fade_write(uint32_t *addr, uint8_t channel, int32_t *wr_buff, uint32_t num_samples, uint8_t decrement, float fade){
	uint32_t i;
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
		if (SAMPLINGBYTES==2)
			rd = *((int16_t *)(addr[channel]));
		else
			rd = *((int32_t *)(addr[channel]));

		mix = ((float)wr_buff[i] * fade) + ((float)rd * (1.0-fade));

		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		if (SAMPLINGBYTES==2)
			*((int16_t *)addr[channel]) = mix;
		else
			*((int32_t *)addr[channel]) = mix;

		addr[channel] = inc_addr(addr[channel], channel, decrement);

	}

	return 0;

}


uint32_t RAM_test(void){

	uint32_t addr;
	uint32_t i;
	uint16_t rd0;
	uint16_t rd1;
	volatile uint32_t fail=0;

	addr=SDRAM_BASE;
	for (i=0;i<(SDRAM_SIZE/2);i++){
		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}


		rd1 = (uint16_t)((i) & 0x0000FFFF);
		*((uint16_t *)addr) = rd1;

		addr+=2;


	}

	addr=SDRAM_BASE;
	for (i=0;i<(SDRAM_SIZE/2);i++){
		while(FMC_GetFlagStatus(FMC_Bank2_SDRAM, FMC_FLAG_Busy) != RESET){;}

		rd1 = *((uint16_t *)addr);

		rd0=(uint16_t)((i) & 0x0000FFFF);
		if (rd1 != rd0)
		{
			fail++;
		}

		addr+=2;

	}

	return(fail);
}


void RAM_startup_test(void)
{
	volatile register uint32_t ram_errors=0;


	ram_errors = RAM_test();

	PLAYLED1_ON;
	PLAYLED2_ON;
	CLIPLED1_ON;
	CLIPLED2_ON;


	//Display the number of bad memory addresses using the seven lights (up to 15 can be shown)
	//If there's 15 or more bad memory addresses, then flash all the lights

	if (ram_errors & 1)
		PLAYLED1_OFF;
	if (ram_errors & 2)
		CLIPLED1_OFF;
	if (ram_errors & 4)
		CLIPLED2_OFF;
	if (ram_errors & 8)
		PLAYLED2_OFF;

	while (1)
	{
		if (ram_errors >= 15)
		{
			blink_all_lights(50);
		}
		else if (ram_errors == 0)
		{
			chase_all_lights(50);
		}
	}


}

