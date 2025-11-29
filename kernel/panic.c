#include "stdlib/stdlib.h"
#include "panic.h"
int panic(CHAR16* pMessage){
	if (!pMessage)
		return -1;
	__asm__ volatile("cli");
	clear();
	printf(L"PANIC: %s", pMessage);
	__asm__ volatile("hlt");
	return 0;
}
