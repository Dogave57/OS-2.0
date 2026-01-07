#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/filesystem.h"
#include "subsystem/subsystem.h"
#include "drivers/timer.h"
#include "cpu/thread.h"
#include "align.h"
#include "elf.h"
#include "kexts/loader.h"
static struct subsystem_desc* pSubsystemDesc = (struct subsystem_desc*)0x0;
int kext_subsystem_init(void){
	if (subsystem_init(&pSubsystemDesc, MEM_KB*256)!=0)
		return -1;
	return 0;
}
int kext_load(uint64_t mount_id, unsigned char* filename, uint64_t* pPid){
	if (!filename||!pPid)
		return -1;
	uint64_t time_us = get_time_us();
	struct elf_handle* pHandle = (struct elf_handle*)0x0;
	if (elf_load(mount_id, filename, &pHandle)!=0)
		return -1;
	printf("took %dus to load elf\r\n", get_time_us()-time_us);
	uint64_t pid = 0;
	if (kext_register(pHandle, &pid)!=0){
		printf("failed to register kext\r\n");
		elf_unload(pHandle);
		return -1;
	}
	struct kext_desc_t* pKextDesc = (struct kext_desc_t*)kmalloc(sizeof(struct kext_desc_t));
	if (!pKextDesc){
		elf_unload(pHandle);
		return -1;
	}
	struct kext_bootstrap_args_t* pArgs = (struct kext_bootstrap_args_t*)kmalloc(sizeof(struct kext_bootstrap_args_t));
	if (!pArgs){
		elf_unload(pHandle);
		kfree((void*)pKextDesc);
		return -1;
	}
	memset((void*)pArgs, 0, sizeof(struct kext_bootstrap_args_t));
	pKextDesc->pid = pid;
	pKextDesc->pElfHandle = pHandle;
	pArgs->pKextDesc = pKextDesc;
	uint64_t tid = 0;
	if (thread_create((uint64_t)kext_bootstrap, 0, 0, &tid, (uint64_t)pArgs)!=0){
		printf("failed to create thread\r\n");
		kfree((void*)pArgs);
		elf_unload(pHandle);
		return -1;
	}
	while (!thread_exists(tid)){};
	kfree((void*)pArgs);
	if (kext_unregister(pid)!=0){
		printf("failed to unregister kext\r\n");
		elf_unload(pHandle);
		return -1;
	}
	elf_unload(pHandle);
	printf("kext finished\r\n");
	return 0;
}
int kext_unload(uint64_t pid){
	struct kext_desc_t* pKextDesc = (struct kext_desc_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, pid, (uint64_t*)&pKextDesc)!=0)
		return -1;
	if (elf_unload(pKextDesc->pElfHandle)!=0)
		return -1;
	if (kext_unregister(pid)!=0)
		return -1;
	return 0;
}
int kext_register(struct elf_handle* pElfHandle, uint64_t* pPid){
	if (!pElfHandle||!pPid)
		return -1;
	uint64_t pid = 0;
	struct kext_desc_t* pKextDesc = (struct kext_desc_t*)kmalloc(sizeof(struct kext_desc_t));
	if (!pKextDesc)
		return -1;
	memset((void*)pKextDesc, 0, sizeof(struct kext_desc_t));
	if (subsystem_alloc_entry(pSubsystemDesc, (unsigned char*)pKextDesc, &pid)!=0){
		kfree((void*)pKextDesc);
		return -1;
	}
	pKextDesc->pElfHandle = pElfHandle;
	pKextDesc->pid = pid;
	*pPid = pid;
	return 0;
}
int kext_unregister(uint64_t pid){
	struct kext_desc_t* pKextDesc = (struct kext_desc_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, pid, (uint64_t*)&pKextDesc)!=0)
		return -1;
	if (subsystem_free_entry(pSubsystemDesc, pid)!=0)
		return -1;
	return 0;
}
int kext_get_entry(uint64_t pid, struct kext_desc_t** ppEntry){
	if (!ppEntry)
		return -1;
	struct kext_desc_t* pEntry = (struct kext_desc_t*)0x0;
	if (subsystem_get_entry(pSubsystemDesc, pid, (uint64_t*)&pEntry)!=0)
		return -1;
	*ppEntry = pEntry;
	return 0;
}
int kext_bootstrap(uint64_t tid, struct kext_bootstrap_args_t* pArgs){
	if (!pArgs){
		printf("invalid arguments\r\n");
		while (1){};
		return -1;
	}
	if (!pArgs->pKextDesc){
		printf("invalid kext descriptor\r\n");
		while (1){};
		return -1;
	}
	if (!pArgs->pKextDesc->pElfHandle){
		printf("invalid ELF handle\r\n");
		while (1){};
		return -1;
	}
	kextEntry entry = (kextEntry)0x0;
	if (elf_get_entry(pArgs->pKextDesc->pElfHandle, (uint64_t*)&entry)!=0){
		printf("failed to get entry\r\n");
		while (1){};
		return -1;	
	}
	if (!entry){
		printf("invalid entry\r\n");
		while (1){};
		return -1;
	}
	uint64_t status = entry(pArgs->pKextDesc->pid);
	printf("program exited with status 0x%x\r\n", status);
	if (thread_destroy(tid)!=0){
		printf("failed to destroy thread\r\n");
		while (1){
			thread_yield();
		}
		return -1;
	}
	thread_yield();
	while (1){};	
	return 0;
}
