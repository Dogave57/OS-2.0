#ifndef _HEAP
#define _HEAP
#include "vmm.h"
#include "pmm.h"
#define HEAP_MAX_BLOCK_SIZE (1024)
#define HEAP_MIN_BLOCK_SIZE (16)
#define HEAP_LIST_COUNT (4)
#define HEAP_BLOCK_PER_NODE 507
struct heap_block_node{
	struct heap_block_node* pblink;
	struct heap_block_node* pflink;
	uint64_t pageCnt;
	uint64_t pPage;
	uint64_t freeBlockCnt;
	uint64_t va[HEAP_BLOCK_PER_NODE];
}__attribute__((packed));
struct heap_block_list{
	uint64_t blockSize;
	struct heap_block_node* pFirstNode;
	struct heap_block_node* pLastNode;
};
struct heap_block_hdr{
	uint64_t heapTracked;
	struct heap_block_list* pList;
	uint64_t pageCnt;
	uint64_t va;
};
int heap_init(void);
void* kmalloc(uint64_t size);
int kfree(void* pBlock);
int heap_get_block_list(struct heap_block_list** ppBlockList, uint64_t size);
int heap_fill_block_list(struct heap_block_list* pBlockList);
int heap_push_block(struct heap_block_list* pBlockList, uint64_t va);
int heap_pop_block(struct heap_block_list* pBlockList);
int heap_get_block(uint64_t* pVa, struct heap_block_list* pBlockList);
int heap_alloc_block_node(struct heap_block_list* pBlockList);
int heap_free_block_node(struct heap_block_list* pBlockList, struct heap_block_node* pBlock);
#endif
