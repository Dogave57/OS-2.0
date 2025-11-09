#include "mem/vmm.h"
#include "mem/pmm.h"
#include "align.h"
#include "stdlib/stdlib.h"
#include "drivers/graphics.h"
#include "mem/heap.h"
struct heap_block_list* pBlockLists = (struct heap_block_list*)0x0;
uint64_t blockList_cnt = 0;
int heap_init(void){
	blockList_cnt = HEAP_LIST_COUNT;
	uint64_t blockSize = 16;
	uint64_t blockListSize = sizeof(struct heap_block_list)*blockList_cnt;
	uint64_t blockListPages = blockListSize/PAGE_SIZE;
	if (blockListSize%PAGE_SIZE)
		blockListPages++;
	if (virtualAllocPages((uint64_t*)&pBlockLists, blockListPages, PTE_RW, 0)!=0){
		printf(L"failed to allocate block lists\r\n");
		return -1;
	}
	for (uint64_t i = 0;i<blockList_cnt;i++,blockSize*=4){
		struct heap_block_list* pList = pBlockLists+i;
		pList->blockSize = blockSize;
		pList->pFirstNode = (struct heap_block_node*)0x0;
		pList->pLastNode = (struct heap_block_node*)0x0;
	}
	for (uint64_t i = 0;i<4;i++){
		uint64_t* pBlock = (uint64_t*)kmalloc(16);
		if (!pBlock){
			printf(L"failed to allocate heap block %d\r\n", i);
			return -1;
		}
		*pBlock = 67;
		kfree((void*)pBlock);
	}
	for (uint64_t i = 0;i<5;i++){
		uint64_t* pBlock = (uint64_t*)kmalloc(1024);
		if (!pBlock){
			printf(L"failed to allocate heap block %d\r\n", i);
			return -1;
		}
		uint64_t* pBlock2 = (uint64_t*)kmalloc(64);
		if (!pBlock2){
			printf(L"failed to allocate heap block 2: %d\r\n", i);
			return -1;
		}
		*pBlock = 67;
		*pBlock2 = 67;
		if (!(i%2))
			continue;
		if (kfree((void*)pBlock)!=0){
			printf(L"failed to free block\r\n");
			while (1){};
			return -1;
		}
	}
	uint64_t* test1 = (uint64_t*)kmalloc(1024);
	if (!test1){
		printf(L"failed to allocate test1\r\n");
		return -1;
	}
	if (kfree((void*)test1)!=0){
		printf(L"heap free mechanism failed so it litreally cooked bro\r\n");
		return -1;
	}
	uint64_t* test2 = (uint64_t*)kmalloc(1024);
	if (!test2){
		printf(L"failed to allocate test2\r\n");
		return -1;
	}
	if (test1!=test2){
		printf(L"heap free mechanism is cooked bro\r\n");
		return -1;
	}
	return 0;
}
void* kmalloc(uint64_t size){
	if (size>HEAP_MAX_BLOCK_SIZE||size<HEAP_MIN_BLOCK_SIZE)
		return (void*)0x0;
	if (!pBlockLists){
		if (heap_init()!=0)
			return (void*)0x0;
	}
	struct heap_block_list* pBlockList = (struct heap_block_list*)0x0;
	if (heap_get_block_list(&pBlockList, size)!=0){
		printf(L"failed to get block list\r\n");
		return (void*)0x0;
	}
	uint64_t block_va = 0;
	if (heap_get_block(&block_va, pBlockList)!=0){
		printf(L"failed to get block\r\n");
		return (void*)0x0;	
	}
	if (heap_pop_block(pBlockList)!=0){
		printf(L"failed to pop block\r\n");
		return (void*)0x0;
	}
	struct heap_block_hdr* pHdr = (struct heap_block_hdr*)block_va;
	return (void*)(pHdr+1);
}
int kfree(void* pBlock){
	if (!pBlockLists){
		if (heap_init()!=0)
			return -1;
	}
	if (!pBlock)
		return -1;
	struct heap_block_hdr* pHdr = (struct heap_block_hdr*)pBlock;
	pHdr--;
	if (pHdr->va!=((uint64_t)pHdr)){
		printf(L"corrupted heap block at: %p\r\n", (void*)pBlock);
		printf(L"va points to: %p\r\n", (void*)pHdr->va);
		while (1){};
		return -1;
	}
	struct heap_block_list* pList = pHdr->pList;
	if (heap_push_block(pList, pHdr->va)!=0){
		printf(L"failed to push free block\r\n");
		return -1;
	}
	return 0;
}
int heap_get_block_list(struct heap_block_list** ppBlockList, uint64_t size){
	if (!ppBlockList||!size)
		return -1;
	for (uint64_t i = 0;i<blockList_cnt;i++){
		struct heap_block_list* pBlockList = pBlockLists+i;
		if (pBlockList->blockSize<size)
			continue;
		*ppBlockList = pBlockList;
		return 0;
	}
	return -1;
}
int heap_fill_block_list(struct heap_block_list* pBlockList){
	if (!pBlockList)
		return -1;
	uint64_t blockSize = pBlockList->blockSize+sizeof(struct heap_block_hdr);
	uint64_t blocksToAlloc = HEAP_BLOCK_PER_NODE;
	uint64_t blockDataSize = align_up(blockSize*blocksToAlloc, PAGE_SIZE);
	uint64_t blockDataPages = blockDataSize/PAGE_SIZE;
	uint64_t pPage = 0;
	if (virtualAllocPages(&pPage, blockDataPages, PTE_RW, 0)!=0){
		return -1;
	}
	struct heap_block_node* pNewNode = (struct heap_block_node*)0x0;
	if (heap_alloc_block_node(pBlockList)!=0){
		printf(L"failed to allocate block node\r\n");
		return -1;
	}
	pNewNode = pBlockList->pLastNode;
	if (!pNewNode){
		printf(L"invalid last node when filling block list\r\n");
		return -1;
	}
	pNewNode->pPage = pPage;
	pNewNode->pageCnt = blockDataPages;
	for (uint64_t i = 0;i<HEAP_BLOCK_PER_NODE;i++){
		uint64_t block_va = pPage+(i*blockSize);
		if (heap_push_block(pBlockList, block_va)!=0){
			printf(L"failed to push block\r\n");
			return -1;
		}
	}
	return 0;
}
int heap_push_block(struct heap_block_list* pBlockList, uint64_t va){
	if (!pBlockList){
		printf(L"invalid block list\r\n");
		return -1;
	}
	if (!va){
		printf(L"invalid block va\r\n");
		return -1;
	}
	struct heap_block_node* pLastNode = pBlockList->pLastNode;
	if (!pLastNode){
		if (heap_alloc_block_node(pBlockList)!=0){
			printf(L"failed to allocate new node\r\n");
			return -1;
		}
		pLastNode = pBlockList->pLastNode;
	}
	if (!pLastNode){
		printf(L"failed to allocate new node\r\n");
		return -1;
	}
	if (pLastNode->freeBlockCnt>HEAP_BLOCK_PER_NODE){
		printf(L"node too full!\r\n");
		return -1;
	}
	pLastNode->va[pLastNode->freeBlockCnt] = va;
	struct heap_block_hdr* pHdr = (struct heap_block_hdr*)va;
	pHdr->onHeap = 1;
	pHdr->pList = pBlockList;
	pHdr->va = va;
	pLastNode->freeBlockCnt++;
	if (pLastNode->freeBlockCnt>HEAP_BLOCK_PER_NODE){
		if (heap_free_block_node(pBlockList, pLastNode)!=0){
			printf(L"failed to free node\r\n");
			return -1;
		}
		return 0;
	}
	return 0;
}
int heap_pop_block(struct heap_block_list* pBlockList){
	if (!pBlockList)
		return -1;
	struct heap_block_node* pLastNode = pBlockList->pLastNode;
	if (!pLastNode){
		printf(L"no last node\r\n");
		return -1;
	}
	if (!pLastNode->freeBlockCnt)
		return -1;
	pLastNode->freeBlockCnt--;
	return 0;
}
int heap_get_block(uint64_t* pVa, struct heap_block_list* pBlockList){
	if (!pVa||!pBlockList)
		return -1;
	struct heap_block_node* pLastNode = pBlockList->pLastNode;
	if (!pLastNode){
		if (heap_fill_block_list(pBlockList)!=0){
			printf(L"failed to fill block list\r\n");
			return -1;
		}
	}
	pLastNode = pBlockList->pLastNode;
	if (!pLastNode){
		printf(L"invalid last node\r\n");	
		return -1;
	}
	if (!pLastNode->freeBlockCnt){
		printf(L"no blocks available\r\n");
		return -1;
	}
	uint64_t va = pLastNode->va[pLastNode->freeBlockCnt-1];
	*pVa = va;
	return 0;
}
int heap_alloc_block_node(struct heap_block_list* pBlockList){
	if (!pBlockList)
		return -1;
	struct heap_block_node* pNewNode = (struct heap_block_node*)0x0;
	if (virtualAllocPage((uint64_t*)&pNewNode, PTE_RW, 0)!=0){
		printf(L"failed to allocate page for new block node\r\n");
		return -1;
	}
	pNewNode->pPage = 0;
	pNewNode->pageCnt = 0;
	pNewNode->freeBlockCnt = 0;
	pNewNode->pflink = (struct heap_block_node*)0x0;
	pNewNode->pblink = (struct heap_block_node*)0x0;
	if (pBlockList->pLastNode)
		pBlockList->pLastNode->pflink = pNewNode;
	pNewNode->pblink = pBlockList->pLastNode;
	pBlockList->pLastNode = pNewNode;
	if (!pBlockList->pFirstNode)
		pBlockList->pFirstNode = pNewNode;
	return 0;
}
int heap_free_block_node(struct heap_block_list* pBlockList, struct heap_block_node* pBlock){
	if (!pBlockList||!pBlock)
		return -1;
	if (pBlock->pflink)
		pBlock->pflink->pblink = pBlock->pblink;
	if (pBlock->pblink)
		pBlock->pblink->pflink = pBlock->pflink;
	if (pBlock==pBlockList->pFirstNode)
		pBlockList->pFirstNode = pBlock->pflink;
	if (pBlock==pBlockList->pLastNode)
		pBlockList->pLastNode = pBlock->pblink;
	if (virtualFreePage((uint64_t)pBlock, 0)!=0)
		return -1;
	return 0;
}
