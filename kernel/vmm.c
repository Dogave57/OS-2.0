#include <stdint.h>
#include "stdlib.h"
#include "pmm.h"
#include "vmm.h"
uint64_t* pml4 = (uint64_t*)0x0;
int vmm_init(void){
	if (physicalAllocPage((uint64_t*)&pml4)!=0){
		printf(L"failed to allocate page for pml4\r\n");
		return -1;
	}
	memset((void*)pml4, 0, PAGE_SIZE);
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	uint64_t bytes_mapped = 0;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(pbootargs->memoryInfo.memoryDescSize*i));
		if (pMemDesc->Type==EfiReservedMemoryType)
			continue;
		for (UINTN page = 0;page<pMemDesc->NumberOfPages;page++){
			uint64_t pa = pMemDesc->PhysicalStart+(page*PAGE_SIZE);
			if (virtualMapPage(pa, &pa, PTE_RW, 0)!=0){
				printf(L"failed to map page at pa: %p\r\n", (void*)pa);
				return -1;
			}
		}
		bytes_mapped+=pMemDesc->NumberOfPages*PAGE_SIZE;
	}
	uint64_t fb_pages = (pbootargs->graphicsInfo.width*pbootargs->graphicsInfo.height*4)/PAGE_SIZE;
	uint64_t pFrameBuffer = (uint64_t)pbootargs->graphicsInfo.physicalFrameBuffer;
	uint64_t va = pFrameBuffer;
	if (virtualMapPages(pFrameBuffer, &pFrameBuffer, PTE_RW|PTE_PCD|PTE_PWT|PTE_NX, fb_pages, 0)!=0){
		printf(L"failed to map framebuffer\r\n");
		return -1;
	}
	load_pt((uint64_t)pml4);
	return 0;
}
int vmm_getNextLevel(uint64_t* pCurrentLevel, uint64_t** ppNextLevel, uint64_t index, uint64_t flags){
	if (!pCurrentLevel||!ppNextLevel)
		return -1;
	uint64_t* pNextLevel = (uint64_t*)*(((uint64_t**)pCurrentLevel)+index);
	if (((uint64_t)pNextLevel)&PTE_PRESENT){
		*ppNextLevel = (uint64_t*)PTE_GET_ADDR(pNextLevel);
		return 0;
	}
	if (physicalAllocPage((uint64_t*)&pNextLevel)!=0){
		return -1;
	}
	memset((void*)pNextLevel, 0, PAGE_SIZE);
	pNextLevel = (uint64_t*)((uint64_t)(pNextLevel)|flags|PTE_PRESENT);
	*((uint64_t*)(pCurrentLevel)+index) = (uint64_t)pNextLevel;
	*ppNextLevel = (uint64_t*)PTE_GET_ADDR(pNextLevel);
	return 0;
}
int virtualMapPage(uint64_t pa, uint64_t* pVa, uint64_t flags, unsigned int alloc_new){
	if (!pVa)
		return -1;
	uint64_t va = *pVa;
	uint64_t pml4_index = (va>>39)&0x1FF;
	uint64_t pdpt_index = (va>>30)&0x1FF;
	uint64_t pd_index = (va>>21)&0x1FF;
	uint64_t pt_index = (va>>12)&0x1FF;
	uint64_t* page_pdpt = (uint64_t*)0x0;
	uint64_t* page_pd = (uint64_t*)0x0;
	uint64_t* page_pt = (uint64_t*)0x0;
	if (vmm_getNextLevel(pml4, (uint64_t**)&page_pdpt, pml4_index, PTE_RW)!=0){
		return -1;
	}
	if (vmm_getNextLevel(page_pdpt, (uint64_t**)&page_pd, pdpt_index, PTE_RW)!=0){
		return -1;
	}
	if (vmm_getNextLevel(page_pd, (uint64_t**)&page_pt, pd_index, PTE_RW)!=0){
		return -1;
	}
	uint64_t pt_entry = pa|flags|PTE_PRESENT;
	page_pt[pt_index] = pt_entry;
	return 0;
}
int virtualMapPages(uint64_t pa, uint64_t* pVa, uint64_t flags, uint64_t page_cnt, unsigned int alloc_new){
	if (!pVa)
		return -1;
	uint64_t va = *pVa;
	for (uint64_t i = 0;i<page_cnt;i++){
		uint64_t page_pa = pa+(i*PAGE_SIZE);
		uint64_t page_va = va+(i*PAGE_SIZE);
		if (virtualMapPage(page_pa, &page_va, flags, alloc_new)!=0)
			return -1;
	}
	return 0;
}
int virtualUnmapPage(uint64_t va){
	uint64_t pml4_index = (va>>39)&0x1FF;
	uint64_t pdpt_index = (va>>30)&0x1FF;
	uint64_t pd_index = (va>>21)&0x1FF;
	uint64_t pt_index = (va>>12)&0x1FF;
	uint64_t** page_pdpt = (uint64_t**)0x0;
	uint64_t** page_pd = (uint64_t**)0x0;
	uint64_t** page_pt = (uint64_t**)0x0;
	return 0;
}
int virtualAllocPage(uint64_t pa, uint64_t* pVa){
	if (!pVa)
		return -1;
	return 0;
}
int virtualFreePage(uint64_t va){

	return 0;
}
