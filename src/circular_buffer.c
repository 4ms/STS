
#include "globals.h"
#include "circular_buffer.h"


uint8_t CB_offset_in_address(CircularBuffer *b, uint32_t amt, uint8_t subtract)
{

	if (!subtract)
	{
		if ((b->max - b->in) <= amt) //same as "if ((b->in + amt) >= b->max)" but doing the math this way avoids overflow in case b->max == 0xFFFFFFFF
		{
			b->in -= b->size - amt;
			if (b->wrapping) return(1); //warning: already wrapped, and wrapped again!
			b->wrapping = 1;
		}
		else
			b->in += amt;
	}
	else
	{
		if ((b->in - b->min) < amt) //same as "if (b->in - amt) < b->min" but avoids using negative numbers in case amt > b->in
		{
			b->in += b->size - amt;
			if (!b->wrapping)return(1); //warning: already unwrapped!
			b->wrapping = 0;
		}
		else
			b->in -= amt;
	}
	return(0);
}


uint8_t CB_offset_out_address(CircularBuffer *b, uint32_t amt, uint8_t subtract)
{
	if (!subtract)
	{
		if ((b->max - b->out) <= amt) //same as "if (b->out + amt) > b->max" but doing the math this way avoids overflow in case b->max == 0xFFFFFFFF
		{
			b->out -= b->size - amt;
			if (!b->wrapping)return(1); //warning: already unwrapped!
			b->wrapping = 0;
		}
		else
			b->out += amt;
	}
	else
	{
		if ((b->out - b->min) < amt) //same as "if (b->out - amt) < b->min" but avoids using negative numbers in case amt > b->out
		{
			b->out += b->size - amt;
			if (b->wrapping) return(1); //warning: already wrapped, and wrapped again!
			b->wrapping = 1;
		}
		else
			b->out -= amt;
	}
	return(0);
}

uint32_t CB_distance_points(uint32_t leader, uint32_t follower, uint32_t size, uint8_t reverse)
{

	if (reverse)
	{
		if (follower >= leader)
			return (follower - leader);
		else	//wrapping
			return ((follower + size) - leader);
	}
	else
	{
		if (leader >= follower)
			return (leader - follower);
		else	//wrapping
			return ((leader + size) - follower);
	}

}

uint32_t CB_distance(CircularBuffer *b, uint8_t reverse)
{

	if (reverse)
	{
		if (b->out >= b->in)
			return (b->out - b->in);
		else	//wrapping
			return ((b->out + b->size) - b->in);
	}
	else
	{
		if (b->in >= b->out)
			return (b->in - b->out);
		else	//wrapping
			return ((b->in + b->size) - b->out);
	}

}

void CB_init(CircularBuffer *b, uint8_t rev)
{
	b->wrapping = 0;

	// if (rev)
	// {
	// 	b->in 	= b->max;
	// 	b->out 	= b->max;
	// }
	// else
	// {
		b->in 	= b->min;
		b->out 	= b->min;
	// }

}

