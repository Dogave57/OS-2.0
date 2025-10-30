#include "stdlib.h"
#include "graphics.h"
#include "align.h"
#include "bootloader.h"
#include "pmm.h"
uint64_t totalMemory = 0;
uint64_t installedMemory = 0;
uint64_t freeMemory = 0;
struct p_pt_info* pt = (struct p_pt_info*)0x0;
uint64_t pt_size = 0;
int pmm_init(void){
	getTotalMemory(&totalMemory);
	getInstalledMemory(&installedMemory); 
	getFreeMemory(&freeMemory);
	if (allocatePageTable()!=0){
		printf(L"failed to allocate memory for physical page table\r\n");
		return -1;
	}
	if (initPageTable()!=0){
		printf(L"failed to intialize physical page table\r\n");
		return -1;
	}
	printf(L"intsalled memory + hardware memorey: %dmb\r\n", totalMemory/MEM_MB);
	printf(L"installed memory: %dmb\r\n", installedMemory/MEM_MB);
	printf(L"free memory: %dmb\r\n", freeMemory/MEM_MB);
	return 0;
}
int getTotalMemory(uint64_t* pTotalMemory){
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
int allocatePageTable(void){
	if (!freeMemory)
		getFreeMemory(&freeMemory);
	uint64_t max_pages = installedMemory/PAGE_SIZE;
	pt_size = (max_pages*(sizeof(struct p_page)+sizeof(struct p_page*)+sizeof(struct p_page*))+sizeof(struct p_pt_info));
	unsigned int pt_found = 0;
	uint64_t pt_pages = pt_size/PAGE_SIZE;
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	if (physicalAllocRaw((uint64_t*)&pt, pt_size)!=0){
		printf(L"failed to allocate page tables\r\n");
		return -1;
	}
	pt->pPageEntries = (struct p_page*)(pt+1);
	pt->pFreeEntries = (struct p_page**)(pt->pPageEntries+max_pages);
	pt->pUsedEntries = (struct p_page**)(pt->pFreeEntries+max_pages);
	return 0;
}
int initPageTable(void){
	uint64_t max_entries = pt_size/sizeof(struct p_page);
	for (uint64_t i = 0;i<max_entries;i++){
		struct p_page* pentry = pt->pPageEntries+i;
		pentry->status = PAGE_RESERVED;
		pentry->virtualAddress = 0x0;
		pt->pFreeEntries[i] = (struct p_page*)0x0;
		pt->pUsedEntries[i] = (struct p_page*)0x0;
	}
	UINTN memoryDescCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<memoryDescCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->Type==EfiMemoryMappedIO||pMemDesc->Type==EfiMemoryMappedIOPortSpace||pMemDesc->Type==EfiACPIReclaimMemory||pMemDesc->Type==EfiACPIMemoryNVS||pMemDesc->Type==EfiReservedMemoryType)
			continue;
		uint64_t page_index = (pMemDesc->PhysicalStart+(pMemDesc->NumberOfPages*EFI_PAGE_SIZE))/EFI_PAGE_SIZE;
		struct p_page* pentry = (struct p_page*)(pt->pPageEntries+page_index);
		uint64_t pa = pMemDesc->PhysicalStart;
		for (UINTN page = 0;page<pMemDesc->NumberOfPages;page++){
			if (pMemDesc->Type==EfiConventionalMemory){
				pentry->status = PAGE_FREE;
				pt->pFreeEntries[pt->freeEntryCnt] = pentry;
				pt->freeEntryCnt++;
			}
			else{
				pentry->status = PAGE_RESERVED;
			}
			pentry->virtualAddress = 0x0;
		}
		pentry->status = PAGE_RESERVED;
		pentry->virtualAddress = 0x0;
	}
	uint64_t pt_pages = pt_size/PAGE_SIZE;
	for (uint64_t i = 0;i<pt_pages;i++){
		uint64_t pa = ((uint64_t)pt)+(i*PAGE_SIZE);
		physicalFreePage(pa);
	}
	return 0;
}
int physicalAllocPage(uint64_t* pPhysicalAddress){
	if (!pPhysicalAddress)
		return -1;
	if (!pt->freeEntryCnt){
		printf(L"out of memory\r\n");
		return -1;	
	}
	struct p_page* pNewPage = pt->pFreeEntries[pt->freeEntryCnt-1];
	if (!pNewPage){
		printf(L"invalid free entry\r\n");
		return -1;
	}
	pNewPage->status = PAGE_INUSE;
	pt->pUsedEntries[pt->usedEntryCnt] = pNewPage;
	pt->freeEntryCnt--;
	pt->usedEntryCnt++;
	uint64_t pa = (uint64_t)(pNewPage-pt->pPageEntries);
	pa*=PAGE_SIZE;
	*pPhysicalAddress = pa;
	return 0;
}
int physicalFreePage(uint64_t physicalAddress){
	uint64_t pageEntry = (physicalAddress/PAGE_SIZE);
	struct p_page* pentry = pt->pPageEntries+pageEntry;
	pentry->status = PAGE_FREE;
	pt->pFreeEntries[pt->freeEntryCnt] = pentry;
	pt->freeEntryCnt++;
	return 0;
}
int physicalMapPage(uint64_t physicalAddress){
	struct p_page* pNewPage = pt->pPageEntries+(physicalAddress/sizeof(struct p_page));
	pNewPage->status = PAGE_INUSE;
	pt->pUsedEntries[pt->usedEntryCnt] = pNewPage;
	pt->usedEntryCnt++;
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
		if (pMemDesc->Type!=EfiConventionalMemory)
			continue;
		if (pMemDesc->NumberOfPages<requestedPages)
			continue;
		pMemDesc->NumberOfPages-=requestedPages;
		pMemDesc->PhysicalStart+=size;
		if (!pt){
			*pPhysicalAddress = (uint64_t)pMemDesc->PhysicalStart;
			return 0;
		}
		*pPhysicalAddress = (uint64_t)pMemDesc->PhysicalStart;
		for (UINTN page = 0;page<pMemDesc->NumberOfPages;page++){
			physicalMapPage(pMemDesc->PhysicalStart+(page*PAGE_SIZE));
		}
		return 0;
	}
	printf(L"failed to find %d page entries to fit block of size: %d\r\n", requestedPages, size);
	return -1;
}
int physicalFreeRaw(uint64_t physicalAddress, uint64_t size){
	UINTN entryCnt = pbootargs->memoryInfo.memoryMapSize/pbootargs->memoryInfo.memoryDescSize;
	for (UINTN i = 0;i<entryCnt;i++){
		EFI_MEMORY_DESCRIPTOR* pMemDesc = (EFI_MEMORY_DESCRIPTOR*)(((unsigned char*)pbootargs->memoryInfo.pMemoryMap)+(i*pbootargs->memoryInfo.memoryDescSize));
		if (pMemDesc->PhysicalStart-size!=physicalAddress)
			continue;
		pMemDesc->PhysicalStart-=size;
		pMemDesc->Type = EfiConventionalMemory;
		return 0;
	}
	return -1;
}
