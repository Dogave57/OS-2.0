#include "stdlib/stdlib.h"
#include "mem/heap.h"
#include "mem/vmm.h"
#include "cpu/mutex.h"
#include "align.h"
#include "subsystem/subsystem.h"
int subsystem_init(struct subsystem_desc** ppSubsystemDesc, uint64_t max_entries){
	if (!ppSubsystemDesc)
		return -1;
	struct subsystem_desc* pSubsystemDesc = (struct subsystem_desc*)kmalloc(sizeof(struct subsystem_desc));
	if (!pSubsystemDesc)
		return -1;
	memset((void*)pSubsystemDesc, 0, sizeof(struct subsystem_desc));
	uint64_t* pEntries = (uint64_t*)0x0;
	uint64_t* pFreeEntries = (uint64_t*)0x0;
	uint64_t entryReservePages = align_up(max_entries*sizeof(uint64_t), PAGE_SIZE)/PAGE_SIZE;
	uint64_t entryCommitPages = SUBSYSTEM_DEFAULT_COMMIT_PAGE_COUNT;
	if (virtualAllocPages((uint64_t*)&pEntries, entryReservePages, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	if (virtualAllocPages((uint64_t*)&pFreeEntries, entryReservePages, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	pSubsystemDesc->pEntries = pEntries;
	pSubsystemDesc->pFreeEntries = pFreeEntries;
	pSubsystemDesc->entryReserveCount = (entryReservePages*PAGE_SIZE)/sizeof(uint64_t);
	if (subsystem_commit_entries(pSubsystemDesc, PAGE_SIZE/sizeof(uint64_t))!=0){
		return -1;
	}
	*ppSubsystemDesc = pSubsystemDesc;
	return 0;
}
int subsystem_deinit(struct subsystem_desc* pSubsystemDesc){
	if (!pSubsystemDesc)
		return -1;
	if (virtualFree((uint64_t)pSubsystemDesc->pEntries, pSubsystemDesc->entryReserveCount*sizeof(uint64_t))!=0)
		return -1;
	if (virtualFree((uint64_t)pSubsystemDesc->pFreeEntries, pSubsystemDesc->entryReserveCount*sizeof(uint64_t))!=0)
		return -1;
	return 0;
}
int subsystem_get_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id, uint64_t* ppEntry){
	if (!pSubsystemDesc|!ppEntry)
		return -1;
	id--;
	uint64_t pEntry = pSubsystemDesc->pEntries[id];
	*ppEntry = pEntry;
	return 0;
}
int subsystem_alloc_entry(struct subsystem_desc* pSubsystemDesc, unsigned char* pEntry, uint64_t* pId){
	if (!pSubsystemDesc||!pId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (!pSubsystemDesc->freeEntryCount){
		if (subsystem_commit_entries(pSubsystemDesc, (PAGE_SIZE*8)/sizeof(uint64_t))!=0){
			mutex_unlock(&mutex);
			return -1;
		}
	}
	uint64_t id = pSubsystemDesc->pFreeEntries[pSubsystemDesc->freeEntryCount-1];
	if (id>=pSubsystemDesc->entryCommitCount){
		mutex_unlock(&mutex);
		return -1;
	}
	if (id>pSubsystemDesc->lastEntry){
		pSubsystemDesc->lastEntry = id;
		if (!(pSubsystemDesc->lastEntry%(PAGE_SIZE/sizeof(uint64_t)))){
			memset((void*)(pSubsystemDesc->pEntries+id), 0, PAGE_SIZE);
		}
	}
	pSubsystemDesc->pEntries[id] = (uint64_t)pEntry;
	pSubsystemDesc->freeEntryCount--;
	*pId = id+1;
	mutex_unlock(&mutex);
	return 0;
}
int subsystem_free_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id){
	if (!pSubsystemDesc||!id)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	id--;
	if (pSubsystemDesc->freeEntryCount>=pSubsystemDesc->entryReserveCount){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t freeEntry = pSubsystemDesc->freeEntryCount;
	pSubsystemDesc->pFreeEntries[freeEntry] = id;
	pSubsystemDesc->pEntries[id] = 0x0;
	pSubsystemDesc->freeEntryCount++;
	mutex_unlock(&mutex);
	return 0;
}
int subsystem_commit_entries(struct subsystem_desc* pSubsystemDesc, uint64_t commitEntryCount){
	if (!pSubsystemDesc||!commitEntryCount)
		return -1;
	if (pSubsystemDesc->entryCommitCount+commitEntryCount>=pSubsystemDesc->entryReserveCount){
		return -1;
	}
	for (uint64_t i = 0;i<commitEntryCount;i++){
		uint64_t entryId = pSubsystemDesc->entryCommitCount+commitEntryCount-i;
		if (subsystem_free_entry(pSubsystemDesc, entryId)!=0){
			printf("failed to commit free entry with ID: %d\r\n", entryId);
			return -1;
		}
	}
	pSubsystemDesc->entryCommitCount+=commitEntryCount;
	return 0;
}
