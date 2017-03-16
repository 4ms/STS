
#include "globals.h"
#include "circular_buffer.h"

//in = leader
//out = follower
//if in>out then wrapping should be 0
//if in<out then wrapping should be 1 (in has "wrapped" around max->min)

//uint8_t check_wrap(CircularBuffer* b)
//{
//	if (b->wrapping && (b->in > b->out))
//		return(2); //wrap error
//
//	else if (!b->wrapping && (b->in < b->out))
//		return(2); //wrap error
//
//	else
//		return(0);
//
//}

uint8_t CB_offset_in_address(CircularBuffer *b, uint32_t amt, uint8_t subtract)
{

	if (!subtract)
	{
		if ((b->max - b->in) < amt) //same as "if (b->in + amt) > b->max" but doing the math this way avoids overflow in case b->max == 0xFFFFFFFF
		{
			//if (b->out > b->in) 
			//	b->wrapping = 0;
			//else 
				b->wrapping = 1;

			b->in -= b->size - amt;

	//		if (b->wrapping)
	//			return(1); //warning: already wrapped, and wrapped again!
	//		else
		}
		else
			b->in += amt;
	}
	else
	{
		if ((b->in - b->min) < amt) //same as "if (b->in - amt) < b->min" but avoids using negative numbers in case amt > b->in
		{
			//if (b->out > b->in) 
				b->wrapping = 0;
			//else 
			//	b->wrapping = 1;

			b->in += b->size - amt;
//			if (b->wrapping)
		//		b->wrapping = 0;
//			else
//				return(1); //warning: already unwrapped!
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
		if ((b->max - b->out) < amt) //same as "if (b->out + amt) > b->max" but doing the math this way avoids overflow in case b->max == 0xFFFFFFFF
		{
			//offsetting by amt caused a wrap from max to min
			b->out -= b->size - amt;

//			if (b->wrapping)
				b->wrapping = 0;
//			else
//				return(1); //warning: already unwrapped!
		}
		else
			b->out += amt;
	}
	else
	{
		if ((b->out - b->min) < amt) //same as "if (b->out - amt) < b->min" but avoids using negative numbers in case amt > b->out
		{
			//offsetting by amt caused a wrap from min to max
			b->out += b->size - amt;

			b->wrapping = 1;
		}
		else
			b->out -= amt;



	}

	return(0);
}

//uint32_t CB_distance_to_stop(CircularBuffer *b, uint8_t reverse)
//{
//
//	if (reverse)
//	{
//		if (b->out >= b->stop_pt)
//			return (b->out - b->stop_pt);
//		else	//wrapping
//			return ((b->out + b->size) - b->stop_pt);
//	}
//	else
//	{
//		if (b->stop_pt >= b->out)
//			return (b->stop_pt - b->out);
//		else	//wrapping
//			return ((b->stop_pt + b->size) - b->out);
//	}
//}

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
	b->filled 	= 0;
	b->stop_pt 	= 0;

	if (rev)
	{
		b->in 	= b->max - READ_BLOCK_SIZE;
		b->out 	= b->max;
		b->seam = b->max;
	}
	else
	{
		b->in 	= b->min;
		b->out 	= b->min;
		b->seam = b->min;
	}

}

