#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "math/math.h"
uint64_t upowq(uint64_t value, uint64_t pow){
	if (!pow)
		return 1;
	if (value==2)
		return 2<<pow;
	for (uint64_t i = 0;i<pow-1;i++){
		value*=value;
	}
	return value;
}
double floorf(double value){
	return (double)((uint64_t)value);
}
double modf(double x, double y){
	if (!y)
		return 0.0;
	return x-(((double)(uint64_t)(x/y))*y);
}
double anglef_to_radf(double angle){
	return PI_F/(180.0/angle);
}
double cosf(double rad){
	rad = modf(rad, PI_F*2.0);
	double result = 1.0;
	double current_term = 1.0;
	for (uint64_t i = 1;i<16;i++){
		current_term*=-(rad*rad)/((2*i-1)*(2*i));
		result+=current_term;
	}
	return result;
}
double sinf(double rad){
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
