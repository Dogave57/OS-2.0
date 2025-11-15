#ifndef _PMM
#define _PMM
#include <stdint.h>
#include "vmm.h"
#define MEM_GB 1073741824
#define MEM_MB 1048576
#define MEM_KB 1024
#define PAGE_SIZE 4096
#define MAX_ORDER 2048
#define PAGE_TYPE_RESERVED 0
#define PAGE_TYPE_FREE 1
#define PAGE_TYPE_NORMAL 2
#define PAGE_TYPE_STACK 3
#define PAGE_TYPE_HEAP 4
#define PAGE_TYPE_MMIO 5
#define PAGE_TYPE_FIRMWARE_DATA 6
#define PAGE_TYPE_PMM 7
#define PAGE_TYPE_VMM 8
#define PAGE_INVALID 0
#define PAGE_FREE 1
#define PAGE_INUSE 2
#define PAGE_RESERVED 3
struct p_page{
	uint8_t status;
	uint8_t pageType;
}__attribute__((packed));
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
int physicalAllocPage(uint64_t* pPhysicalAddress, uint8_t pageType);
int physicalFreePage(uint64_t physicalAddress);
int physicalMapPage(uint64_t physicalAddress, uint8_t pageType);
int physicalUnmapPage(uint64_t physicalAddress);
int physicalAllocRaw(uint64_t* pPhysicalAddress, uint64_t size);
int physicalFreeRaw(uint64_t physicalAddress, uint64_t size);
int getPhysicalPageTable(uint64_t* pPa, uint64_t* pSize);
int getUsedPhysicalPages(uint64_t* pUsedPages);
int getFreePhysicalPages(uint64_t* pFreePages);
#endif
