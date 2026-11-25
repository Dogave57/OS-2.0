#ifndef _PMM
#define _PMM
#include <stdint.h>
#include "kernel_include.h"
#include "bootloader.h"
#include "mem/vmm.h"
#define MEM_GB ((uint64_t)1073741824)
#define MEM_MB ((uint64_t)1048576)
#define MEM_KB ((uint64_t)1024)
#define PAGE_SIZE (MEM_KB*0x04)
struct pmm_page_segment_desc{
	EFI_MEMORY_DESCRIPTOR* pPageDataMemoryDesc;
	unsigned char* pPageData;
	uint64_t pageDataCommit;
	uint64_t pageDataReserve;
};
struct pmm_page_data_info{
	struct pmm_page_segment_desc* pPageSegmentDescList;
	uint64_t pageSegmentDescListSize;
	uint64_t pageSegmentDescReserve;
	uint64_t pageSegmentDescCommit;
};
struct pmm_page_size_desc{
	uint64_t pageSize;
	uint64_t* pFreePageList;
	uint64_t freePageListSize;
	uint64_t freePageCount;
};
struct pmm_info{
	uint64_t totalMemory;
	uint64_t installedMemory;
	uint64_t freeMemory;
	struct pmm_page_data_info pageDataInfo;
	struct pmm_page_size_desc* pPageSizeDescList;
	uint64_t pageSizeDescListSize;
	uint64_t pageSizeDescCount;
};
int pmm_init(void);
KAPI int pmm_get_total_memory(uint64_t* pTotalMemory);
KAPI int pmm_get_installed_memory(uint64_t* pInstalledMemory);
KAPI int pmm_get_free_memory(uint64_t* pFreeMemory);
int physical_page_size_get_desc(uint64_t pageSize, struct pmm_page_size_desc** ppPageSizeDesc);
KAPI int physicalAlloc(uint64_t* pPhysicalAddress, uint64_t pageSize);
KAPI int physicalFree(uint64_t physicalAddress, uint64_t pageSize);
int physical_alloc_raw(uint64_t* pPhysicalAddress, uint64_t size);
int pmm_page_data_get_info(struct pmm_page_data_info** ppPhysicalPageDataInfo);
#endif
