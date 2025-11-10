#ifndef _VMM
#define _VMM
#include <stdint.h>
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
#define PTE_ADDR_MASK 0x000ffffffffffff000
#define PTE_GET_ADDR(entry)((((uint64_t)entry)&PTE_ADDR_MASK))
#define PTE_GET_PAT(entry)(((uint64_t)entry)&PTE_PAT)
#define PTE_IS_PRESENT(entry)((((uint64_t)entry)&PTE_PRESENT))
#define PTE_IS_WRITABLE(entry)(((uint64_t)entry)&PTE_RW)
#define PTE_IS_CACHABLE(entry)((((uint64_t)entry)&PTE_PCD))
#define PTE_PWT_ENABLED(entry)(((uint64_t)entry)&PTE_PWT)
#define PTE_IS_USER(entry)(((uint64_t)entry)&PTE_USER)
#define PTE_IS_DIRTY(entry)(((uint64_t)entry)&PTE_DIRTY)
#define PTE_IS_GLOBAL(entry)(((uint64_t)entry)&PTE_GLOBAL)
#define PTE_IS_EXECUTABLE(entry)(!(((uint64_t)entry)&PTE_NX))
#define VMM_RANGE_INVALID 0
#define VMM_RANGE_FREE 1
#define VMM_RANGE_INUSE 2
#define VMM_MAX_RANGE 169
#define MAP_FLAGS_NO_RANGE_ALLOC (1ull<<0)
struct vmm_range_entry{
	uint64_t status;
	uint64_t min;
	uint64_t max;
}__attribute__((packed));
struct vmm_range_node{
	struct vmm_range_node* pflink;
	struct vmm_range_node* pblink;
	uint64_t currentRange;
	uint8_t padding[16];
	struct vmm_range_entry ranges[169];
}__attribute__((packed));
int vmm_init(void);
int vmm_getPageTableEntry(uint64_t va, uint64_t** ppEntry);
int vmm_getNextLevel(uint64_t* pCurrentLevel, uint64_t** ppNextLevel, uint64_t index);
int virtualMapPage(uint64_t pa, uint64_t va, uint64_t flags, unsigned int shared, uint64_t map_flags, uint64_t pageType);
int virtualMapPages(uint64_t pa, uint64_t va, uint64_t flags, uint64_t page_cnt, unsigned int shared, uint64_t map_flags, uint64_t pageType);
int virtualUnmapPage(uint64_t va, uint64_t map_flags);
int virtualUnmapPages(uint64_t va, uint64_t page_cnt, uint64_t map_flags);
int virtualToPhysical(uint64_t va, uint64_t* pVa);
int virtualAllocPage(uint64_t* pVa, uint64_t flags, uint64_t map_flags, uint64_t pageType);
int virtualFreePage(uint64_t va, uint64_t map_flags);
int virtualAllocPages(uint64_t* pVa, uint64_t page_cnt, uint64_t flags, uint64_t map_flags, uint64_t pageType);
int virtualFreePages(uint64_t va, uint64_t pagecnt);
uint64_t get_pt(void);
int load_pt(uint64_t pml4);
int flush_tlb(uint64_t va);
int flush_full_tlb(void);
#endif
