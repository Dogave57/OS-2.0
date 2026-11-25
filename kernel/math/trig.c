#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "math/basic.h"
#include "math/trig.h"
KAPI double anglef_to_radf(double angle){
	return PI_F/(180.0/angle);
}
KAPI double cosf(double rad){
	rad = modf(rad, PI_F*2.0);
	double result = 1.0;
	double current_term = 1.0;
	for (uint64_t i = 1;i<16;i++){
		current_term*=-(rad*rad)/((2*i-1)*(2*i));
		result+=current_term;
	}
	return result;
}
KAPI double sinf(double rad){
	rad = modf(rad, PI_F*2.0);
	double rad2 = rad*rad;
	double result = rad;
	double current_term = rad;
	for (uint64_t i = 1;i<16;i++){
		current_term = current_term*(-rad2)/((2*i)*(2*i+1));
		result = result+current_term;
	}
	return result;
}
KAPI double tanf(double rad){
	return sinf(rad)/cosf(rad);
}

