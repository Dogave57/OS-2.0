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
		if (attempts>SPINLOCK_MAX_ATTEMPTS){
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
	uint64_t newFlags = get_rflags();
	newFlags = (pMutex->old_rflags&(1<<9)) ? newFlags|(1<<9) : newFlags&~(1<<9);
	set_rflags(newFlags);
	return 0;
}
int mutex_lock_isr_safe(struct mutex_t* pMutex){
	if (!pMutex)
		return -1;
	uint64_t attempts = 0;
	while (pMutex->value){
		if (attempts>SPINLOCK_MAX_ATTEMPTS){
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
	pMutex->oldSchedulerState = scheduler_halted();
	scheduler_halt();
	pMutex->old_rflags = get_rflags();
	__asm__ volatile("sti");
	pMutex->value = 1;
	return 0;
}
int mutex_unlock_isr_safe(struct mutex_t* pMutex){
	if (!pMutex)
		return -1;
	pMutex->value = 0;
	uint64_t newFlags = get_rflags();
	newFlags = (pMutex->old_rflags&(1<<9)) ? newFlags|(1<<9) : newFlags&~(1<<9);
	set_rflags(newFlags);
	if (!pMutex->oldSchedulerState)
		scheduler_resume();
	else
		scheduler_halt();
	return 0;
}
