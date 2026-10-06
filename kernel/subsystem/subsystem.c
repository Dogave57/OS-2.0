#include "stdlib/stdlib.h"
#include "mem/heap.h"
#include "mem/vmm.h"
#include "cpu/mutex.h"
#include "align.h"
#include "subsystem/subsystem.h"
struct subsystem_mem_info subsystemMemInfo = {0};
int subsystem_mem_init(void){
	struct subsystem_desc* pSubsystemDescList = (struct subsystem_desc*)0x00;
	uint64_t subsystemDescListSize = MAX_SUBSYSTEM_COUNT*sizeof(struct subsystem_desc);
	if (virtualAlloc((uint64_t*)&pSubsystemDescList, subsystemDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY)!=0){
		printf("failed to allocate subsystem descriptor list physical pages\r\n");
		return -1;
	}
	uint64_t* pFreeSubsystemList = (uint64_t*)0x00;
	uint64_t freeSubsystemListSize = MAX_SUBSYSTEM_COUNT*sizeof(uint64_t);
	if (virtualAlloc((uint64_t*)&pFreeSubsystemList, freeSubsystemListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY)!=0){
		printf("failed to allocate free subsystem descriptor list physical pages\r\n");
		return -1;
	}
	subsystemMemInfo.pSubsystemDescList = pSubsystemDescList;
	subsystemMemInfo.subsystemDescListSize = subsystemDescListSize;
	subsystemMemInfo.subsystemDescListLength = 0x00;
	subsystemMemInfo.pFreeSubsystemList = pFreeSubsystemList;
	subsystemMemInfo.freeSubsystemListSize = freeSubsystemListSize;
	subsystemMemInfo.freeSubsystemCount = 0x00;
	return 0;
}
int subsystem_init(struct subsystem_desc** ppSubsystemDesc, uint64_t maxEntryCount){
	if (!ppSubsystemDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct subsystem_desc* pSubsystemDesc = (struct subsystem_desc*)0x00;
	if (!subsystemMemInfo.freeSubsystemCount){
		pSubsystemDesc = (subsystemMemInfo.pSubsystemDescList+subsystemMemInfo.subsystemCount);
		subsystemMemInfo.subsystemCount++;
	}
	if (subsystemMemInfo.freeSubsystemCount){
		subsystemMemInfo.freeSubsystemCount--;
		pSubsystemDesc = subsystemMemInfo.pSubsystemDescList;
		pSubsystemDesc+=*(subsystemMemInfo.pFreeSubsystemList+subsystemMemInfo.freeSubsystemCount);
	}
	memset((void*)pSubsystemDesc, 0, sizeof(struct subsystem_desc));
	uint64_t* pEntryList = (uint64_t*)0x00;
	uint64_t* pFreeEntryList = (uint64_t*)0x00;
	uint64_t entryListSize = sizeof(uint64_t)*maxEntryCount;
	if (virtualAlloc((uint64_t*)&pEntryList, entryListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY)!=0){
		printf("failed to allocate subsystem descriptor entry QWORD list lazy allocated physical pages\r\n");
		mutex_unlock(&mutex);	
		return -1;
	}
	if (virtualAlloc((uint64_t*)&pFreeEntryList, entryListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY)!=0){
		printf("failed to allocate subsystem descriptor free entry QWORD list lazy allocated physical pages\r\n");
		virtualFree((uint64_t)pEntryList, entryListSize);
		mutex_unlock(&mutex);
		return -1;
	}
	pSubsystemDesc->pEntryList = pEntryList;
	pSubsystemDesc->entryListSize = entryListSize;
	pSubsystemDesc->pFreeEntryList = pFreeEntryList;
	pSubsystemDesc->freeEntryCount = 0x00;
	pSubsystemDesc->entryReserveCount = maxEntryCount;
	pSubsystemDesc->entryCommitCount = 0x00;
	*ppSubsystemDesc = pSubsystemDesc;
	mutex_unlock(&mutex);
	return 0;
}
int subsystem_deinit(struct subsystem_desc* pSubsystemDesc){
	if (!pSubsystemDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t subsystemDescIndex = (uint64_t)(pSubsystemDesc-subsystemMemInfo.pSubsystemDescList);
	*(subsystemMemInfo.pFreeSubsystemList+subsystemMemInfo.freeSubsystemCount) = subsystemDescIndex;
	subsystemMemInfo.freeSubsystemCount++;
	if (virtualFree((uint64_t)pSubsystemDesc->pEntryList, pSubsystemDesc->entryListSize)!=0){
		printf("failed to free subsystem descriptor entry QWORD list lazy allocated physical pages\r\n");
		virtualFree((uint64_t)pSubsystemDesc->pFreeEntryList, pSubsystemDesc->entryListSize);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualFree((uint64_t)pSubsystemDesc->pFreeEntryList, pSubsystemDesc->entryListSize)!=0){
		printf("failed to free subsystem descriptor free entry QWORD list lazy allocated physical pages\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int subsystem_get_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id, uint64_t* ppEntry){
	if (!pSubsystemDesc|!ppEntry)
		return -1;
	id--;
	uint64_t pEntry = *(pSubsystemDesc->pEntryList+id);
	*ppEntry = pEntry;
	return 0;
}
int subsystem_alloc_entry(struct subsystem_desc* pSubsystemDesc, unsigned char* pEntry, uint64_t* pId){
	if (!pSubsystemDesc||!pId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (!pSubsystemDesc->freeEntryCount){
		uint64_t id = pSubsystemDesc->entryCommitCount;
		*(pSubsystemDesc->pEntryList+id) = (uint64_t)pEntry;
		pSubsystemDesc->entryCommitCount++;
		*pId = id+0x01;
		mutex_unlock(&mutex);
		return 0;
	}
	pSubsystemDesc->freeEntryCount--;
	uint64_t id = (*(pSubsystemDesc->pFreeEntryList+pSubsystemDesc->freeEntryCount));
	*(pSubsystemDesc->pEntryList+id) = (uint64_t)pEntry;
	*pId = id+0x01;
	mutex_unlock(&mutex);
	return 0;
}
int subsystem_free_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id){
	if (!pSubsystemDesc||!id)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	id--;
	uint64_t freeEntry = pSubsystemDesc->freeEntryCount;
	*(pSubsystemDesc->pFreeEntryList+freeEntry) = id;
	pSubsystemDesc->freeEntryCount++;
	mutex_unlock(&mutex);
	return 0;
}
