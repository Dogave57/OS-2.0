#include "mem/pmm.h"
#include "mem/vmm.h"
#include "stdlib/stdlib.h"
#include "cpu/mutex.h"
#include "mem/heap.h"
struct heap_mem_info heapMemInfo = {0};
int heap_mem_init(void){
	struct heap_desc* pHeapDescList = (struct heap_desc*)0x00;
	uint64_t heapDescListSize = HEAP_MAX_COUNT*sizeof(struct heap_desc);
	if (virtualAlloc((uint64_t*)&pHeapDescList, heapDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY)!=0){
		printf("failed to allocate heap descriptor list lazy allocated physical pages\r\n");
		return -1;
	}
	uint64_t* pFreeHeapList = (uint64_t*)0x00;
	uint64_t freeHeapListSize = HEAP_MAX_COUNT*sizeof(uint64_t);
	if (virtualAlloc((uint64_t*)&pFreeHeapList, freeHeapListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY)!=0){
		printf("failed to allocate free heap descirptor list lazy allocated physical pages\r\n");
		virtualFree((uint64_t)pHeapDescList, heapDescListSize);
		return -1;
	}
	heapMemInfo.pHeapDescList = pHeapDescList;
	heapMemInfo.heapDescListSize = heapDescListSize;
	heapMemInfo.heapCount = 0x00;
	heapMemInfo.pFreeHeapList = pFreeHeapList;
	heapMemInfo.freeHeapListSize = freeHeapListSize;
	heapMemInfo.freeHeapCount = 0x00;
	return 0;
}
int heap_mem_deinit(void){
	if (virtualFree((uint64_t)heapMemInfo.pHeapDescList, heapMemInfo.heapDescListSize)!=0){
		printf("failed to free hepa volatile memory heap descriptor lazy allocated physical pages\r\n");
		virtualFree((uint64_t)heapMemInfo.pFreeHeapList, heapMemInfo.freeHeapListSize);
		return -1;
	}
	if (virtualFree((uint64_t)heapMemInfo.pFreeHeapList, heapMemInfo.freeHeapListSize)!=0){
		printf("failed to free heap volatile memory free heap descriptor physical pages\r\n");
		return -1;
	}
	return 0;
}
KAPI int heap_init(uint64_t reserveDataSize, uint64_t minBlockSize, uint64_t maxBlockSize, uint64_t* pHeapId){
	if (!pHeapId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pReserveData = (unsigned char*)0x00;
	if (virtualAlloc((uint64_t*)&pReserveData, reserveDataSize, PTE_RW|PTE_NX, 0x00)!=0){
		printf("failed to allocate heap volatile memory reserve lazy allocated physical pages\r\n");
		mutex_unlock(&mutex);
		return -1;		
	}
	struct heap_block_size_desc* pBlockSizeDescList = (struct heap_block_size_desc*)0x00;
	uint64_t blockSizeDescCount = 0x00;
	uint64_t blockSizeDescListSize = sizeof(struct heap_block_size_desc)*0x40;
	if (virtualAlloc((uint64_t*)&pBlockSizeDescList, blockSizeDescListSize, PTE_RW|PTE_NX, 0x00)!=0){
		printf("failed to allocate heap volatile memory block size descriptor list physical pages\r\n");
		virtualFree((uint64_t)pReserveData, reserveDataSize);
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t blockSize = minBlockSize;
	for (blockSizeDescCount = 0x00;;blockSizeDescCount++){
		if (blockSize>maxBlockSize)
			break;
		struct heap_block_size_desc* pBlockSizeDesc = pBlockSizeDescList+blockSizeDescCount;
		memset((void*)pBlockSizeDesc, 0, sizeof(struct heap_block_size_desc));
		uint64_t* pFreeBlockList = (uint64_t*)0x00;
		uint64_t freeBlockListSize = reserveDataSize/blockSize;
		if (virtualAlloc((uint64_t*)&pFreeBlockList, freeBlockListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY)!=0){
			printf("failed to allocate block size descriptor free block list lazy allocated physical pages\r\n");
			virtualFree((uint64_t)pReserveData, reserveDataSize);
			virtualFree((uint64_t)pBlockSizeDescList, blockSizeDescListSize);
			mutex_unlock(&mutex);
			return -1;
		}
		pBlockSizeDesc->blockSize = blockSize;
		pBlockSizeDesc->pFreeBlockList = pFreeBlockList;
		pBlockSizeDesc->freeBlockListSize = freeBlockListSize;
		pBlockSizeDesc->freeBlockCount = 0x00;
		blockSize*=0x02;
	}
	uint64_t heapId = 0x00;
	if (!heapMemInfo.freeHeapCount){
		heapId = heapMemInfo.heapCount;
	}
	if (heapMemInfo.freeHeapCount){
		heapMemInfo.freeHeapCount--;
		heapId = *(heapMemInfo.pFreeHeapList+heapMemInfo.freeHeapCount);
	}
	printf("heap volatile memory block size descriptor count: %d\r\n", blockSizeDescCount);
	struct heap_desc* pHeapDesc = heapMemInfo.pHeapDescList+heapId;
	memset((void*)pHeapDesc, 0, sizeof(struct heap_desc));
	pHeapDesc->heapInfo.pReserveData = pReserveData;
	pHeapDesc->heapInfo.reserveDataSize = reserveDataSize;
	pHeapDesc->heapInfo.reserveDataLength = 0x00;
	pHeapDesc->heapInfo.pBlockSizeDescList = pBlockSizeDescList;
	pHeapDesc->heapInfo.blockSizeDescListSize = blockSizeDescListSize;
	pHeapDesc->heapInfo.blockSizeDescCount = blockSizeDescCount;
	pHeapDesc->heapInfo.minBlockSize = minBlockSize;
	pHeapDesc->heapInfo.maxBlockSize = maxBlockSize;
	*pHeapId = heapId+0x01;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int heap_deinit(uint64_t heapId){
	if (!heapId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct heap_desc* pHeapDesc = heapMemInfo.pHeapDescList+heapId-0x01;
	if (virtualFree((uint64_t)pHeapDesc->heapInfo.pReserveData, pHeapDesc->heapInfo.reserveDataSize)!=0){
		printf("failed to free heap volatile memory reserve lazy allocated physical pages\r\n");
		virtualFree((uint64_t)pHeapDesc->heapInfo.pBlockSizeDescList, pHeapDesc->heapInfo.blockSizeDescListSize);
		mutex_unlock(&mutex);
		return -1;
	}
	for (uint64_t i = 0x00;i<pHeapDesc->heapInfo.blockSizeDescCount;i++){
		struct heap_block_size_desc* pBlockSizeDesc = pHeapDesc->heapInfo.pBlockSizeDescList+i;
		if (virtualFree((uint64_t)pBlockSizeDesc->pFreeBlockList, pBlockSizeDesc->freeBlockListSize)!=0){
			printf("failed to free heap volatile memory block size descriptor free block QWORD offset list\r\n");
			virtualFree((uint64_t)pHeapDesc->heapInfo.pBlockSizeDescList, pHeapDesc->heapInfo.blockSizeDescListSize);
			mutex_unlock(&mutex);
			return -1;
		}
	}
	if (virtualFree((uint64_t)pHeapDesc->heapInfo.pBlockSizeDescList, pHeapDesc->heapInfo.blockSizeDescListSize)!=0){
		printf("failed to free heap volatile memory block size list physical pages\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	*(heapMemInfo.pFreeHeapList+heapMemInfo.freeHeapCount) = heapId-0x01;
	heapMemInfo.freeHeapCount++;
	mutex_unlock(&mutex);
	return 0;
}
int heap_get_desc(uint64_t heapId, struct heap_desc** ppHeapDesc){
	if (!heapId||!ppHeapDesc)
		return -1;
	struct heap_desc* pHeapDesc = heapMemInfo.pHeapDescList+heapId-0x01;
	if (!pHeapDesc){
		printf("unexpected heap volatile memory heap descriptor\r\n");
		return -1;
	}
	*ppHeapDesc = pHeapDesc;
	return 0;
}
int heap_block_size_get_desc(struct heap_desc* pHeapDesc, uint64_t blockSize, struct heap_block_size_desc** ppBlockSizeDesc){
	if (!pHeapDesc||!blockSize||!ppBlockSizeDesc)
		return -1;
	for (uint64_t i = 0x00;i<pHeapDesc->heapInfo.blockSizeDescCount;i++){
		struct heap_block_size_desc* pBlockSizeDesc = pHeapDesc->heapInfo.pBlockSizeDescList+i;
		if (pBlockSizeDesc->blockSize<blockSize)
			continue;
		*ppBlockSizeDesc = pBlockSizeDesc;
		return 0;
	}
	return -1;
}
KAPI int heap_alloc(uint64_t heapId, unsigned char** ppData, uint64_t dataSize){
	if (!heapId||!dataSize||!ppData)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct heap_desc* pHeapDesc = (struct heap_desc*)0x00;
	if (heap_get_desc(heapId, &pHeapDesc)!=0){
		printf("failed to get heap volatile memory descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (dataSize>pHeapDesc->heapInfo.maxBlockSize){
		unsigned char* pData = (unsigned char*)0x00;
		if (virtualAlloc((uint64_t*)&pData, dataSize, PTE_RW|PTE_NX, 0x00)!=0){
			printf("failed to allocate heap volatile memory extensive physical pages\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		*ppData = pData;
		mutex_unlock(&mutex);
		return 0;
	}
	struct heap_block_size_desc* pBlockSizeDesc = (struct heap_block_size_desc*)0x00;
	if (heap_block_size_get_desc(pHeapDesc, dataSize, &pBlockSizeDesc)!=0){
		printf("failed to get heap volatile memory block size descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	dataSize = pBlockSizeDesc->blockSize;
	if (pBlockSizeDesc->freeBlockCount){
		pBlockSizeDesc->freeBlockCount--;
		uint64_t dataOffset = *(pBlockSizeDesc->pFreeBlockList+pBlockSizeDesc->freeBlockCount);
		unsigned char* pData = pHeapDesc->heapInfo.pReserveData+dataOffset;
		*ppData = pData;
		mutex_unlock(&mutex);
		return 0;
	}
	unsigned char* pData = pHeapDesc->heapInfo.pReserveData+pHeapDesc->heapInfo.reserveDataLength;
	pHeapDesc->heapInfo.reserveDataLength+=dataSize;
	*ppData = pData;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int heap_free(uint64_t heapId, unsigned char* pData, uint64_t dataSize){
	if (!heapId||!pData||!dataSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct heap_desc* pHeapDesc = (struct heap_desc*)0x00;
	if (heap_get_desc(heapId, &pHeapDesc)!=0){
		printf("failed to get heap volatile memory descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (dataSize>pHeapDesc->heapInfo.maxBlockSize){
		if (virtualFree((uint64_t)pData, dataSize)!=0){
			printf("failed to free extensive heap volatile memory physical pages\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		mutex_unlock(&mutex);
		return 0;
	}
	struct heap_block_size_desc* pBlockSizeDesc = (struct heap_block_size_desc*)0x00;
	if (heap_block_size_get_desc(pHeapDesc, dataSize, &pBlockSizeDesc)!=0){
		printf("failed to get heap block size descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	dataSize = pBlockSizeDesc->blockSize;
	uint64_t dataOffset = pData-pHeapDesc->heapInfo.pReserveData;
	*(pBlockSizeDesc->pFreeBlockList+pBlockSizeDesc->freeBlockCount) = dataOffset;	
	pBlockSizeDesc->freeBlockCount++;
	mutex_unlock(&mutex);
	return 0;
}
