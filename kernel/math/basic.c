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
KAPI uint64_t absq(int64_t value){
	return (value<0x00) ? -value : value;
}
KAPI int64_t minq(int64_t value1, int64_t value2){
	int64_t result = (value1<value2) ? value1 : value2;
	return result;
}
KAPI int64_t maxq(int64_t value1, int64_t value2){
	int64_t result = (value1>value2) ? value1 : value2;
	return result;
}
KAPI uint64_t uminq(uint64_t value1, uint64_t value2){
	uint64_t result = (value1<value2) ? value1 : value2;
	return result;
}
KAPI uint64_t umaxq(uint64_t value1, uint64_t value2){
	uint64_t result = (value1>value2) ? value1 : value2;
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
KAPI double minf(double value1, double value2){
	double result = (value1<value2) ? value1 : value2;
	return result;
}
KAPI double maxf(double value1, double value2){
	double result = (value1>value2) ? value1 : value2;
	return result;
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
