#include "stdlib/stdlib.h"
#include "mem/heap.h"
#include "mem/vmm.h"
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
	uint64_t entryPages = align_up(max_entries*sizeof(uint64_t), PAGE_SIZE)/PAGE_SIZE;
	if (virtualAllocPages((uint64_t*)&pEntries, entryPages, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	if (virtualAllocPages((uint64_t*)&pFreeEntries, entryPages, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	pSubsystemDesc->pEntries = pEntries;
	pSubsystemDesc->pFreeEntries = pFreeEntries;
	pSubsystemDesc->maxEntries = max_entries;
	for (uint64_t i = 0;i<max_entries;i++){
		if (subsystem_free_entry(pSubsystemDesc, max_entries-1-i)!=0)
			return -1;
	}
	*ppSubsystemDesc = pSubsystemDesc;
	return 0;
}
int subsystem_deinit(struct subsystem_desc* pSubsystemDesc){
	if (!pSubsystemDesc)
		return -1;
	for (uint64_t i = 0;i<pSubsystemDesc->maxEntries;i++){
		subsystem_free_entry(pSubsystemDesc, i);
	}
	return 0;
}
int subsystem_get_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id, uint64_t* ppEntry){
	if (!pSubsystemDesc||!ppEntry)
		return -1;
	uint64_t pEntry = pSubsystemDesc->pEntries[id];
	*ppEntry = pEntry;
	return 0;
}
int subsystem_alloc_entry(struct subsystem_desc* pSubsystemDesc, unsigned char* pEntry, uint64_t* pId){
	if (!pSubsystemDesc||!pId||!pEntry)
		return -1;
	if (!pSubsystemDesc->freeEntries)
		return -1;
	uint64_t id = pSubsystemDesc->pFreeEntries[pSubsystemDesc->freeEntries-1];
	if (id>=pSubsystemDesc->maxEntries)
		return -1;
	pSubsystemDesc->pEntries[id] = (uint64_t)pEntry;
	pSubsystemDesc->freeEntries--;
	*pId = id;
	return 0;
}
int subsystem_free_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id){
	if (!pSubsystemDesc)
		return -1;
	if (pSubsystemDesc->freeEntries>=pSubsystemDesc->maxEntries)
		return -1;
	pSubsystemDesc->pFreeEntries[pSubsystemDesc->freeEntries] = id;
	pSubsystemDesc->freeEntries++;
	return 0;
}
