#ifndef _PAGING
#define _PAGING
#define MEM_GB (uint64_t)1073741824
#define MEM_MB (uint64_t)1048576
#define MEM_KB (uint64_t)1024
int paging_init(void);
int load_pt(void* pml4);
void* get_pt(void);
#endif
