#include "globals.h"
#include "circular_buffer.h"
#include "circular_buffer_cache.h"



//
//Given: the starting address of the cache, and the address in the buffer to which it refers.
//Returns: a cache address equivalent to the buffer_point
//Assumes cache_start is the lowest value of the cache 
//
uint32_t map_buffer_to_cache(uint32_t buffer_point, uint8_t sampleByteSize, uint32_t cache_start, uint32_t buffer_start, CircularBuffer *b)
{
	uint32_t p;

	//Find out how far ahead the buffer_point is from the buffer reference
	p = CB_distance_points(buffer_point, buffer_start, b->size, 0);

	//Divide that by 2 to get the number of samples
	//and multiply by the sampleByteSize to get the position in the cache
	p = (p * sampleByteSize) >> 1;

	//add that to the cache reference
	p += cache_start;

	return(p);
}

//
//Given: the starting address of the cache, and the address in the buffer to which it refers.
//Returns: a buffer address equivalent to cache_point
//Assumes cache_start is the lowest value of the cache
//
uint32_t map_cache_to_buffer(uint32_t cache_point, uint8_t sampleByteSize, uint32_t cache_start, uint32_t buffer_start, CircularBuffer *b)
{
	uint32_t p;

	if (cache_point < cache_start)
		return(b->min);//error condition

	//Find how far ahead the cache_point is from the start of the cache
	p = cache_point - cache_start;

	//Find how many samples that is
	p = p/sampleByteSize;

	//Multiply that by 2 to get the address offset in b
	p *= 2;

	//Add the offset to the start of the buffer
	p += buffer_start;

	//Shorter way to write it is this:
	//p = buffer_start + (((cache_point - cache_start) * 2) / sampleByteSize);

	//Wrap the circular buffer
	while (p > b->max)
		p -= b->size;

	return(p);
}
