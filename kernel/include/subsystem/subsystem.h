#ifndef _SUBSYSTEM
#define _SUBSYSTEM
#include <stdint.h>
#define SUBSYSTEM_DEFAULT_COMMIT_PAGE_COUNT (1)
struct subsystem_desc{
	uint64_t* pEntries;
	uint64_t* pFreeEntries;
	uint64_t freeEntryCount;
	uint64_t entryReserveCount;
	uint64_t entryCommitCount;
	uint64_t lastEntry;
};
int subsystem_init(struct subsystem_desc** ppSubsystemDesc, uint64_t max_entries);
int subsystem_deinit(struct subsystem_desc* pSubsystemDesc);
int subsystem_get_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id, uint64_t* ppEntry);
int subsystem_alloc_entry(struct subsystem_desc* pSubsystemDesc, unsigned char* pEntry, uint64_t* pId);
int subsystem_free_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id);
int subsystem_commit_entries(struct subsystem_desc* pSubsystemDesc, uint64_t commitEntryCount);
#endif
