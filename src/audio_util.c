#include "globals.h"
#include "audio_util.h"
#include "audio_sdram.h"
#include "sdram_driver.h"

extern const uint32_t AUDIO_MEM_BASE[4];

extern uint8_t SAMPLINGBYTES;

inline uint32_t offset_addr(uint32_t addr, uint8_t channel, int32_t offset)
{
	uint32_t t_addr;

	t_addr = addr + offset;

	//Range check
	while (t_addr >= (AUDIO_MEM_BASE[channel] + MEM_SIZE))
	{
		t_addr = t_addr - MEM_SIZE;
	}

	while (t_addr < AUDIO_MEM_BASE[channel])
	{
		t_addr = t_addr + MEM_SIZE;
	}

	return(t_addr);
}



uint32_t inc_addr(uint32_t addr, uint8_t channel, uint8_t direction)
{

	if (direction == 0)
	{
		addr+=SAMPLINGBYTES;
		if (addr >= (AUDIO_MEM_BASE[channel] + MEM_SIZE))
			addr = AUDIO_MEM_BASE[channel];
	}
	else
	{
		addr-=SAMPLINGBYTES;
		if (addr <= AUDIO_MEM_BASE[channel])
			addr = AUDIO_MEM_BASE[channel] + MEM_SIZE - SAMPLINGBYTES;
	}

	return(addr & 0xFFFFFFFE);

	//return (offset_samples(channel, addr, 1, mode[channel][REV]));
}


uint32_t inc_addr_within_limits(uint32_t addr, uint32_t low_limit, uint32_t high_limit, uint8_t direction)
{

	if (direction == 0)
	{
		addr+=SAMPLINGBYTES;
		if (addr >= high_limit)
			addr = low_limit;
	}
	else
	{
		addr-=SAMPLINGBYTES;
		if (addr <= low_limit)
			addr = high_limit - SAMPLINGBYTES;
	}

	return(addr & 0xFFFFFFFE);
}

uint32_t diff_circular(uint32_t leader, uint32_t follower, uint32_t wrap_size)
{
	if (leader==follower) return(0);
	else if (leader > follower) return(leader-follower);
	else return ((leader+wrap_size)-(follower+1));
}


uint32_t diff_wrap(uint32_t leader, uint32_t follower, uint8_t wrapping, uint32_t wrap_size)
{

	if (!wrapping)
	{
		if (leader > follower)
			return (leader - follower);
		else
			return 0;
	}
	else	//wrapping
	{
		if (follower > leader)
			return ((leader + MEM_SIZE) - follower);
		else
			return 0;

	}
}


// Utility function to determine if address mid is in between addresses beg and end in a circular (ring) buffer.
// ToDo: draw a truth table and condense this into as few as possible boolean logic functions
/*
 * There are four different ways three things can line up:
 * a<b<c
 * a=b<c
 * a<b=c
 * a=b=c
 *
 * And abc could be:
 * bme
 * bem
 * mbe
 * meb
 * emb
 * ebm
 *
 * So we start with 6*4 = 24 possibilities,
 * but some of them are the same (b<m=e is the same as b<e=m)
 * b<m<e --> 1
 * b<e<m --> 0
 * m<b<e --> 0
 * m<e<b --> 1
 * e<m<b --> 0
 * e<b<m --> 1
 *
 * b=m<e --> 1
 * b=e<m --> 0 ? beg=end error
 * m=e<b --> 1
 *
 * b<m=e --> 1
 * m<b=e --> 0 ? beg=end error
 * e<m=b --> 1
 *
 * b=m=e  --> 0 ? beg=end error
 *
 * Therefore we have 6 + 3 + 3 + 1 = 13 possibilities
 * if beg=end ---> error
 * if beg<end ---> b<=m && m<=e -> 1
 * if end<beg --->
 */
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


