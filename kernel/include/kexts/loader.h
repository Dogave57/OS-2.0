#ifndef _KEXT_LOADER
#define _KEXT_LOADER
typedef int(*kextEntry)(uint64_t pid);
struct kext_desc_t{
	struct elf_handle* pElfHandle;
	uint64_t pid;
};
struct kext_bootstrap_args_t{
	struct kext_desc_t* pKextDesc;	
};
int kext_subsystem_init(void);
int kext_load(uint64_t mount_id, unsigned char* filename, uint64_t* pPid);
int kext_unload(uint64_t pid);
int kext_register(struct elf_handle* pElfHandle, uint64_t* pPid);
int kext_unregister(uint64_t pid);
int kext_get_entry(uint64_t pid, struct kext_desc_t** ppEntry);
int kext_bootstrap(uint64_t tid, struct kext_bootstrap_args_t* pArgs);
#endif
