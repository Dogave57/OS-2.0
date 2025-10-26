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
	uint64_t systemMemory = 0;
	uint64_t pageTables_size = 0;
	for (UINTN i = 0;i<memoryMapEntries;i++){
		pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((uint64_t)pFirstMemDesc)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiReservedMemoryType&&pMemDesc->Type!=EfiACPIReclaimMemory&&pMemDesc->Type!=EfiACPIMemoryNVS&&pMemDesc->Type!=EfiMemoryMappedIO&&pMemDesc->Type!=EfiMemoryMappedIOPortSpace){
			systemMemory+=pMemDesc->NumberOfPages*EFI_PAGE_SIZE;
		}
		if (pMemDesc->Type!=EfiConventionalMemory){
			continue;
		}
		freeMemory+=pMemDesc->NumberOfPages*EFI_PAGE_SIZE;
	}
	uint64_t pt_cnt = (freeMemory/(MEM_MB*2))+1;
	uint64_t pd_cnt = (pt_cnt/512)+1;
	uint64_t pdpt_cnt = (pd_cnt/512)+1;
	uint64_t pml4_cnt = (pdpt_cnt/512)+1;
	uint64_t pt_size= (pml4_cnt*4096)+(pdpt_cnt*4096)+(pd_cnt*4096)+(pt_cnt*4096);
	printf(L"pml4: %d\r\n", pml4_cnt);
	printf(L"page directory list: %d\r\n", pdpt_cnt);
	printf(L"page directory cnt: %d\r\n", pd_cnt);
	printf(L"page table cnt: %d\r\n", pt_cnt);
	unsigned char* new_pt = (unsigned char*)0x0;
	unsigned int pt_found = 0;
	for (UINTN i = 0;i<memoryMapEntries;i++){
		pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((uint64_t)pFirstMemDesc)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiConventionalMemory)
			continue;
		if ((pMemDesc->NumberOfPages*EFI_PAGE_SIZE)<pt_size)
			continue;
		pMemDesc->NumberOfPages+=pt_size;
		pMemDesc->Type = EfiLoaderData;
		new_pt = (unsigned char*)pMemDesc->PhysicalStart;
		pt_found = 1;
		break;
	}
	if (!pt_found){
		printf(L"failed to find available memory for page tables\r\n");
		return -1;
	}
	void* old_pt = get_pt();
	printf(L"page tables size: %dkb\r\n", pt_size/MEM_KB);
	printf(L"%dmb of installed memory\r\n", systemMemory/MEM_MB);
	printf(L"%dmb of free memory\r\n", freeMemory/MEM_MB);
	printf(L"old page tables: %p\r\n", (void*)old_pt);
	printf(L"new page tables: %p\r\n", (void*)new_pt);
	load_pt(old_pt);
	return 0;
}
