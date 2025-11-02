#ifndef _VMM
#define _VMM
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
#define PTE_GET_ADDR(addr)(((uint64_t)addr&PTE_ADDR_MASK))
int vmm_init(void);
int vmm_getNextLevel(uint64_t* pCurrentLevel, uint64_t** ppNextLevel, uint64_t index, uint64_t flags);
int virtualMapPage(uint64_t pa, uint64_t* pVa, uint64_t flags, unsigned int alloc_new);
int virtualMapPages(uint64_t pa, uint64_t* pVa, uint64_t flags, uint64_t page_cnt, unsigned int alloc_new);
int virtualUnmapPage(uint64_t va);
uint64_t get_pt(void);
int load_pt(uint64_t pml4);
int flush_tlb(void);
#endif
