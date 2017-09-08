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

#define ADCVALS_PER_OCTAVE 408.0
#define CENTER_ADC 2048
int main(void){
	int i;
	double v;
	double pitch_per_adcval;

	pitch_per_adcval = pow(2.0, 1.0/ADCVALS_PER_OCTAVE);

	printf ("const float voltoct[4096]={\n");

	for (i=0;i<4096;i++)
	{
		v = pow(pitch_per_adcval, (CENTER_ADC - i));

		printf("%.16g",v);
		if (i!=4095) 	printf(",");
		printf("\t//%d\n",i);
	}
	printf ("};\n");

	return(0);
}
