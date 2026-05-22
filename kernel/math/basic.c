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
KAPI double floorf(double value){
	return (double)((uint64_t)value);
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
