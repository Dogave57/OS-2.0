#ifndef _MSR
#define _MSR
#include <stdint.h>
uint64_t read_msr(uint64_t msr);
int write_msr(uint64_t msr, uint64_t value);
#endif
