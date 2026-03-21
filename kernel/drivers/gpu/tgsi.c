#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/subsystem.h"
#include "cpu/mutex.h"
#include "align.h"
#include "stdlib/stdlib.h"
#include "drivers/gpu/tgsi.h"
static struct tgsi_driver_info tgsiDriverInfo = {0};
int tgsi_driver_init(void){
	if (subsystem_init(&tgsiDriverInfo.pContextSubsystemDesc, TGSI_MAX_CONTEXT_COUNT)!=0){
		printf("failed to initialize TGSI context subsystem\r\n");
		return -1;
	}

	return 0;
}
int tgsi_context_init(struct tgsi_context_desc** ppContextDesc){
	if (!ppContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct tgsi_context_desc* pContextDesc = (struct tgsi_context_desc*)kmalloc(sizeof(struct tgsi_context_desc));
	if (!pContextDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t contextId = 0;
	if (subsystem_alloc_entry(tgsiDriverInfo.pContextSubsystemDesc, (unsigned char*)pContextDesc, &contextId)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pContextDesc, 0, sizeof(struct tgsi_context_desc));
	uint64_t bytecodeReserve = TGSI_DEFAULT_BYTECODE_RESERVE;
	uint64_t bytecodeCommit = TGSI_DEFAULT_BYTECODE_COMMIT;
	pContextDesc->contextId = contextId;
	pContextDesc->bytecodeReserve = bytecodeReserve;
	pContextDesc->bytecodeCommit = bytecodeCommit;
	uint32_t* pBytecode = (uint32_t*)0x0;
	if (virtualAlloc((uint64_t*)&pBytecode, bytecodeReserve, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate virtual pages for TGSI bytecode reserve\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	for (uint64_t i = 0;i<align_up(bytecodeCommit, PAGE_SIZE)/PAGE_SIZE;i++){
		unsigned char* pBytecodePage = ((unsigned char*)pBytecode)+(PAGE_SIZE*i);
		*pBytecodePage = 0;
	}
	pContextDesc->pBytecode = pBytecode;
	*ppContextDesc = pContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_context_deinit(struct tgsi_context_desc* pContextDesc){
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (subsystem_free_entry(tgsiDriverInfo.pContextSubsystemDesc, pContextDesc->contextId)!=0){
		virtualFree((uint64_t)pContextDesc->pBytecode, pContextDesc->bytecodeReserve);
		kfree((void*)pContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFree((uint64_t)pContextDesc->pBytecode, pContextDesc->bytecodeReserve);
	kfree((void*)pContextDesc);
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_context_reset(struct tgsi_context_desc* pContextDesc){
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pContextDesc->tokenCount = 0;
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_context_get_bytecode_list(struct tgsi_context_desc* pContextDesc, uint32_t** ppBytecodeList){
	if (!pContextDesc||!ppBytecodeList)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint32_t* pBytecodeList = pContextDesc->pBytecode;
	*ppBytecodeList = pBytecodeList;
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_context_get_bytecode_size(struct tgsi_context_desc* pContextDesc, uint64_t* pBytecodeSize){
	if (!pContextDesc||!pBytecodeSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t bytecodeSize = pContextDesc->tokenCount*sizeof(uint32_t);
	*pBytecodeSize = bytecodeSize;
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_emit_token(struct tgsi_context_desc* pContextDesc, uint32_t token){
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint32_t* pToken = pContextDesc->pBytecode+pContextDesc->tokenCount;
	*pToken = token;
	pContextDesc->tokenCount++;
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_emit_token_list(struct tgsi_context_desc* pContextDesc, uint64_t tokenCount, uint32_t* pTokenList){
	if (!pContextDesc||!tokenCount||!pTokenList)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	for (uint64_t i = 0;i<tokenCount;i++){
		uint32_t token = pTokenList[i];
		if (tgsi_emit_token(pContextDesc, token)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
	}
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_compile_shader(struct tgsi_context_desc* pContextDesc, unsigned char* pShaderCode, uint64_t shaderCodeSize){
	if (!pContextDesc||!pShaderCode||!shaderCodeSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	
	mutex_unlock(&mutex);
	return 0;
}
