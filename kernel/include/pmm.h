#ifndef _PMM
#define _PMM
#define MEM_GB 1073741824
#define MEM_MB 1048576
#define MEM_KB 1024
#define PAGE_SIZE 4096
enum pageStatus{
	PAGE_INVALID,
	PAGE_FREE,
	PAGE_INUSE,
};
struct p_page{
	enum pageStatus status;
	uint64_t virtualAddress;
	uint64_t physicalAddress;
};
struct p_pt_info{
	uint64_t pt_size;
	uint64_t freeEntryCnt;
	uint64_t usedEntryCnt;
	struct p_page* pPageEntries;
	struct p_page** pFreeEntries;
	struct p_page** pUsedEntries;
};
int pmm_init(void);
int getInstalledMemory(uint64_t* pInstalledMemory);
int getFreeMemory(uint64_t* pFreeMemory);
int allocatePageTable(void);
int initPageTable(void);
int physicalAllocPage(uint64_t* pPhysicalAddress);
int physicalFreePage(uint64_t physicalAddress);
#endif
