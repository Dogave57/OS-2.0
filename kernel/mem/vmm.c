#include <stdint.h>
#include "stdlib/stdlib.h"
#include "align.h"
#include "panic.h"
#include "cpu/mutex.h"
#include "drivers/serial.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
struct vmm_info vmmInfo = {0};
int vmm_init(void){
	uint64_t installedMemory = 0x00;
	if (pmm_get_installed_memory(&installedMemory)!=0){
		printf("failed to get installed physical volatile memory\r\n");
		return -1;
	}
	struct pmm_page_data_info* pPhysicalPageDataInfo = (struct pmm_page_data_info*)0x00;
	if (pmm_page_data_get_info(&pPhysicalPageDataInfo)!=0){
		printf("failed to get physical volatile memory page data info descriptor\r\n");
		return -1;
	}
	if (physicalAlloc((uint64_t*)&vmmInfo.pml4List, PAGE_SIZE)!=0){
		printf("failed to allocate physical volatile memory PML4 PTE QWORD list physical page\r\n");
		return -1;
	}
	memset_64((void*)vmmInfo.pml4List, 0x00, PAGE_SIZE);
	vmmInfo.pNextVirtualPage = VMM_RANGE_BASE;
	for (uint64_t i = 0x00;i<pPhysicalPageDataInfo->pageSegmentDescReserve;i++){
		struct pmm_page_segment_desc* pPageSegmentDesc = pPhysicalPageDataInfo->pPageSegmentDescList+i;
		if (virtualMapPages((uint64_t)pPageSegmentDesc->pPageData, (uint64_t)pPageSegmentDesc->pPageData, PTE_RW, align_up(pPageSegmentDesc->pageDataReserve, PAGE_SIZE)/PAGE_SIZE, 0x01, 0x00)!=0){
			printf("failed to map contiguous volatile physical memory segment\r\n");
			return -1;
		}
	}
	uint64_t max_pages = installedMemory/PAGE_SIZE;
	uint64_t fb_pages = (pbootargs->graphicsInfo.width*pbootargs->graphicsInfo.height*4)/PAGE_SIZE;
	uint64_t fb_size = fb_pages*PAGE_SIZE;
	uint64_t pPhysicalFrameBuffer = (uint64_t)pbootargs->graphicsInfo.physicalFrameBuffer;
	uint64_t pFrameBuffer_va = pPhysicalFrameBuffer;
	if (virtualMapPages(pPhysicalFrameBuffer, pPhysicalFrameBuffer, PTE_RW|PTE_PCD|PTE_PWT|PTE_NX, fb_pages, 0x01, 0x00)!=0){
		printf("failed to map framebuffer\r\n");
		return -1;
	}
	uint64_t kernelPageCount = align_up(pbootargs->kernelInfo.kernelSize, PAGE_SIZE)/PAGE_SIZE;
	if (virtualMapPages((uint64_t)pbootargs->kernelInfo.pKernel, (uint64_t)pbootargs->kernelInfo.pKernel, PTE_RW, kernelPageCount, 0x01, 0x00)!=0){
		printf("failed to map base kernel physical volatile memory physical pages\r\n");
		return -1;
	}
	uint64_t fileBufferPageCount = align_up(pbootargs->kernelInfo.kernelFileDataSize, PAGE_SIZE)/PAGE_SIZE;
	if (virtualMapPages(pbootargs->kernelInfo.pKernelFileData, pbootargs->kernelInfo.pKernelFileData, PTE_RW|PTE_NX, fileBufferPageCount, 0x01, 0x00)!=0){
		printf("failed to map kernel file buffer\r\n");
		return -1;
	}
	uint64_t kernelStackPageCount = align_up(pbootargs->kernelInfo.kernelStackSize, PAGE_SIZE)/PAGE_SIZE;
	if (virtualMapPages(pbootargs->kernelInfo.pKernelStack, pbootargs->kernelInfo.pKernelStack, PTE_RW|PTE_NX, kernelStackPageCount, 0x01, 0x00)!=0){
		printf("failed to map kernel stack\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)pbootargs, (uint64_t)pbootargs, PTE_RW|PTE_NX, 0x01, 0x00)!=0x00){
		printf("failed to map boot args\r\n");
		return -1;
	}
	uint64_t fontPages = align_up(pbootargs->graphicsInfo.fontDataSize, PAGE_SIZE)/PAGE_SIZE;
	if (virtualMapPages((uint64_t)pbootargs->graphicsInfo.fontData, (uint64_t)pbootargs->graphicsInfo.fontData, PTE_RW|PTE_NX, fontPages, 0x01, 0x00)!=0){
		printf("failed to map font\r\n");
		return -1;
	}
	uint64_t memoryMapPages = align_up(pbootargs->memoryInfo.memoryMapSize, PAGE_SIZE)/PAGE_SIZE;
	if (virtualMapPages((uint64_t)pbootargs->memoryInfo.pMemoryMap, (uint64_t)pbootargs->memoryInfo.pMemoryMap, PTE_RW|PTE_NX, memoryMapPages, 0x01, 0x00)!=0){
		printf("failed to map memory map\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)pbootargs->driveInfo.devicePathStr, (uint64_t)pbootargs->driveInfo.devicePathStr, PTE_RW|PTE_NX, 0x01, 0x00)!=0){
		printf("failed to map drive device path string\r\n");
		return -1;
	}
	pbootargs->graphicsInfo.virtualFrameBuffer = (struct uvec4_8*)pPhysicalFrameBuffer;
	load_pml4((uint64_t)vmmInfo.pml4List);
	virtualUnmapPage(0x00, 0x00);
	return 0;
}
int vmm_getPageTableEntry(uint64_t virtualAddress, uint64_t** ppPageEntry){
	if (!ppPageEntry)
		return -1;
	uint64_t pml4Index = (virtualAddress>>39)&0x1FF;
	uint64_t pdptIndex = (virtualAddress>>30)&0x1FF;
	uint64_t pdIndex = (virtualAddress>>21)&0x1FF;
	uint64_t ptIndex = (virtualAddress>>12)&0x1FF;
	uint64_t* pdptList = (uint64_t*)0x00;
	uint64_t* pdList = (uint64_t*)0x00;
	uint64_t* ptList = (uint64_t*)0x00;
	if (vmm_getNextLevel(vmmInfo.pml4List, &pdptList, pml4Index)!=0){
		printf("failed to get virtual volatile memory PML4 QWORD PTE list physical page entry\r\n");
		return -1;
	}
	if (vmm_getNextLevel(pdptList, &pdList, pdptIndex)!=0){
		printf("failed to get virtual volatile memory PDPT QWORD PTE list physical page entry\r\n");
		return -1;
	}
	if (vmm_getNextLevel(pdList, &ptList, pdIndex)!=0){
		printf("failed to get virtual volatile memory PD QWORD PTE list physical page entry\r\n");
		return -1;
	}
	uint64_t* pPageEntry = ptList+ptIndex;
	*ppPageEntry = pPageEntry;
	return 0;
}
int vmm_getNextLevel(uint64_t* pCurrentLevel, uint64_t** ppNextLevel, uint64_t nextLevelIndex){
	if (!pCurrentLevel||!ppNextLevel)
		return -1;
	uint64_t* pPageEntry = pCurrentLevel+nextLevelIndex;
	uint64_t pageEntry = *pPageEntry;
	if (PTE_IS_PRESENT(pageEntry)){
		uint64_t* pNextLevel = (uint64_t*)PTE_GET_ADDRESS(pageEntry);
		*ppNextLevel = pNextLevel;
		return 0;
	}
	uint64_t* pNextLevel = (uint64_t*)0x00;
	if (physicalAlloc((uint64_t*)&pNextLevel, PAGE_SIZE)!=0){
		printf("failed to allocate virtual volatile memory QWORD PTE list physical page\r\n");
		return -1;
	}
	memset((void*)pNextLevel, 0x00, PAGE_SIZE);
	*pPageEntry = ((uint64_t)pNextLevel)|PTE_PRESENT|PTE_RW;
	*ppNextLevel = pNextLevel;
	return 0;
}
KAPI int virtualMapPage(uint64_t physicalAddress, uint64_t virtualAddress, uint64_t flags, uint8_t shared, uint64_t mapFlags){
	if (!VA_IS_CANONICAL(virtualAddress)){
		printf("non-canonical virtual address QWORD\r\n");
		return -1;
	}
	if (virtualAddress>=VA_MAX){
		printf("max virtual address\r\n");
		return -1;
	}
	if (virtualAddress%PAGE_SIZE||physicalAddress%PAGE_SIZE){
		physicalAddress = align_down(physicalAddress, PAGE_SIZE);
		virtualAddress = align_down(virtualAddress, PAGE_SIZE);
		if (virtualMapPage(physicalAddress+PAGE_SIZE, virtualAddress+PAGE_SIZE, flags, shared, mapFlags)!=0){
			return -1;
		}
	}
	uint64_t* pPageEntry = (uint64_t*)0x00;
	if (vmm_getPageTableEntry(virtualAddress, &pPageEntry)!=0){
		printf("failed to get volatile physical page entry descriptor\r\n");
		return -1;
	}
	uint64_t pageEntry = *pPageEntry;
	if (!(mapFlags&MAP_FLAG_LAZY)&&!(flags&PTE_PRESENT)){
		flags|=PTE_PRESENT;
	}
	if (mapFlags&MAP_FLAG_LAZY){
		flags|=PTE_LAZY;
	}
	if (!shared&&PTE_IS_MAPPED(pageEntry)){
		return -1;
	}
	if (virtualAddress>=vmmInfo.pNextVirtualPage)
		vmmInfo.pNextVirtualPage = virtualAddress+PAGE_SIZE;
	pageEntry = physicalAddress|flags;
	*pPageEntry = pageEntry;
	if (get_pml4()!=(uint64_t)vmmInfo.pml4List)
		return 0;
	flush_tlb(virtualAddress);
	return 0;
}
KAPI int virtualMapPages(uint64_t physicalAddress, uint64_t virtualAddress, uint64_t flags, uint64_t pageCount, uint8_t shared, uint64_t mapFlags){
	for (uint64_t i = 0x00;i<pageCount;i++){
		uint64_t pPageDataPhysical = physicalAddress+(i*PAGE_SIZE);
		uint64_t pPageData = virtualAddress+(i*PAGE_SIZE);
		if (virtualMapPage(pPageDataPhysical, pPageData, flags, shared, mapFlags)!=0){
			printf("failed to map physical volatile memory physical page\r\n");
			return -1;
		}
	}
	return 0;
}
KAPI int virtualUnmapPage(uint64_t virtualAddress, uint64_t mapFlags){
	virtualAddress = (virtualAddress%PAGE_SIZE) ? align_down(virtualAddress, PAGE_SIZE) : virtualAddress;
	uint64_t* pPageEntry = (uint64_t*)0x00;
	if (vmm_getPageTableEntry(virtualAddress, (uint64_t**)&pPageEntry)!=0){
		printf("failed to get volatile physical page entry descriptor\r\n");
		return -1;
	}
	uint64_t pageEntry = *pPageEntry;
	if (!PTE_IS_MAPPED(pageEntry)){
		return -1;
	}
	*pPageEntry = 0x00;
	if (get_pml4()!=(uint64_t)vmmInfo.pml4List)
		return 0;
	flush_tlb(virtualAddress);
	return 0;
}
KAPI int virtualUnmapPages(uint64_t virtualAddress, uint64_t pageCount, uint64_t mapFlags){
	for (uint64_t i = 0x00;i<pageCount;i++){
		uint64_t pPageData = virtualAddress+(i*PAGE_SIZE);
		if (virtualUnmapPage(pPageData, mapFlags)!=0){
			printf("failed to unmap virtual volatile memory virtual page\r\n");
			return -1;
		}
	}
	return 0;	
}
KAPI int virtualToPhysical(uint64_t pPageData, uint64_t* pPhysicalAddress){
	if (!pPhysicalAddress)
		return -1;
	uint64_t* pPageEntry = (uint64_t*)0x00;
	if (vmm_getPageTableEntry(pPageData, &pPageEntry)!=0){
		printf("failed to get volatile physical page QWORD entry descriptor\r\n");
		return -1;
	}
	uint64_t pageEntry = *pPageEntry;
	uint64_t physicalAddress = PTE_GET_ADDRESS(pageEntry);
	uint64_t pageOffset = pPageData-align_down(pPageData, PAGE_SIZE);
	physicalAddress+=pageOffset;
	*pPhysicalAddress = physicalAddress;
	return 0;
}
KAPI int virtualAllocPage(uint64_t* ppPageData, uint64_t flags, uint64_t mapFlags){
	if (!ppPageData)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t physicalAddress = 0x00;
	if (!(mapFlags&MAP_FLAG_LAZY)){
		if (physicalAlloc(&physicalAddress, PAGE_SIZE)!=0){
			printf("failed to allocate volatile physical page\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
	}
	uint64_t pPageData = 0x00;
	if (virtualGetSpace(&pPageData, PAGE_SIZE)!=0){
		if (physicalAddress)
			physicalFree(physicalAddress, PAGE_SIZE);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualMapPage(physicalAddress, pPageData, flags, 0x01, mapFlags)!=0){
		if (physicalAddress)
			physicalFree(physicalAddress, PAGE_SIZE);
		mutex_unlock(&mutex);
		return -1;
	}
	*ppPageData = pPageData;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int virtualFreePage(uint64_t pPageData, uint64_t map_flags){
	uint64_t* pPageEntry = (uint64_t*)0x00;
	if (vmm_getPageTableEntry(pPageData, &pPageEntry)!=0){
		printf("failed to get QWORD page table entry descriptor\r\n");
		return -1;
	}
	uint64_t pageEntry = *pPageEntry;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t physicalAddress = PTE_GET_ADDRESS(pageEntry);
	if (!PTE_IS_LAZY(pageEntry)&&physicalFree(physicalAddress, PAGE_SIZE)!=0){
		printf("failed to free volatile physical page data\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualUnmapPage(pPageData, 0x00)!=0){
		printf("failed to unmap volatile physical page\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int virtualAllocPages(uint64_t* ppPageData, uint64_t pageCount, uint64_t flags, uint64_t mapFlags){
	if (!ppPageData||!pageCount)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pPageList = (unsigned char*)0x00;
	uint64_t pageListSize = pageCount*PAGE_SIZE;
	if (virtualGetSpace((uint64_t*)&pPageList, pageListSize)!=0){
		printf("failed to get virtual addressing space for volatile physical page list\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	for (uint64_t i = 0X00;i<pageCount;i++){
		uint64_t physicalPageAddress = 0x00;
		if (!(mapFlags&MAP_FLAG_LAZY)){
			if (physicalAlloc(&physicalPageAddress, PAGE_SIZE)!=0){
				printf("failed to allocate volatile physical page\r\n");
				mutex_unlock(&mutex);
				return -1;
			}
		}
		unsigned char* pPage = pPageList+(PAGE_SIZE*i);
		if (virtualMapPage(physicalPageAddress, (uint64_t)pPage, flags, 0x01, mapFlags)!=0){
			mutex_unlock(&mutex);	
			return -1;
		}
	}
	*ppPageData = (uint64_t)pPageList;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int virtualFreePages(uint64_t pPageList, uint64_t pageCount){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	for (uint64_t i = 0x00;i<pageCount;i++){
		uint64_t pPage = pPageList+(i*PAGE_SIZE);
		if (virtualFreePage(pPage, 0x00)!=0){
			printf("failed to free volatile physical page\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int virtualAlloc(uint64_t* ppPageData, uint64_t pageDataSize, uint64_t flags, uint64_t mapFlags){
	return virtualAllocPages(ppPageData, align_up(pageDataSize, PAGE_SIZE)/PAGE_SIZE, flags, mapFlags);
}
KAPI int virtualFree(uint64_t pPageData, uint64_t pageDataSize){
	return virtualFreePages(pPageData, align_up(pageDataSize, PAGE_SIZE)/PAGE_SIZE);
}
KAPI int virtualProtectPage(uint64_t pPageData, uint64_t protectionFlags){
	uint64_t* pPageEntry = (uint64_t*)0x00;
	if (vmm_getPageTableEntry(pPageData, &pPageEntry)!=0){
		printf("failed to get QWORD page table entry descriptor\r\n");
		return -1;
	}
	uint64_t pageEntry = *pPageEntry;
	if (!protectionFlags||protectionFlags==~PTE_PRESENT){
		*pPageEntry = pageEntry|~PTE_PRESENT;
		return 0;
	}
	protectionFlags = (protectionFlags&~PTE_PRESENT) ? protectionFlags|PTE_PRESENT : protectionFlags;
	*pPageEntry = pPageData|protectionFlags;
	return 0;
}
KAPI int virtualProtectPages(uint64_t pPageData, uint64_t pageCount, uint64_t protectionFlags){
	for (uint64_t i = 0;i<pageCount;i++,pPageData+=PAGE_SIZE){
		if (virtualProtectPage(pPageData, protectionFlags)!=0){
			printf("failed to protect physical volatile page\r\n");
			return -1;
		}
	}
	return 0;
}
KAPI int virtualGetPageFlags(uint64_t pPageData, uint64_t* pFlags){
	if (!pPageData||!pFlags)
		return -1;
	uint64_t* pPageEntry = (uint64_t*)0x00;
	if (vmm_getPageTableEntry(pPageData, &pPageEntry)!=0){
		printf("failed to get QWORD page table entry descriptor\r\n");
		return -1;
	}
	uint64_t pageEntry = *pPageEntry;
	*pFlags = PTE_GET_FLAGS(pageEntry);
	return 0;
}
KAPI int virtualGetSpace(uint64_t* ppPageData, uint64_t pageDataSize){
	if (!ppPageData)
		return -1;
	uint64_t pPageData = vmmInfo.pNextVirtualPage;
	*ppPageData = pPageData;
	return 0;
}
KAPI int pt_loaded(void){
	return (get_pml4()==(uint64_t)vmmInfo.pml4List) ? 0x00 : -1;
}
