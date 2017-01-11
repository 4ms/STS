#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/malloc.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>


#ifdef T_MSVC
#include <float.h>
#define NAN nan_global
#endif

#include <sys/ioctl.h>
#include <sys/times.h>

//
//Empirically found constant, by taking the average of several readings:
//K = DESIRED_FREQ_MULTIPLIER^(1/(CENTER_ADC_VAL - MEASURED_ADCVAL))
//Where CENTER_ADC_VAL is the ADC value with 0V applied to the jack
//
#define K 1.00171259374942
#define CENTER_ADC 2048
int main(void){
	int i;
	double v;

	printf ("const float voltoct[4096]={\n");

	for (i=0;i<4096;i++)
	{
		v = pow(K,(CENTER_ADC - i));
		printf("%.16g",v);
		if (i!=4095) 	printf(",");
		printf("\t//%d",i);
		printf("\n");
	}
	printf ("};");
	printf("\n");

	return(0);
}
