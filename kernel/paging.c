#include <Uefi.h>
#include "graphics.h"
#include "stdlib.h"
#include "bootloader.h"
#include "paging.h"
uint64_t pt_cnt = 0;
uint64_t pd_cnt = 0;
uint64_t pdpt_cnt = 0;
uint64_t pml4_cnt = 0;
uint64_t pt_size = 0;
uint64_t* new_pml4 = (uint64_t*)0x0;
uint64_t* new_pdpt = (uint64_t*)0x0;
uint64_t* new_pd = (uint64_t*)0x0;
uint64_t* new_pt = (uint64_t*)0x0;
uint64_t last_pml4 = 0;
uint64_t last_pdpt = 0;
uint64_t last_pd = 0;
uint64_t last_pt = 0;
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
	pt_cnt = (freeMemory/(MEM_MB*2))+1;
	pd_cnt = (pt_cnt/512)+1;
	pdpt_cnt = (pd_cnt/512)+1;
	pml4_cnt = (pdpt_cnt/512)+1;
	pt_size= ((pml4_cnt*4096)+(pdpt_cnt*4096)+(pd_cnt*4096)+(pt_cnt*4096));
	unsigned int pt_found = 0;
	for (UINTN i = 0;i<memoryMapEntries;i++){
		pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((uint64_t)pFirstMemDesc)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiConventionalMemory)
			continue;
		if ((pMemDesc->NumberOfPages*EFI_PAGE_SIZE)<pt_size)
			continue;
		pMemDesc->NumberOfPages+=pt_size;
		pMemDesc->Type = EfiLoaderData;
		new_pml4 = (uint64_t*)pMemDesc->PhysicalStart;
		memset((uint64_t*)new_pml4, 0, pt_size);
		pt_found = 1;
		break;
	}
	if (!pt_found){
		printf(L"failed to find available memory for page tables\r\n");
		return -1;
	}
	void* old_pml4 = get_pt();
	new_pdpt = (uint64_t*)((uint64_t)new_pml4+(4096*pml4_cnt));
	new_pd = (uint64_t*)((uint64_t)new_pdpt+(4096*pdpt_cnt));
	new_pt = (uint64_t*)((uint64_t)new_pt+(4096*pt_cnt));
	printf(L"page tables size: %dkb\r\n", pt_size/MEM_KB);
	printf(L"%dmb of installed memory\r\n", systemMemory/MEM_MB);
	printf(L"%dmb of free memory\r\n", freeMemory/MEM_MB);
	printf(L"old page tables: %p\r\n", (void*)old_pml4);
	printf(L"new page tables: %p\r\n", (void*)new_pml4);
	for (unsigned int i = 0;i<systemMemory/4096;i++){
		map_pages(i*4096, i*4096, 7, 1);
	}
//	load_pt(new_pml4);
	return 0;
}
int map_pages(uint64_t va, uint64_t pa, uint32_t flags, uint64_t pagecnt){
	uint8_t pml4_index = (va>>39)&0x1FF;
	uint8_t pdpt_index = (va>>30)&0x1FF;
	uint8_t pd_index = (va>>21)&0x1FF;
	uint8_t pt_index = (va>>12)&0x1FF;
	uint16_t page_offset = va&0xFFF;
/*	printf(L"pml4 index: %d\r\n", pml4_index);
	printf(L"pdpt index: %d\r\n", pdpt_index);
	printf(L"pd index: %d\r\n", pd_index);
	printf(L"pt index: %d\r\n", pt_index);
	printf(L"page offset: %d\r\n", page_offset);
*/
	return 0;
}
