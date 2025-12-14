#ifndef _CPUID
#define _CPUID
__attribute__((sysv_abi)) int cpuid(unsigned int leaf, unsigned int subleaf, unsigned int* peax, unsigned int* pebx, unsigned int* pecx, unsigned int* pedx);
#endif
