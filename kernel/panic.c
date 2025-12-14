#include "stdlib/stdlib.h"
#include "panic.h"
int panic(unsigned char* pMessage){
	if (!pMessage)
		return -1;
	__asm__ volatile("cli");
	clear();
	printf("PANIC: %s", pMessage);
	__asm__ volatile("hlt");
	return 0;
}
