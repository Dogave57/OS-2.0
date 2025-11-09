#include "cpuid.h"
int cpuid(unsigned int leaf, unsigned int subleaf, unsigned int* peax, unsigned int* pebx, unsigned int* pecx, unsigned int* pedx){
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	__asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(leaf), "c"(subleaf));
	if (peax)
		*peax = eax;
	if (pebx)
		*pebx = ebx;
	if (pecx)
		*pecx = ecx;
	if (pedx)
		*pedx = edx;
	return 0;
}
