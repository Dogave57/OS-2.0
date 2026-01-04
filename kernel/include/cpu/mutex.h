#ifndef _MUTEX
#define _MUTEX
struct mutex_t{
	uint64_t value;
	uint64_t old_rflags;
};
int mutex_lock(struct mutex_t* pMutex);
int mutex_unlock(struct mutex_t* pMutex);
#endif
