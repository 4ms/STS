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
#include "leds.h"
#include "circular_buffer.h"


extern uint8_t SAMPLINGBYTES;

void memory_clear(void)
{
	uint32_t i;
	//Takes 700ms to clear the channel buffer in 32-bit chunks, roughly 83ns per write
	for(i = SDRAM_BASE; i < (SDRAM_BASE + SDRAM_SIZE); i += 4)
			*((uint32_t *)i) = 0x00000000;
}



uint32_t memory_read16_cb(CircularBuffer* b, int16_t *rd_buff, uint32_t num_samples, uint8_t decrement)
{
	uint32_t i;
	uint32_t num_filled=0;

	for (i=0;i<num_samples;i++)
	{

		while(SDRAM_IS_BUSY){;}

		if (b->out==b->in)			num_filled=1;
		else if (num_filled)		num_filled++;

		if (num_filled)				rd_buff[i] = 0;
		else						rd_buff[i] = *((int16_t *)(b->out));

		CB_offset_out_address(b, 2, decrement);
		b->out = (b->out & 0xFFFFFFFE);

	}
	return (num_filled);
}

uint32_t memory_read24_cb(CircularBuffer* b, uint8_t *rd_buff, uint32_t num_samples, uint8_t decrement)
{
	uint32_t i;
	uint32_t num_filled=0;
	uint32_t num_bytes=num_samples*3;

	for (i=0;i<num_bytes;i++)
	{
		// if (b->out==b->in)			num_filled=1;
		// else if (num_filled)		num_filled++;

		while(SDRAM_IS_BUSY){;}

		// if (num_filled)				rd_buff[i] = 0;
		// else						
									rd_buff[i] = *((uint8_t *)(b->out));

		CB_offset_out_address(b, 1, decrement);
	}
	return (num_filled);
}


//Grab 16-bit ints and write them into b as 16-bit ints
//num_words should be the number of 32-bit words to read from wr_buff (bytes>>2)
uint32_t memory_write_16as16(CircularBuffer* b, uint32_t *wr_buff, uint32_t num_words, uint8_t decrement)
{
	uint32_t i;
	uint8_t heads_crossed=0;
	uint8_t start_polarity, end_polarity, start_wrap, end_wrap;

	//b->in = (b->in & 0xFFFFFFFE);

	//better way of detecting head-crossing:
	start_polarity = (b->in < b->out) ? 0 : 1;
	start_wrap = b->wrapping;

	for (i=0;i<num_words;i++)
	{
		*((uint32_t *)b->in) = wr_buff[i];

		while(SDRAM_IS_BUSY){;}

		CB_offset_in_address(b, 4, decrement);
	}

	end_polarity = (b->in < b->out) ? 0 : 1;
	end_wrap = b->wrapping; //0 or 1

	//start_polarity + end_polarity  is (0/2 if no change, 1 if change)
	//start_wrap + end_wrap is (0/2 if no change, 1 if change)
	//Thus the sum of all four is even unless just polarity or just wrap changes (but not both)

	if ((end_wrap + start_wrap + start_polarity + end_polarity) & 0b01) //if (sum is odd)
		return(1); //warning: in pointer and out pointer crossed
	else
		return(0); //pointers did not cross

}


//Grab 24-bit words and write them into b as 16-bit values
uint32_t memory_write_24as16(CircularBuffer* b, uint8_t *wr_buff, uint32_t num_bytes, uint8_t decrement)
{
	uint32_t i;
	uint8_t heads_crossed=0;
	uint8_t start_polarity, end_polarity, start_wrap, end_wrap;

	start_polarity = (b->in < b->out) ? 0 : 1;
	start_wrap = b->wrapping;

	for (i=0;i<num_bytes;i+=3) //must be a multiple of 3!
	{
		*((int16_t *)b->in) = (int16_t)(wr_buff[i+2]<<8 | wr_buff[i+1]);

		while(SDRAM_IS_BUSY){;}

		CB_offset_in_address(b, 2, decrement);
	}

	end_polarity = (b->in < b->out) ? 0 : 1;
	end_wrap = b->wrapping; //0 or 1

	if ((end_wrap + start_wrap + start_polarity + end_polarity) & 0b01) 
		return(1); //warning: in pointer and out pointer crossed
	else
		return(0); //pointers did not cross

}

//Grab 32-bit words and write them into b as 16-bit values
uint32_t memory_write_32ias16(CircularBuffer* b, uint8_t *wr_buff, uint32_t num_bytes, uint8_t decrement)
{
	uint32_t i;
	uint8_t heads_crossed=0;
	uint8_t start_polarity, end_polarity, start_wrap, end_wrap;

	start_polarity = (b->in < b->out) ? 0 : 1;
	start_wrap = b->wrapping;

	for (i=0;i<num_bytes;i+=4)
	{
		*((int16_t *)b->in) = (int16_t)(wr_buff[i+3]<<8 | wr_buff[i+2]);

		while(SDRAM_IS_BUSY){;}

		CB_offset_in_address(b, 2, decrement);
	}

	end_polarity = (b->in < b->out) ? 0 : 1;
	end_wrap = b->wrapping; //0 or 1

	if ((end_wrap + start_wrap + start_polarity + end_polarity) & 0b01) 
		return(1); //warning: in pointer and out pointer crossed
	else
		return(0); //pointers did not cross

}

//Grab 32-bit floats and write them into b as 16-bit values
uint32_t memory_write_32fas16(CircularBuffer* b, float *wr_buff, uint32_t num_floats, uint8_t decrement)
{
	uint32_t i;
	uint8_t heads_crossed=0;
	uint8_t start_polarity, end_polarity, start_wrap, end_wrap;

	start_polarity = (b->in < b->out) ? 0 : 1;
	start_wrap = b->wrapping;

	for (i=0;i<num_floats;i++)
	{
		if (wr_buff[i] >= 1.0) 			*((int16_t *)b->in) = 32767;
		else if (wr_buff[i] <= -1.0) 	*((int16_t *)b->in) = -32768;
		else 							*((int16_t *)b->in) = (int16_t)(wr_buff[i]*32767.0);

		while(SDRAM_IS_BUSY){;}

		CB_offset_in_address(b, 2, decrement);
	}

	end_polarity = (b->in < b->out) ? 0 : 1;
	end_wrap = b->wrapping; //0 or 1

	if ((end_wrap + start_wrap + start_polarity + end_polarity) & 0b01) 
		return(1); //warning: in pointer and out pointer crossed
	else
		return(0); //pointers did not cross

}

//Grab 8-bit ints from wr_buff and write them into b as 16-bit ints
uint32_t memory_write_8as16(CircularBuffer* b, uint8_t *wr_buff, uint32_t num_bytes, uint8_t decrement)
{
	uint32_t i;
	uint8_t heads_crossed=0;
	uint8_t start_polarity, end_polarity, start_wrap, end_wrap;

	//setup to detect head-crossing:
	start_polarity = (b->in < b->out) ? 0 : 1;
	start_wrap = b->wrapping;

	for (i=0;i<num_bytes;i++)
	{
		*((int16_t *)b->in) = ((int16_t)(wr_buff[i])-128)*256;

		while(SDRAM_IS_BUSY){;}

		CB_offset_in_address(b, 2, decrement);
	}

	end_polarity = (b->in < b->out) ? 0 : 1;
	end_wrap = b->wrapping; //0 or 1

	if ((end_wrap + start_wrap + start_polarity + end_polarity) & 0b01) //if (sum is odd)
		return(1); //warning: in pointer and out pointer crossed
	else
		return(0); //pointers did not cross

}


uint32_t memory_write16_cb(CircularBuffer* b, int16_t *wr_buff, uint32_t num_samples, uint8_t decrement)
{
	uint32_t i;
	uint32_t heads_crossed=0;

	b->in = (b->in & 0xFFFFFFFE);

	for (i=0;i<num_samples;i++)
	{

		*((int16_t *)b->in) = wr_buff[i];

		while(SDRAM_IS_BUSY){;}

		CB_offset_in_address(b, 2, decrement);

		if (b->in == b->out) //don't consider the heads being crossed if they begin at the same place
			heads_crossed = b->out;

	}

	return (heads_crossed);
}



/*
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

		while(SDRAM_IS_BUSY){;}

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

		while(SDRAM_IS_BUSY){;}

		if (SAMPLINGBYTES==2)
			*((int16_t *)addr[channel]) = mix;
		else
			*((int32_t *)addr[channel]) = mix;

		addr[channel] = inc_addr(addr[channel], channel, decrement);

	}

	return 0;

}
*/

uint32_t RAM_test(void){

	uint32_t addr;
	uint32_t i;
	uint16_t rd0;
	uint16_t rd1;
	volatile uint32_t fail=0;

	addr=SDRAM_BASE;
	for (i=0;i<(SDRAM_SIZE/2);i++){

		if (i & 0x80000) 	PLAYLED1_ON;
		else 				SIGNALLED_ON;

		while(SDRAM_IS_BUSY){;}

		if (i & 0x80000) 	PLAYLED1_OFF;
		else				SIGNALLED_OFF;

		rd1 = (uint16_t)((i) & 0x0000FFFF);
		*((uint16_t *)addr) = rd1;

		addr+=2;


	}

	addr=SDRAM_BASE;
	for (i=0;i<(SDRAM_SIZE/2);i++){

		if (i & 0x80000) 	PLAYLED2_ON;
		else 				BUSYLED_ON;

		while(SDRAM_IS_BUSY){;}

		if (i & 0x80000) 	PLAYLED2_OFF;
		else				BUSYLED_OFF;

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


	// Testing SDRAM timing:
	// Results:
	// reading a 4-byte word takes on average 218ns = 54.5 ns/Byte
	// reading a 2-byte word takes on average 204ns = 102 ns/Byte
	//
	//	volatile uint32_t i32;
	//	volatile int32_t dummy;
	//	volatile int32_t dummy16;
	//
	//
	//	for (i32=0;i32<100000;i32++)
	//	{
	//		DEBUG0_ON;
	//		dummy = *((int32_t *)(0xD0000000 + i32*4));
	//		dummy += *((int32_t *)(0xD0000000 + i32*4 + 500));
	//		dummy += *((int32_t *)(0xD0000000 + i32*4 + 1500));
	//		dummy += *((int32_t *)(0xD0000000 + i32*4 + 2000));
	//		DEBUG0_OFF;
	//
	//		DEBUG1_ON;
	//		dummy16 = *((int16_t *)(0xD1000000 + i32*2));
	//		dummy16 += *((int16_t *)(0xD1000000 + i32*2 + 500));
	//		dummy16 += *((int16_t *)(0xD1000000 + i32*2 + 1500));
	//		dummy16 += *((int16_t *)(0xD1000000 + i32*2 + 2000));
	//		DEBUG1_OFF;
	//		if (dummy == 156 || dummy16 == 298)
	//			DEBUG3_ON;
	//	}


	ram_errors = RAM_test();

	PLAYLED1_ON;
	PLAYLED2_ON;
	SIGNALLED_ON;
	BUSYLED_ON;


	//Display the number of bad memory addresses using the seven lights (up to 15 can be shown)
	//If there's 15 or more bad memory addresses, then flash all the lights

	if (ram_errors & 1)
		PLAYLED1_OFF;
	if (ram_errors & 2)
		SIGNALLED_OFF;
	if (ram_errors & 4)
		BUSYLED_OFF;
	if (ram_errors & 8)
		PLAYLED2_OFF;

	while (1)
	{
		if (ram_errors >= 15)
			blink_all_lights(50);
		else if (ram_errors == 0)
			chase_all_lights(100);
	}


}

