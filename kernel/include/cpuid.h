#ifndef _CPUID
#define _CPUID
int cpuid(unsigned int leaf, unsigned int subleaf, unsigned int* peax, unsigned int* pebx, unsigned int* pecx, unsigned int* pedx);
#endif
