#include <stdint.h>
#include "stdlib.h"
#include "pmm.h"
#include "vmm.h"
uint64_t**** pml4 = (uint64_t****)0x0;
int vmm_init(void){
	
	return 0;
}
int virtualAllocPage(uint64_t pa, uint64_t* pVirtualAddress){
	if (!pVirtualAddress)
		return -1;
	
	return 0;
}
int virtualFreePage(uint64_t va){

	return 0;
}
