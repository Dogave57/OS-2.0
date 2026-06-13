#include "mem/vmm.h"
#include "stdlib/stdlib.h"
#include "math/basic.h"
KAPI uint64_t upowq(uint64_t value, uint64_t pow){
	uint64_t result = 0x00;
	for (uint64_t i = 0;i<pow;i++){
		result*=value;
	}
	return result;
}
KAPI double powf(double value, int64_t exponent){
	double result = value;
	for (uint64_t i = 0;i<absq(exponent);i++){
		result*=result;
	}
	if (exponent<0x00)
		result = 1.0/result;
	return result;
}
KAPI uint64_t absq(int64_t value){
	return (value<0x00) ? -value : value;
}
KAPI double floorf(double value){
	return (double)((int64_t)value);
}
KAPI double ceilf(double value){
	return (double)((int64_t)(value+1.0));
}
KAPI double roundf(double value){
	return (double)floorf(((value-floorf(value))>=0.5) ? value+1.0 : value);
}
KAPI double absf(double value){
	uint64_t realValue = *(uint64_t*)&value;
	if (realValue&((uint64_t)1<<63))
		realValue&=~((uint64_t)1<<63);
	return *(double*)&realValue;
}
KAPI double modf(double x, double y){
	if (!y)
		return 0.0;
	return x-(floorf(x/y)*y);
}
KAPI double sqrtf(double value){
	return value*sqrtf_inv(value);
}
KAPI double sqrtf_inv(double value){
	double half = value*0.5;
	uint64_t i = *(uint64_t*)&value;
	i = 0x5FE6EB50C7B537A9-(i>>1);
	value = *(double*)&i;
	value = value*(1.5-(half*value*value));
	value = value*(1.5-(half*value*value));
	return value;
}
