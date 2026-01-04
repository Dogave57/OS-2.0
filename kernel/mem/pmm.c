#include "stdlib/stdlib.h"
#include "drivers/gpu/framebuffer.h"
#include "cpu/cpuid.h"
#include "align.h"
#include "bootloader.h"
#include "panic.h"
#include "mem/pmm.h"
uint64_t totalMemory = 0;
uint64_t installedMemory = 0;
uint64_t freeMemory = 0;
struct p_pt_info* pt = (struct p_pt_info*)0x0;
uint64_t pt_size = 0;
uint64_t maxPhysicalAddress = 0;
int pmm_init(void){
	getTotalMemory(&totalMemory);
	getInstalledMemory(&installedMemory); 
	getFreeMemory(&freeMemory);
	if (allocatePageTable()!=0){
		printf("failed to allocate memory for physical page table\r\n");
		return -1;
	}
	if (initPageTable()!=0){
		printf("failed to intialize physical page table\r\n");
		return -1;
	}
	printf("installed memory + hardware memory: %dmb\r\n", totalMemory/MEM_MB);
	printf("installed memory: %dmb\r\n", installedMemory/MEM_MB);
	printf("free memory: %dmb\r\n", freeMemory/MEM_MB);
	uint32_t eax = 0;
	if (cpuid(0x80000008, 0, &eax, (uint32_t*)0x0, (uint32_t*)0x0, (uint32_t*)0x0)!=0)
		return -1;
	uint64_t paBits = (uint64_t)(eax&0xFF);
	maxPhysicalAddress = ((uint64_t)1<<paBits)-1;	
	return 0;
}
KAPI int getTotalMemory(uint64_t* pTotalMemory){
	if (!pTotalMemory)
		return -1;
	if (totalMemory){
		*pTotalMemory = totalMemory;
		return 0;
	}
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		totalMemory+=pMemDesc->NumberOfPages*EFI_PAGE_SIZE;
	}
	*pTotalMemory = totalMemory;
	return 0;
}
KAPI int getInstalledMemory(uint64_t* pInstalledMemory){
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
KAPI int getFreeMemory(uint64_t* pFreeMemory){
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
int allocatePageTable(void){
	if (!freeMemory)
		getFreeMemory(&freeMemory);
	uint64_t max_pages = totalMemory/PAGE_SIZE;
	uint64_t pt_size = (max_pages*(sizeof(struct p_page)+sizeof(struct p_page*)+sizeof(struct p_page*))+sizeof(struct p_pt_info));
	if (physicalAllocRaw((uint64_t*)&pt, pt_size)!=0){
		printf("failed to allocate page tables\r\n");
		return -1;
	}
	pt->pt_size = pt_size;
	pt->pPageEntries = (struct p_page*)(pt+1);
	pt->pFreeEntries = (struct p_page**)(pt->pPageEntries+max_pages);
	pt->pUsedEntries = (struct p_page**)(pt->pFreeEntries+max_pages);
	pt->freeEntryCnt = 0;
	pt->usedEntryCnt = 0;
	pt->maxFreeEntries = max_pages;
	pt->maxUsedEntries = max_pages;
	return 0;
}
int initPageTable(void){
	uint64_t max_entries = pt_size/sizeof(struct p_page);
	for (uint64_t i = 0;i<max_entries;i++){
		struct p_page* pentry = pt->pPageEntries+i;
		pentry->status = PAGE_RESERVED;
		pt->pFreeEntries[i] = (struct p_page*)0x0;
		pt->pUsedEntries[i] = (struct p_page*)0x0;
	}
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type==EfiMemoryMappedIO||pMemDesc->Type==EfiMemoryMappedIOPortSpace||pMemDesc->Type==EfiACPIReclaimMemory||pMemDesc->Type==EfiACPIMemoryNVS||pMemDesc->Type==EfiReservedMemoryType)
			continue;
		uint64_t page_index = pMemDesc->PhysicalStart/EFI_PAGE_SIZE;
		uint64_t pa = pMemDesc->PhysicalStart;
		for (UINTN page = 0;page<pMemDesc->NumberOfPages;page++,page_index++,pa+=PAGE_SIZE){
			struct p_page* pentry = pt->pPageEntries+page_index;
			if (pMemDesc->Type==EfiConventionalMemory&&pa!=maxPhysicalAddress){
				pentry->status = PAGE_FREE;
				pt->pFreeEntries[pt->freeEntryCnt] = pentry;
				pt->freeEntryCnt++;
			}
			else{
				pentry->status = PAGE_RESERVED;
			}
		}
	}
	return 0;
}
KAPI int physicalAllocPage(uint64_t* pPhysicalAddress, uint8_t pageType){
	if (!pPhysicalAddress)
		return -1;
	if (!pt->freeEntryCnt){
		printf("out of memory\r\n");
		return -1;	
	}
	struct p_page* pNewPage = pt->pFreeEntries[pt->freeEntryCnt-1];
	if (!pNewPage){
		printf("invalid free entry\r\n");
		return -1;
	}
	pNewPage->status = PAGE_INUSE;
	pNewPage->pageType = pageType;
	pt->pUsedEntries[pt->usedEntryCnt] = pNewPage;
	pt->freeEntryCnt--;
	pt->usedEntryCnt++;
	uint64_t pa = (uint64_t)(pNewPage-pt->pPageEntries);
	pa*=PAGE_SIZE;
	*pPhysicalAddress = pa;
	return 0;
}
KAPI int physicalFreePage(uint64_t physicalAddress){
	if (!pt->usedEntryCnt)
		return -1;
	uint64_t pageEntry = (physicalAddress/PAGE_SIZE);
	struct p_page* pentry = pt->pPageEntries+pageEntry;
	if (pentry->status==PAGE_FREE)
		return 0;
	pentry->status = PAGE_FREE;
	pt->pFreeEntries[pt->freeEntryCnt] = pentry;
	pt->freeEntryCnt++;
	pt->usedEntryCnt--;
	if (pt->freeEntryCnt>pt->maxFreeEntries){
		panic("too many free physical pages!\r\n");
	}
	return 0;
}
KAPI int physicalMapPage(uint64_t physicalAddress, uint64_t virtualAddress, uint8_t pageType){
	struct p_page* pNewPage = pt->pPageEntries+(physicalAddress/PAGE_SIZE);
	pNewPage->status = PAGE_INUSE;
	pNewPage->pageType = pageType;
	pNewPage->virtualAddress = align_down(virtualAddress, PAGE_SIZE);
	return 0;
}
KAPI int physicalUnmapPage(uint64_t physicalAddress){
	struct p_page* pPage = pt->pPageEntries+(physicalAddress/PAGE_SIZE);
	pPage->status = PAGE_FREE;
	return 0;
}
int physicalAllocRaw(uint64_t* pPhysicalAddress, uint64_t size){
	if (!pPhysicalAddress)
		return -1;
	size = align_up(size, PAGE_SIZE);
	uint64_t requestedPages = size/PAGE_SIZE;
	UINTN entryCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<entryCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type!=EfiConventionalMemory||!pMemDesc->PhysicalStart)
			continue;
		if (pMemDesc->NumberOfPages<requestedPages)
			continue;
		pMemDesc->PhysicalStart+=size;
		pMemDesc->NumberOfPages-=requestedPages;
		if (!pt){
			*pPhysicalAddress = (uint64_t)pMemDesc->PhysicalStart;
			return 0;
		}
		*pPhysicalAddress = (uint64_t)pMemDesc->PhysicalStart;
	}
	printf("failed to find %d page entries to fit block of size: %d\r\n", requestedPages, size);
	return -1;
}
int physicalFreeRaw(uint64_t physicalAddress, uint64_t size){
	UINTN entryCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<entryCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->PhysicalStart!=physicalAddress)
			continue;
		pMemDesc->Type = EfiConventionalMemory;
		return 0;
	}
	return -1;
}
int getPhysicalPageTable(uint64_t* pPa, uint64_t* pSize){
	if (!pPa||!pSize)
		return -1;
	*pPa = (uint64_t)pt;
	*pSize = pt->pt_size;
	return 0;
}
KAPI int getUsedPhysicalPages(uint64_t* pUsedPages){
	if (!pUsedPages)
		return -1;
	*pUsedPages = pt->usedEntryCnt;
	return 0;
}
KAPI int getFreePhysicalPages(uint64_t* pFreePages){
	if (!pFreePages)
		return -1;
	*pFreePages = pt->freeEntryCnt;
	return 0;
}
KAPI int physicalToVirtual(uint64_t pa, uint64_t* pVa){
	if (!pVa)
		return -1;
	uint64_t pageEntry = pa/PAGE_SIZE;
	struct p_page* pPageEntry = pt->pPageEntries+pageEntry;
	uint64_t page_offset = PAGE_SIZE-(pa%PAGE_SIZE);
	uint64_t va = pPageEntry->virtualAddress;
	if (pPageEntry->status==PAGE_FREE)
		return -1;
	*pVa = va;
	return 0;	
}
