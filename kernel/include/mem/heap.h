#ifndef _MEM_HEAP
#define _MEM_HEAP
#include "mem/heap.h"
#define HEAP_MAX_COUNT (0x4000)
struct heap_block_size_desc{
	uint64_t blockSize;
	uint64_t* pFreeBlockList;
	uint64_t freeBlockListSize;
	uint64_t freeBlockCount;
};
struct heap_info{
	unsigned char* pReserveData;
	uint64_t reserveDataSize;
	uint64_t reserveDataLength;
	struct heap_block_size_desc* pBlockSizeDescList;
	uint64_t blockSizeDescListSize;
	uint64_t blockSizeDescCount;
	uint64_t minBlockSize;
	uint64_t maxBlockSize;
};
struct heap_desc{
	uint64_t heapId;
	struct heap_info heapInfo;
};
struct heap_mem_info{
	struct heap_desc* pHeapDescList;
	uint64_t heapDescListSize;
	uint64_t heapCount;
	uint64_t* pFreeHeapList;
	uint64_t freeHeapListSize;
	uint64_t freeHeapCount;
};
int heap_mem_init(void);
int heap_mem_deinit(void);
KAPI int heap_init(uint64_t reserveSize, uint64_t minBlockSize, uint64_t maxBlockSize, uint64_t* pHeapId);
KAPI int heap_deinit(uint64_t heapId);
int heap_get_desc(uint64_t heapId, struct heap_desc** ppHeapDesc);
int heap_block_size_get_desc(struct heap_desc* pHeapDesc, uint64_t dataSize, struct heap_block_size_desc** ppBlockSizeDesc);
KAPI int heap_alloc(uint64_t heapId, unsigned char** ppData, uint64_t dataSize);
KAPI int heap_free(uint64_t heapId, unsigned char* pData, uint64_t dataSize);
#endif
