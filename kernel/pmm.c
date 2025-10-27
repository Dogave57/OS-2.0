#include "stdlib.h"
#include "graphics.h"
#include "bootloader.h"
#include "pmm.h"
uint64_t installedMemory = 0;
uint64_t freeMemory = 0;
struct page* pageTable = (struct page*)0x0;
uint64_t pageTableSize = 0;
int pmm_init(void){
	getInstalledMemory(&installedMemory);
	getFreeMemory(&freeMemory);
	if (allocatePageTable(&pageTable, &pageTableSize)!=0){
		printf(L"failed to allocate memory for physical page table\r\n");
		return -1;
	}
	printf(L"installed memory: %dmb\r\n", installedMemory/MEM_MB);
	printf(L"free memory: %dmb\r\n", freeMemory/MEM_MB);
	printf(L"page table: %p\r\n", (void*)pageTable);
	printf(L"page table size: %dmb\r\n", pageTableSize/MEM_MB);
	return 0;
}
int getInstalledMemory(uint64_t* pInstalledMemory){
	if (!pInstalledMemory)
		return -1;
	if (installedMemory){
		*pInstalledMemory = installedMemory;
		return 0;
	}
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(pbootargs->memoryInfo.memoryDescSize*i));
		if (pMemDesc->Type==EfiReservedMemoryType||pMemDesc->Type==EfiMemoryMappedIO||pMemDesc->Type==EfiMemoryMappedIOPortSpace||pMemDesc->Type==EfiACPIMemoryNVS||pMemDesc->Type==EfiACPIReclaimMemory)
			continue;
		installedMemory+=(pMemDesc->NumberOfPages*EFI_PAGE_SIZE);
	}
	*pInstalledMemory = installedMemory;
	return 0;
}
int getFreeMemory(uint64_t* pFreeMemory){
	if (!pFreeMemory)
		return -1;
	if (freeMemory){
		*pFreeMemory = freeMemory;
		return 0;
	}
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiConventionalMemory)
			continue;
		freeMemory+=pMemDesc->NumberOfPages*EFI_PAGE_SIZE;
	}
	*pFreeMemory = freeMemory;
	return 0;
}
int allocatePageTable(struct page** ppPageTable, uint64_t* pPageTableSize){
	if (!ppPageTable||!pPageTableSize)
		return -1;
	if (!freeMemory)
		getFreeMemory(&freeMemory);
	uint64_t pt_size = (freeMemory/PAGE_SIZE)*sizeof(struct page);
	struct page* pt = (struct page*)0x0;
	unsigned int pt_found = 0;
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));	
		if (pMemDesc->Type!=EfiConventionalMemory)
			continue;
		uint64_t size = pMemDesc->NumberOfPages*EFI_PAGE_SIZE;
		if (size<pt_size)
			continue;
		pMemDesc->Type = EfiLoaderData;
		pMemDesc->NumberOfPages-=pt_size/PAGE_SIZE;
		pt = (struct page*)pMemDesc->PhysicalStart;
		pt_found = 1;
		break;
	}
	if (!pt_found)
		return -1;
	*ppPageTable = pt;
	*pPageTableSize = pt_size;
	freeMemory-=pt_size;
	return 0;
}
