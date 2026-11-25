#ifndef _VMM
#define _VMM
#include <stdint.h>
#include "kernel_include.h"
#include "pmm.h"
#define PTE_PRESENT (1ull<<0)
#define PTE_RW (1ull<<1)
#define PTE_USER (1ull<<2)
#define PTE_PWT (1ull<<3)
#define PTE_PCD (1ull<<4)
#define PTE_ACCESSED (1ull<<5)
#define PTE_DIRTY (1ull<<6)
#define PTE_PAT (1ull<<7)
#define PTE_GLOBAL (1ull<<8)
#define PTE_NX (1ull<<63)
#define PTE_LAZY (1ull<<9)
#define PTE_ADDR_MASK ((uint64_t)0x000FFFFFFFFFF000)
#define PTE_GET_ADDRESS(entry)(((uint64_t)entry)&PTE_ADDR_MASK)
#define PTE_GET_FLAGS(entry)(((uint64_t)entry)&~PTE_ADDR_MASK)
#define PTE_GET_PAT(entry)(((uint64_t)entry)&PTE_PAT)
#define PTE_IS_PRESENT(entry)((((uint64_t)entry)&PTE_PRESENT))
#define PTE_IS_WRITABLE(entry)(((uint64_t)entry)&PTE_RW)
#define PTE_IS_CACHABLE(entry)((((uint64_t)entry)&PTE_PCD))
#define PTE_PWT_ENABLED(entry)(((uint64_t)entry)&PTE_PWT)
#define PTE_IS_USER(entry)(((uint64_t)entry)&PTE_USER)
#define PTE_IS_DIRTY(entry)(((uint64_t)entry)&PTE_DIRTY)
#define PTE_IS_GLOBAL(entry)(((uint64_t)entry)&PTE_GLOBAL)
#define PTE_IS_EXECUTABLE(entry)(!(((uint64_t)entry)&PTE_NX))
#define PTE_IS_LAZY(entry)(((uint64_t)entry)&PTE_LAZY)
#define PTE_IS_MAPPED(entry)(PTE_IS_PRESENT(entry)||PTE_IS_LAZY(entry))
#define NON_CANONICAL_RANGE_LOW ((uint64_t)0x0000800000000000)
#define NON_CANONICAL_RANGE_HIGH ((uint64_t)0xFFFF7FFFFFFFFFFF)
#define VA_MAX ((uint64_t)0xFFFFFFFFFFFFFFFF)
#define VA_IS_CANONICAL(va)(!(va>NON_CANONICAL_RANGE_LOW&&va<NON_CANONICAL_RANGE_HIGH))
#define VMM_RANGE_BASE ((uint64_t)0xffff800000000000)
#define VMM_IDENTITY_MAP_BASE ((uint64_t)0xffff800000000000)
#define MAP_FLAG_LAZY (1<<0)
struct vmm_info{
	uint64_t* pml4List;
	uint64_t pNextVirtualPage;
};
int vmm_init(void);
int vmm_getPageTableEntry(uint64_t virtualAddress, uint64_t** ppEntry);
int vmm_getNextLevel(uint64_t* pCurrentLevel, uint64_t** ppNextLevel, uint64_t nextLevelIndex);
KAPI int virtualMapPage(uint64_t physicalAddress, uint64_t virtualAddress, uint64_t flags, uint8_t shared, uint64_t mapFlags);
KAPI int virtualMapPages(uint64_t physicalAddress, uint64_t virtualAddress, uint64_t flags, uint64_t pageCount, uint8_t shared, uint64_t mapFlags);
KAPI int virtualUnmapPage(uint64_t virtualAddress, uint64_t mapFlags);
KAPI int virtualUnmapPages(uint64_t virtualAddress, uint64_t pageCount, uint64_t mapFlags);
KAPI int virtualToPhysical(uint64_t virtualAddress, uint64_t* pPhysicalAddress);
KAPI int virtualAllocPage(uint64_t* pVirtualAddress, uint64_t flags, uint64_t mapFlags);
KAPI int virtualFreePage(uint64_t virtualAddress, uint64_t mapFlags);
KAPI int virtualAllocPages(uint64_t* pVirtualAddress, uint64_t pageCount, uint64_t flags, uint64_t mapFlags);
KAPI int virtualFreePages(uint64_t virtaulAddress, uint64_t pageCount);
KAPI int virtualAlloc(uint64_t* pVirtualAddress, uint64_t dataSize, uint64_t flags, uint64_t mapFlags);
KAPI int virtualFree(uint64_t virtualAddress, uint64_t dataSize);
KAPI int virtualProtectPage(uint64_t pPageData, uint64_t protectionFlag);
KAPI int virtualProtectPages(uint64_t pPageData, uint64_t pageCount, uint64_t protectionFlag);
KAPI int virtualGetPageFlags(uint64_t pPageData, uint64_t* pFlags);
KAPI int virtualGetSpace(uint64_t* ppPageData, uint64_t pageDataSize);
KAPI int pt_loaded(void);
uint64_t get_pml4(void);
int load_pml4(uint64_t pml4);
int flush_tlb(uint64_t va);
int flush_full_tlb(void);
#endif
