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
struct page{
	enum pageStatus status;
	uint64_t virtualAddress;
	uint64_t physicalAddress;
};
int pmm_init(void);
int getInstalledMemory(uint64_t* pInstalledMemory);
int getFreeMemory(uint64_t* pFreeMemory);
int allocatePageTable(struct page** ppPageTable, uint64_t* pPageTableSize);
#endif
