#ifndef _KEXT_LOADER
#define _KEXT_LOADER
typedef int(*kextEntry)(void);
int kext_load(uint64_t mount_id, unsigned char* filename, uint64_t* pPid);
int kext_bootstrap(uint64_t pEntry);
#endif
