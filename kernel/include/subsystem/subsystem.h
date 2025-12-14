#ifndef _SUBSYSTEM
#define _SUBSYSTEM
#include <stdint.h>
struct subsystem_desc{
	uint64_t maxEntries;
	uint64_t freeEntries;
	uint64_t* pEntries;
	uint64_t* pFreeEntries;
	uint64_t entryPages;
};
int subsystem_init(struct subsystem_desc** ppSubsystemDesc, uint64_t max_entries);
int subsystem_deinit(struct subsystem_desc* pSubsystemDesc);
int subsystem_get_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id, uint64_t* ppEntry);
int subsystem_alloc_entry(struct subsystem_desc* pSubsystemDesc, unsigned char* pEntry, uint64_t* pId);
int subsystem_free_entry(struct subsystem_desc* pSubsystemDesc, uint64_t id);
#endif
