#ifndef _MSR
#define _MSR
#include <stdint.h>
__attribute__((ms_abi)) uint64_t read_msr(uint64_t msr);
__attribute__((ms_abi)) int write_msr(uint64_t msr, uint64_t value);
#endif
