#ifndef _MUTEX
#define _MUTEX
#define SPINLOCK_MAX_ATTEMPTS (10000000000)
struct mutex_t{
	uint64_t value;
	uint64_t old_rflags;
};
int mutex_lock(struct mutex_t* pMutex);
int mutex_unlock(struct mutex_t* pMutex);
int mutex_lock_isr_safe(struct mutex_t* pMutex);
int mutex_unlock_isr_safe(struct mutex_t* pMutex);
#endif
