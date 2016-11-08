#include "globals.h"
#include "audio_util.h"
#include "audio_sdram.h"
#include "sdram_driver.h"

const uint32_t AUDIO_MEM_BASE[NUM_CHAN] = {SDRAM_BASE, SDRAM_BASE + MEM_SIZE};
extern uint8_t SAMPLESIZE;

inline uint32_t offset_samples(uint8_t channel, uint32_t base_addr, uint32_t offset, uint8_t subtract)
{
	uint32_t t_addr;

	//convert samples to addresses
	offset*=SAMPLESIZE;

	if (subtract == 0){

		t_addr = base_addr + offset;

		while (t_addr >= (AUDIO_MEM_BASE[channel] + MEM_SIZE))
			t_addr = t_addr - MEM_SIZE;

	} else {

		t_addr = base_addr - offset;

		while (t_addr < AUDIO_MEM_BASE[channel])
			t_addr = t_addr + MEM_SIZE;

	}

	if (SAMPLESIZE==2)
		t_addr = t_addr & 0xFFFFFFFE; //addresses must be even
	else
		t_addr = t_addr & 0xFFFFFFFC; //addresses must end in 00

	return (t_addr);
}



inline uint32_t inc_addr(uint32_t addr, uint8_t channel, uint8_t direction)
{

	if (direction == 0)
	{
		addr+=SAMPLESIZE;
		if (addr >= (AUDIO_MEM_BASE[channel] + MEM_SIZE))
			addr = AUDIO_MEM_BASE[channel];
	}
	else
	{
		addr-=SAMPLESIZE;
		if (addr <= AUDIO_MEM_BASE[channel])
			addr = AUDIO_MEM_BASE[channel] + MEM_SIZE - SAMPLESIZE;
	}

	return(addr & 0xFFFFFFFE);

	//return (offset_samples(channel, addr, 1, mode[channel][REV]));
}



// Utility function to determine if address mid is in between addresses beg and end in a circular (ring) buffer.
// ToDo: draw a truth table and condense this into as few as possible boolean logic functions
// b<m
// m<e
// b<m
// b==m
// b==e
// m==e
uint8_t in_between(uint32_t mid, uint32_t beg, uint32_t end, uint8_t reverse)
{
	uint32_t t;

	if (beg==end) //zero length, trivial case
	{
		if (mid!=beg) return(0);
		else return(1);
	}

	if (reverse) { //swap beg and end if we're reversed
		t=end;
		end=beg;
		beg=t;
	}

	if (end>beg) //not wrapped around
	{
		if ((mid>=beg) && (mid<=end)) return(1);
		else return(0);

	}
	else //end has wrapped around
	{
		if ((mid<=end) || (mid>=beg)) return(1);
		else return(0);
	}
}


//returns the absolute difference between the values
uint32_t abs_diff(uint32_t a1, uint32_t a2)
{
	if (a1>a2) return (a1-a2);
	else return (a2-a1);
}

inline uint32_t diff_circular(uint32_t leader, uint32_t follower, uint32_t wrap_size){
	if (leader==follower) return(0);
	else if (leader > follower) return(leader-follower);
	else return ((leader+wrap_size)-(follower+1));
}

