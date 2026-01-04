#include "stdlib/stdlib.h"
#include "drivers/gpu/framebuffer.h"
#include "panic.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
int mutex_lock(struct mutex_t* pMutex){
	if (!pMutex)
		return -1;
	uint64_t attempts = 0;
	while (pMutex->value){
		if (attempts>100000){
			__asm__ volatile("cli");
			pMutex->value = 0;
			panic("deadlock detected\r\n");
			__asm__ volatile("hlt");
			while (1){};
			return -1;
		}
		thread_yield();
		attempts++;
	}
	pMutex->old_rflags = get_rflags();
	__asm__ volatile("cli");
	pMutex->value = 1;
	return 0;
}
int mutex_unlock(struct mutex_t* pMutex){
	if (!pMutex)
		return -1;
	pMutex->value = 0;
	set_rflags(pMutex->old_rflags);
	return 0;
}
