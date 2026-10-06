#include "stdlib/stdlib.h"
#include "cpu/cpuid.h"
#include "align.h"
#include "bootloader.h"
#include "panic.h"
#include "cpu/mutex.h"
#include "subsystem/text.h"
#include "mem/pmm.h"
struct pmm_info pmmInfo = {0};
int pmm_init(void){
	if (pmm_get_total_memory(&pmmInfo.totalMemory)!=0){
		printf("failed to get total volatile memory\r\n");
		return -1;
	}
	if (pmm_get_installed_memory(&pmmInfo.installedMemory)!=0){
		printf("failed to get installed volatile physical memory\r\n");
		return -1;
	}
	if (pmm_get_free_memory(&pmmInfo.freeMemory)!=0){
		printf("failed to get free volatile physical memory\r\n");
		return -1;
	}
	struct pmm_page_data_info pageDataInfo = {0};
	memset((void*)&pageDataInfo, 0x00, sizeof(struct pmm_page_data_info));
	struct pmm_page_segment_desc mainPageSegmentDesc = {0};
	memset((void*)&mainPageSegmentDesc, 0x00, sizeof(struct pmm_page_segment_desc));
	uint64_t freeMemoryDescCount = 0x00;
	uint64_t memoryDescCount = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (uint64_t i = 0x00;i<memoryDescCount;i++){
		EFI_MEMORY_DESCRIPTOR* pMemoryDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		uint64_t memoryLength = pMemoryDesc->NumberOfPages*PAGE_SIZE;
		if (pMemoryDesc->Type!=EfiConventionalMemory||memoryLength<(MEM_MB*0x10))
			continue;
		freeMemoryDescCount++;
		if (memoryLength<=mainPageSegmentDesc.pageDataReserve)
			continue;
		mainPageSegmentDesc.pPageDataMemoryDesc = pMemoryDesc;
		mainPageSegmentDesc.pPageData = (unsigned char*)pMemoryDesc->PhysicalStart;
		mainPageSegmentDesc.pageDataReserve = memoryLength;
		mainPageSegmentDesc.pageDataCommit = 0x00;
	}
	mainPageSegmentDesc.pPageDataMemoryDesc->Type = EfiReservedMemoryType;
	struct pmm_page_segment_desc* pPageSegmentDescList = (struct pmm_page_segment_desc*)mainPageSegmentDesc.pPageData;
	uint64_t pageSegmentDescListSize = sizeof(struct pmm_page_segment_desc)*freeMemoryDescCount;
	mainPageSegmentDesc.pageDataCommit+=align_up(pageSegmentDescListSize, PAGE_SIZE);
	*pPageSegmentDescList = mainPageSegmentDesc;
	pageDataInfo.pPageSegmentDescList = pPageSegmentDescList;
	pageDataInfo.pageSegmentDescListSize = pageSegmentDescListSize;
	pageDataInfo.pageSegmentDescReserve = 0x01;
	pageDataInfo.pageSegmentDescCommit = 0x01;
	for (uint64_t i = 0x00;i<memoryDescCount;i++){
		EFI_MEMORY_DESCRIPTOR* pMemoryDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		uint64_t memoryLength = pMemoryDesc->NumberOfPages*PAGE_SIZE;
		if (pMemoryDesc->Type!=EfiConventionalMemory||memoryLength<(MEM_MB*0x10))
			continue;
		struct pmm_page_segment_desc* pPageSegmentDesc = pPageSegmentDescList+pageDataInfo.pageSegmentDescReserve;
		memset((void*)pPageSegmentDesc, 0x00, sizeof(struct pmm_page_segment_desc));
		pPageSegmentDesc->pPageDataMemoryDesc = pMemoryDesc;
		pPageSegmentDesc->pPageData = (unsigned char*)pMemoryDesc->PhysicalStart;
		pPageSegmentDesc->pageDataReserve = memoryLength;
		pPageSegmentDesc->pageDataCommit = 0x00;
		pMemoryDesc->Type = EfiReservedMemoryType;
		pageDataInfo.pageSegmentDescReserve++;
	}
	uint64_t pageSizeDescCount = 0x08;
	struct pmm_page_size_desc* pPageSizeDescList = (struct pmm_page_size_desc*)(pPageSegmentDescList->pPageData+pPageSegmentDescList->pageDataCommit);
	uint64_t pageSizeDescListSize = sizeof(struct pmm_page_size_desc)*pageSizeDescCount;
	pPageSegmentDescList->pageDataCommit+=align_up(pageSizeDescListSize, PAGE_SIZE);
	uint64_t pageSize = PAGE_SIZE;
	for (uint64_t i = 0x00;i<pageSizeDescCount;i++){
		struct pmm_page_size_desc* pPageSizeDesc = pPageSizeDescList+i;
		uint64_t* pFreePageList = (uint64_t*)(pPageSegmentDescList->pPageData+pPageSegmentDescList->pageDataCommit);
		uint64_t freePageListSize = align_up((pmmInfo.freeMemory/pageSize)*sizeof(uint64_t), PAGE_SIZE);
		memset((void*)pPageSizeDesc, 0, sizeof(struct pmm_page_size_desc));
		pPageSizeDesc->pageSize = pageSize;
		pPageSizeDesc->pFreePageList = pFreePageList;
		pPageSizeDesc->freePageListSize = freePageListSize;
		pPageSizeDesc->freePageCount = 0x00;
		pPageSegmentDescList->pageDataCommit+=align_up(freePageListSize, PAGE_SIZE);
		pageSize*=0x02;
	}
	pmmInfo.pageDataInfo = pageDataInfo;
	pmmInfo.pPageSizeDescList = pPageSizeDescList;
	pmmInfo.pageSizeDescListSize = pageSizeDescListSize;
	pmmInfo.pageSizeDescCount = pageSizeDescCount;
	printf("installed volatile memory + hardware volatile memory: %dmb\r\n", pmmInfo.totalMemory/MEM_MB);
	printf("installed volatile memory: %dmb\r\n", pmmInfo.installedMemory/MEM_MB);
	printf("free volatile memory: %dmb\r\n", pmmInfo.freeMemory/MEM_MB);
	uint32_t eax = 0x00;
	if (cpuid(0x80000008, 0, &eax, (uint32_t*)0x0, (uint32_t*)0x0, (uint32_t*)0x0)!=0)
		return -1;
	uint64_t paBits = (uint64_t)(eax&0xFF);
	uint64_t maxPhysicalAddress = (((uint64_t)1)<<paBits)-1;
	return 0;
}
KAPI int pmm_get_total_memory(uint64_t* pTotalMemory){
	if (!pTotalMemory)
		return -1;
	if (pmmInfo.totalMemory){
		*pTotalMemory = pmmInfo.totalMemory;
		return 0;
	}
	uint64_t memoryDescCount = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (uint64_t i = 0x00;i<memoryDescCount;i++){
		EFI_MEMORY_DESCRIPTOR* pMemoryDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		pmmInfo.totalMemory+=(pMemoryDesc->NumberOfPages*PAGE_SIZE);
	}
	*pTotalMemory = pmmInfo.totalMemory;
	return 0;
}
KAPI int pmm_get_installed_memory(uint64_t* pInstalledMemory){
	if (!pInstalledMemory)
		return -1;
	if (pmmInfo.installedMemory){
		*pInstalledMemory = pmmInfo.installedMemory;
		return 0;
	}
	uint64_t memoryDescCount = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (uint64_t i = 0x00;i<memoryDescCount;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(pbootargs->memoryInfo.memoryDescSize*i));
		if (pMemDesc->Type==EfiReservedMemoryType||pMemDesc->Type==EfiMemoryMappedIO||pMemDesc->Type==EfiMemoryMappedIOPortSpace||pMemDesc->Type==EfiACPIMemoryNVS||pMemDesc->Type==EfiACPIReclaimMemory)
			continue;
		pmmInfo.installedMemory+=(pMemDesc->NumberOfPages*PAGE_SIZE);
	}
	*pInstalledMemory = pmmInfo.installedMemory;
	return 0;
}
KAPI int pmm_get_free_memory(uint64_t* pFreeMemory){
	if (!pFreeMemory)
		return -1;
	if (pmmInfo.freeMemory){
		*pFreeMemory = pmmInfo.freeMemory;
		return 0;
	}
	uint64_t memoryDescCount = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (uint64_t i = 0x00;i<memoryDescCount;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiConventionalMemory)
			continue;
		pmmInfo.freeMemory+=(pMemDesc->NumberOfPages*PAGE_SIZE);
	}
	*pFreeMemory = pmmInfo.freeMemory;
	return 0;
}
int physical_page_size_get_desc(uint64_t dataSize, struct pmm_page_size_desc** ppPageSizeDesc){
	if (!dataSize||!ppPageSizeDesc)
		return -1;
	for (uint64_t i = 0x00;i<pmmInfo.pageSizeDescCount;i++){
		struct pmm_page_size_desc* pPageSizeDesc = pmmInfo.pPageSizeDescList+i;
		if (dataSize<pPageSizeDesc->pageSize)
			continue;
		*ppPageSizeDesc = pPageSizeDesc;
		return 0;
	}
	return -1;
}
KAPI int physicalAlloc(uint64_t* pPhysicalAddress, uint64_t dataSize){
	if (!pPhysicalAddress||!dataSize)
		return -1;
	struct pmm_page_size_desc* pPageSizeDesc = (struct pmm_page_size_desc*)0x00;
	if (physical_page_size_get_desc(dataSize, &pPageSizeDesc)!=0){
		printf("failed to get volatile physical page QWORD size descriptor\r\n");
		return -1;
	}
	dataSize = pPageSizeDesc->pageSize;
	if (pPageSizeDesc->freePageCount){
		pPageSizeDesc->freePageCount--;
		uint64_t physicalAddress = *(pPageSizeDesc->pFreePageList+pPageSizeDesc->freePageCount);
		*pPhysicalAddress = physicalAddress;
		return 0;
	}
	struct pmm_page_data_info* pPageDataInfo = &pmmInfo.pageDataInfo;
	struct pmm_page_segment_desc* pPageSegmentDesc = pPageDataInfo->pPageSegmentDescList+pPageDataInfo->pageSegmentDescCommit-0x01;
	if (pPageSegmentDesc->pageDataCommit+dataSize>pPageSegmentDesc->pageDataReserve){
		pPageSegmentDesc++;
		pPageDataInfo->pageSegmentDescCommit++;
		if (pPageDataInfo->pageSegmentDescCommit>pPageDataInfo->pageSegmentDescReserve){
			printf("lack of contiguous volatile physical page reserve\r\n");
			return -1;
		}
	}
	uint64_t physicalAddress = (uint64_t)(pPageSegmentDesc->pPageData+pPageSegmentDesc->pageDataCommit);
	pPageSegmentDesc->pageDataCommit+=dataSize;
	*pPhysicalAddress = physicalAddress;
	return 0;
}
KAPI int physicalFree(uint64_t physicalAddress, uint64_t dataSize){
	if (!physicalAddress||!dataSize)
		return -1;
	struct pmm_page_size_desc* pPageSizeDesc = (struct pmm_page_size_desc*)0x00;
	if (physical_page_size_get_desc(dataSize, &pPageSizeDesc)!=0){
		printf("failed to get volatile physical page data QWORD size descriptor\r\n");
		return -1;
	}
	*(pPageSizeDesc->pFreePageList+pPageSizeDesc->freePageCount) = physicalAddress;
	pPageSizeDesc->freePageCount++;
	return 0;
}
int pmm_page_data_get_info(struct pmm_page_data_info** ppPhysicalPageDataInfo){
	if (!ppPhysicalPageDataInfo)
		return -1;
	*ppPhysicalPageDataInfo = &pmmInfo.pageDataInfo;
	return 0;
}
