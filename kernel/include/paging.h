#ifndef _PAGING
#define _PAGING
#define MEM_GB (uint64_t)1073741824
#define MEM_MB (uint64_t)1048576
#define MEM_KB (uint64_t)1024
int paging_init(void);
int map_pages(uint64_t va, uint64_t pa, uint32_t flags, uint64_t pagecnt);
int load_pt(void* pml4);
void* get_pt(void);
void flush_tlb(void);
#endif
