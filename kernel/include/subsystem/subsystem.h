#ifndef _SUBSYSTEM
#define _SUBSYSTEM
#include <stdint.h>
#define MAX_SUBSYSTEM_COUNT (16384)
#define SUBSYSTEM_DEFAULT_COMMIT_PAGE_COUNT (1)
struct subsystem_mem_info{
	struct subsystem_desc* pSubsystemDescList;
	uint64_t subsystemDescListSize;
	uint64_t subsystemDescListLength;
	uint64_t subsystemCount;
	uint64_t* pFreeSubsystemList;
	uint64_t freeSubsystemListSize;
	uint64_t freeSubsystemCount;
};
struct subsystem_desc{
	uint64_t* pEntryList;
	uint64_t entryListSize;
	uint64_t* pFreeEntryList;
	uint64_t freeEntryCount;
	uint64_t entryReserveCount;
	uint64_t entryCommitCount;
};
int subsystem_mem_init(void);
int subsystem_init(struct subsystem_desc** ppSubsystemDesc, uint64_t max_entries);
int subsystem_deinit(struct subsystem_desc* pSubsystemDesc);
int subsystem_get_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id, uint64_t* ppEntry);
int subsystem_alloc_entry(struct subsystem_desc* pSubsystemDesc, unsigned char* pEntry, uint64_t* pId);
int subsystem_free_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id);
#endif
