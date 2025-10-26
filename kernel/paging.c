#include <Uefi.h>
#include "graphics.h"
#include "stdlib.h"
#include "bootloader.h"
#include "paging.h"
int paging_init(void){
	clear();
	EFI_MEMORY_DESCRIPTOR* pFirstMemDesc = pbootargs->memoryInfo.pMemoryMap;
	EFI_MEMORY_DESCRIPTOR* pMemDesc = pFirstMemDesc;
	UINTN memoryMapEntries = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	uint64_t freeMemory = 0;
	for (UINTN i = 0;i<memoryMapEntries;i++){
		pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((uint64_t)pFirstMemDesc)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiConventionalMemory){
			continue;
		}
//		printf(L"addr: %p, pages: %d, type: %d\r\n", pMemDesc->PhysicalStart, pMemDesc->NumberOfPages, pMemDesc->Type);
		freeMemory+=pMemDesc->NumberOfPages*EFI_PAGE_SIZE;
	}
	printf(L"%dmb of free memory\r\n", freeMemory/1000000);
	load_pt(NULL);
	return 0;
}
