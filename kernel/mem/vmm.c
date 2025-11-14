#include <stdint.h>
#include "stdlib/stdlib.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
uint64_t* pml4 = (uint64_t*)0x0;
uint64_t next_page_va = 0;
int vmm_init(void){
	uint64_t max_pages = installedMemory/PAGE_SIZE;
	if (physicalAllocPage((uint64_t*)&pml4, PAGE_TYPE_NORMAL)!=0){
		printf(L"failed to allocate page for pml4\r\n");
		return -1;
	}
	memset((void*)pml4, 0, PAGE_SIZE);
	if (virtualMapPage((uint64_t)pml4, (uint64_t)pml4, PTE_RW|PTE_PCD|PTE_PWT|PTE_NX, 0, 0, PAGE_TYPE_VMM)!=0){
		printf(L"failed to map pml4\r\n");
		return -1;
	}
	uint64_t fb_pages = (pbootargs->graphicsInfo.width*pbootargs->graphicsInfo.height*4)/PAGE_SIZE;
	uint64_t pFrameBuffer = (uint64_t)pbootargs->graphicsInfo.physicalFrameBuffer;
	uint64_t va = pFrameBuffer;
	if (virtualMapPages(pFrameBuffer, pFrameBuffer, PTE_RW|PTE_PCD|PTE_PWT|PTE_NX, fb_pages, 0, 0, PAGE_TYPE_MMIO)!=0){
		printf(L"failed to map framebuffer\r\n");
		return -1;
	}
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiConventionalMemory)
			continue;
		if (virtualMapPages((uint64_t)pMemDesc->PhysicalStart, (uint64_t)pMemDesc->PhysicalStart, PTE_RW, (uint64_t)pMemDesc->NumberOfPages, 1, 0, PAGE_TYPE_NORMAL)!=0){
			printf(L"failed to identity map %d pages at %p\r\n", pMemDesc->NumberOfPages, (void*)pMemDesc->PhysicalStart);
			return -1;
		}
	}
	uint64_t kernel_pages = pbootargs->kernelInfo.kernelSize/PAGE_SIZE;
	if (pbootargs->kernelInfo.kernelSize%PAGE_SIZE)
		kernel_pages;
	if (virtualMapPages(pbootargs->kernelInfo.pKernel, pbootargs->kernelInfo.pKernel, PTE_RW, kernel_pages, 1, 0, PAGE_TYPE_NORMAL)!=0){
		printf(L"failed to map kernel\r\n");
		return -1;
	}
	uint64_t kernelStackPages = pbootargs->kernelInfo.kernelStackSize/PAGE_SIZE;
	if (pbootargs->kernelInfo.kernelStackSize%PAGE_SIZE)
		kernelStackPages++;
	if (virtualMapPages(pbootargs->kernelInfo.pKernelStack, pbootargs->kernelInfo.pKernelStack, PTE_RW, kernelStackPages, 1, 0, PAGE_TYPE_NORMAL)!=0){
		printf(L"failed to map kernel stack\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)pbootargs, (uint64_t)pbootargs, PTE_RW, 1, 0, PAGE_TYPE_NORMAL)!=0){
		printf(L"failed to map boot args\r\n");
		return -1;
	}
	uint64_t fontPages = pbootargs->graphicsInfo.fontDataSize/PAGE_SIZE;
	if (pbootargs->graphicsInfo.fontDataSize%PAGE_SIZE)
		fontPages++;
	if (virtualMapPages((uint64_t)pbootargs->graphicsInfo.fontData, (uint64_t)pbootargs->graphicsInfo.fontData, PTE_RW, fontPages, 1, 0, PAGE_TYPE_NORMAL)!=0){
		printf(L"failed to map font\r\n");
		return -1;
	}
	uint64_t memoryMapPages = pbootargs->memoryInfo.memoryMapSize/PAGE_SIZE;
	if (pbootargs->memoryInfo.memoryMapSize%PAGE_SIZE)
		memoryMapPages++;
	if (virtualMapPages((uint64_t)pbootargs->memoryInfo.pMemoryMap, (uint64_t)pbootargs->memoryInfo.pMemoryMap, PTE_RW, memoryMapPages, 1, 0, PAGE_TYPE_FIRMWARE_DATA)!=0){
		printf(L"failed to map memory map\r\n");
		return -1;
	}
	uint64_t pPhysicalPageTable = (uint64_t)0;
	uint64_t physicalPageTableSize = (uint64_t)0;
	if (getPhysicalPageTable(&pPhysicalPageTable, &physicalPageTableSize)!=0){
		printf(L"failed to get physical page table\r\n");
		return -1;
	}
	uint64_t physicalPageTablePages = physicalPageTableSize/PAGE_SIZE;
	if (physicalPageTableSize%PAGE_SIZE)
		physicalPageTablePages++;
	if (virtualMapPages((uint64_t)pPhysicalPageTable, (uint64_t)pPhysicalPageTable, PTE_RW, physicalPageTablePages, 1, 0, PAGE_TYPE_PMM)!=0){
		printf(L"failed to map physical page table\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)pbootargs->driveInfo.devicePathStr, (uint64_t)pbootargs->driveInfo.devicePathStr, PTE_RW, 1, 0, PAGE_TYPE_FIRMWARE_DATA)!=0){
		printf(L"failed to map drive device path string\r\n");
		return -1;
	}
	load_pt((uint64_t)pml4);
	flush_full_tlb();
	virtualUnmapPage(0x0, 0);
	physicalMapPage(0x0, PAGE_TYPE_RESERVED);
	return 0;
}
int vmm_getPageTableEntry(uint64_t va, uint64_t** ppEntry){
	if (!ppEntry)
		return -1;
	uint64_t pml4_index = (va>>39)&0x1FF;
	uint64_t pdpt_index = (va>>30)&0x1FF;
	uint64_t pd_index = (va>>21)&0x1FF;
	uint64_t pt_index = (va>>12)&0x1FF;
	uint64_t* page_pdpt = (uint64_t*)0x0;
	uint64_t* page_pd = (uint64_t*)0x0;
	uint64_t* page_pt = (uint64_t*)0x0;
	if (vmm_getNextLevel(pml4, &page_pdpt, pml4_index)!=0)
		return -1;
	if (vmm_getNextLevel(page_pdpt, &page_pd, pdpt_index)!=0)
		return -1;
	if (vmm_getNextLevel(page_pd, &page_pt, pd_index)!=0)
		return -1;
	uint64_t* pentry = page_pt+pt_index;
	*ppEntry = pentry;
	return 0;
}
int vmm_getNextLevel(uint64_t* pCurrentLevel, uint64_t** ppNextLevel, uint64_t index){
	if (!pCurrentLevel||!ppNextLevel)
		return -1;
	uint64_t* pNextLevel = (uint64_t*)*(((uint64_t**)pCurrentLevel)+index);
	if (PTE_IS_PRESENT(pNextLevel)){
		*ppNextLevel = (uint64_t*)PTE_GET_ADDR(pNextLevel);
		return 0;
	}
	if (physicalAllocPage((uint64_t*)&pNextLevel, PAGE_TYPE_NORMAL)!=0){
		return -1;
	}
	memset((void*)pNextLevel, 0, PAGE_SIZE);
	*((uint64_t*)(pCurrentLevel)+index) = (uint64_t)(((uint64_t)(pNextLevel))|PTE_RW|PTE_PRESENT);
	*ppNextLevel = (uint64_t*)pNextLevel;
	return 0;
}
int virtualMapPage(uint64_t pa, uint64_t va, uint64_t flags, unsigned int shared, uint64_t map_flags, uint32_t pageType){
	uint64_t* pentry = (uint64_t*)0x0;
	if (vmm_getPageTableEntry(va, &pentry)!=0)
		return -1;
	uint64_t pt_entry = pa|flags|PTE_PRESENT;
	if (!shared&&PTE_IS_PRESENT(*pentry))
		return -1;
	if (physicalMapPage(pa, pageType)!=0){
		return -1;
	}
	*pentry = pt_entry;
	if (va+PAGE_SIZE>next_page_va)
		next_page_va = va+PAGE_SIZE;
	if (get_pt()!=(uint64_t)pml4)
		return 0;
	flush_tlb(va);
	return 0;
}
int virtualMapPages(uint64_t pa, uint64_t va, uint64_t flags, uint64_t page_cnt, unsigned int shared, uint64_t map_flags, uint32_t pageType){
	for (uint64_t i = 0;i<page_cnt;i++){
		uint64_t page_pa = pa+(i*PAGE_SIZE);
		uint64_t page_va = va+(i*PAGE_SIZE);
		if (virtualMapPage(page_pa, page_va, flags, shared, map_flags, pageType)!=0)
			return -1;
	}
	return 0;
}
int virtualUnmapPage(uint64_t va, uint64_t map_flags){
	uint64_t* pentry = (uint64_t*)0x0;
	if (vmm_getPageTableEntry(va, &pentry)!=0){
		return -1;
	}
	if (!PTE_IS_PRESENT(*pentry)){
		printf(L"PTE not present\r\n");
		return -1;
	}
	uint64_t pa = PTE_GET_ADDR(*pentry);
	if (physicalUnmapPage(pa)!=0)
		return -1;
	*pentry = 0;
	if (get_pt()!=(uint64_t)pml4)
		return 0;
	flush_tlb(va);
	return 0;
}
int virtualUnmapPages(uint64_t va, uint64_t page_cnt, uint64_t map_flags){
	for (uint64_t i = 0;i<page_cnt;i++){
		uint64_t page_va = va+(i*PAGE_SIZE);
		if (virtualUnmapPage(page_va, map_flags)!=0)
			return -1;
	}
	return 0;	
}
int virtualToPhysical(uint64_t va, uint64_t* pPa){
	if (!pPa)
		return -1;
	uint64_t* pentry = (uint64_t*)0x0;
	if (vmm_getPageTableEntry(va, &pentry)!=0)
		return -1;
	uint64_t pa = PTE_GET_ADDR(*pentry);
	uint64_t page_offset = PAGE_SIZE-(va%PAGE_SIZE);
	*pPa = pa+page_offset;
	return 0;
}
int virtualAllocPage(uint64_t* pVa, uint64_t flags, uint64_t map_flags, uint32_t pageType){
	if (!pVa)
		return -1;
	uint64_t pa = 0;
	if (physicalAllocPage(&pa, PAGE_TYPE_NORMAL)!=0){
		return -1;
	}
	if (virtualMapPage(pa, pa, flags, 1, map_flags, pageType)!=0){
		physicalFreePage(pa);
		return -1;
	}
	*pVa = pa;
	return 0;
}
int virtualFreePage(uint64_t va, uint64_t map_flags){
	uint64_t* pentry = (uint64_t*)0x0;
	if (vmm_getPageTableEntry(va, &pentry)!=0){
		printf(L"failed to get PTE\r\n");
		return -1;
	}
	uint64_t pa = PTE_GET_ADDR(*pentry);
	if (physicalFreePage(pa)!=0){
		printf(L"failed to free physical page\r\n");
		return -1;
	}
	if (virtualUnmapPage(va, 0)!=0){
		printf(L"failed to unmap page\r\n");
		return -1;
	}
	return 0;
}
int virtualAllocPages(uint64_t* pVa, uint64_t page_cnt, uint64_t flags, uint64_t map_flags, uint32_t pageType){
	if (!pVa)
		return -1;
	uint64_t va = next_page_va;
	for (uint64_t i = 0;i<page_cnt;i++){
		uint64_t new_ppage = (uint64_t)0;
		uint64_t page_va = va+(i*PAGE_SIZE);
		if (physicalAllocPage(&new_ppage, PAGE_TYPE_NORMAL)!=0)
			return -1;
		if (virtualMapPage(new_ppage, page_va, flags, 1, map_flags, pageType)!=0)
			return -1;
	}
	*pVa = va;
	return 0;
}
int virtualFreePages(uint64_t va, uint64_t pagecnt){
	for (uint64_t i = 0;i<pagecnt;i++){
		uint64_t page_va = va+(i*PAGE_SIZE);
		if (virtualFreePage(page_va, 0)!=0){
			printf(L"failed to free page at va: %p\r\n", (void*)page_va);
			return -1;
		}
	}
	return 0;
}
