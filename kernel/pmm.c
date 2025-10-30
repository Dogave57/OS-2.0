#include "stdlib.h"
#include "graphics.h"
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
		pt = (struct p_pt_info*)pMemDesc->PhysicalStart;
		pt->pPageEntries = (struct p_page*)(pt+1);
		pt->pFreeEntries = (struct p_page**)(pt->pPageEntries+max_pages);
		pt->pUsedEntries = pt->pFreeEntries+max_pages;
		printf(L"p entries: %p\r\nfree entries: %p\r\nused entries: %p\r\n", (void*)pt->pPageEntries, (void*)pt->pFreeEntries, (void*)pt->pUsedEntries);
		printf(L"max pages: %d\r\n", max_pages);
		freeMemory-=pt_size;
		return 0;
	}
	return -1;
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

