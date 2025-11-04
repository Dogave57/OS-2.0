#ifndef _PMM
#define _PMM
#define MEM_GB 1073741824
#define MEM_MB 1048576
#define MEM_KB 1024
#define PAGE_SIZE 4096
#define MAX_ORDER 2048
enum pageStatus{
	PAGE_INVALID,
	PAGE_FREE,
	PAGE_INUSE,
	PAGE_RESERVED,
};
struct p_page{
	enum pageStatus status;
};
struct p_pt_info{
	uint64_t pt_size;
	uint64_t freeEntryCnt;
	uint64_t usedEntryCnt;
	struct p_page* pPageEntries;
	struct p_page** pFreeEntries;
	struct p_page** pUsedEntries;
};
extern uint64_t installedMemory;
extern uint64_t freeMemory;
extern uint64_t totalMemory;
int pmm_init(void);
int getTotalMemory(uint64_t* pTotalMemory);
int getInstalledMemory(uint64_t* pInstalledMemory);
int getFreeMemory(uint64_t* pFreeMemory);
int allocatePageTable(void);
int initPageTable(void);
int physicalAllocPage(uint64_t* pPhysicalAddress);
int physicalFreePage(uint64_t physicalAddress);
int physicalMapPage(uint64_t physicalAddress);
int physicalAllocRaw(uint64_t* pPhysicalAddress, uint64_t size);
int physicalFreeRaw(uint64_t physicalAddress, uint64_t size);
int getPhysicalPageTable(uint64_t* pPa, uint64_t* pSize);
#endif
