#include "stdlib/stdlib.h"
#include "cpu/mutex.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "align.h"
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
#include "drivers/timer.h"
#include "drivers/shaders/ggsl.h"
static struct ggsl_driver_info driverInfo = {0};
int ggsl_driver_init(void){
	static const unsigned char ggslGuid[16] = GGSL_DRIVER_IDENT;
	struct gpu_shader_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct gpu_shader_driver_vtable));
	vtable.driverInit = ggsl_driver_subsystem_driver_init;
	vtable.driverDeinit = ggsl_driver_subsystem_driver_deinit;
	vtable.shaderInit = ggsl_driver_subsystem_shader_init;
	vtable.shaderDeinit = ggsl_driver_subsystem_shader_deinit;
	vtable.instructionListReset = ggsl_driver_subsystem_instruction_list_reset;
	vtable.instructionGetInfo = ggsl_driver_subsystem_instruction_get_info;
	struct gpu_shader_driver_info subsystemDriverInfo = {0};
	memset((void*)&subsystemDriverInfo, 0, sizeof(struct gpu_shader_driver_info));
	memcpy((void*)subsystemDriverInfo.ident, (void*)ggslGuid, sizeof(uint8_t)*16);
	uint64_t driverId = 0x00;
	if (gpu_shader_driver_register(vtable, subsystemDriverInfo, &driverId)!=0){
		printf("failed to register GPU host controller GGSL shader driver\r\n");
		return -1;
	}
	driverInfo.driverId = driverId;
	driverInfo.vtable = vtable;
	return 0;
}
int ggsl_driver_deinit(void){
	if (gpu_shader_driver_unregister(driverInfo.driverId)!=0){
		printf("failed to unregister GPU host controller GGSL shader driver\r\n");
		return -1;
	}
	return 0;
}
int ggsl_driver_hash16(unsigned char* pData, uint64_t dataSize, uint16_t* pHash){
	if (!pData||!pHash)
		return -1;	
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint16_t hash = 0x811C;
	for (uint64_t i = 0;i<dataSize;i++){
		uint8_t dataEntry = *(pData+i);
		hash = (dataEntry^hash)*0x8422;
	}
	*pHash = hash;
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_ident_register(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_info* pIdentInfo, struct ggsl_driver_ident_desc** ppIdentDesc){
	if (!pShaderDesc||!pIdentInfo||!ppIdentDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (ggsl_driver_hash16(pShaderDesc->pShaderCode+pIdentInfo->offset, pIdentInfo->length, &pIdentInfo->hash)!=0){
		printf("failed to obtain GPU host controller GGSL shader driver identifier WORD hash\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ggsl_driver_ident_list_desc* pIdentListDesc = pShaderDesc->pIdentListDescList+pIdentInfo->hash;
	struct ggsl_driver_ident_list_desc* pIdentListPageDesc = (struct ggsl_driver_ident_list_desc*)align_down((uint64_t)pIdentListDesc, PAGE_SIZE);
	uint64_t physicalAddress = 0x00;
	if (virtualToPhysical((uint64_t)pIdentListPageDesc, (uint64_t*)&physicalAddress)!=0){
		printf("failed to get physical page address of GPU host controller GGSL driver identifier list descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!physicalAddress){
		memset((void*)pIdentListPageDesc, 0, PAGE_SIZE);
	}
	struct ggsl_driver_ident_desc* pIdentDesc = (struct ggsl_driver_ident_desc*)kmalloc(sizeof(struct ggsl_driver_ident_desc));
	if (!pIdentDesc){
		printf("failed to allocate GPU host controller GGSL shader driver identifier descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pIdentDesc, 0, sizeof(struct ggsl_driver_ident_desc));
	uint64_t identId = 0x00;
	if (subsystem_alloc_entry(pShaderDesc->pIdentSubsystemDesc, (unsigned char*)pIdentDesc, &identId)!=0){
		printf("failed to allocate GPU host controller GGSL shader driver identifier descriptor entry\r\n");
		kfree((void*)pIdentDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pIdentListDesc->pFirstIdentDesc)
		pIdentListDesc->pFirstIdentDesc = pIdentDesc;
	if (pIdentListDesc->pLastIdentDesc){
		pIdentListDesc->pLastIdentDesc->pFlink = pIdentDesc;
		pIdentDesc->pBlink = pIdentListDesc->pLastIdentDesc;
	}
	pIdentListDesc->pLastIdentDesc = pIdentDesc;
	pIdentDesc->identId = identId;
	pIdentDesc->identInfo = *pIdentInfo;
	*ppIdentDesc = pIdentDesc;
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_ident_unregister(struct ggsl_driver_shader_desc* pShaderDesc, uint64_t identId){
	if (!pShaderDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct ggsl_driver_ident_desc* pIdentDesc = (struct ggsl_driver_ident_desc*)0x00;
	if (subsystem_get_entry(pShaderDesc->pIdentSubsystemDesc, identId, (uint64_t*)&pIdentDesc)!=0){
		printf("failed to get GPU host controller GGSL shader driver identifier descriptor entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pIdentDesc->pBlink)
		pIdentDesc->pBlink->pFlink = pIdentDesc->pFlink;
	if (pIdentDesc->pFlink)
		pIdentDesc->pFlink->pBlink = pIdentDesc->pBlink;
	kfree((void*)pIdentDesc);
	subsystem_free_entry(pShaderDesc->pIdentSubsystemDesc, identId);
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_ident_get_desc(struct ggsl_driver_shader_desc* pShaderDesc, unsigned char* pIdent, uint64_t identLength, struct ggsl_driver_ident_desc** ppIdentDesc){
	if (!pShaderDesc||!pIdent||!ppIdentDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint16_t hash = 0x00;
	if (ggsl_driver_hash16(pIdent, identLength, &hash)!=0){
		printf("failed to hash GPU host controller GGSL shader driver identifier name\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ggsl_driver_ident_list_desc* pIdentListDesc = pShaderDesc->pIdentListDescList+hash;
	struct ggsl_driver_ident_list_desc* pIdentListPageDesc = (struct ggsl_driver_ident_list_desc*)(align_down((uint64_t)pIdentListDesc, PAGE_SIZE));
	uint64_t physicalAddress = 0x00;
	if (virtualToPhysical((uint64_t)pIdentListPageDesc, (uint64_t*)&physicalAddress)!=0){
		printf("failed to get GPU host controller GGSL shader driver identifier list descriptor physical page address\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!physicalAddress){
		printf("unknown identifier\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	unsigned char* pShaderCode = pShaderDesc->pShaderCode;
	uint64_t shaderCodeSize = pShaderDesc->shaderCodeSize;
	struct ggsl_driver_ident_desc* pCurrentIdentDesc = pIdentListDesc->pFirstIdentDesc;
	while (pCurrentIdentDesc){
		if (pCurrentIdentDesc->identInfo.length!=identLength||pCurrentIdentDesc->identInfo.hash!=hash){
			pCurrentIdentDesc = pCurrentIdentDesc->pFlink;
			continue;
		}
		uint8_t identEqual = 0x01;
		for (uint64_t i = 0;i<identLength;i++){
			if (*(pShaderCode+pCurrentIdentDesc->identInfo.offset+i)!=*(pIdent+i)){
				identEqual = 0x00;
				break;
			}
		}
		if (!identEqual){
			pCurrentIdentDesc = pCurrentIdentDesc->pFlink;
			continue;
		}
		*ppIdentDesc = pCurrentIdentDesc;
		mutex_unlock(&mutex);
		return 0;
	}
	mutex_unlock(&mutex);
	return -1;
}
int ggsl_driver_ident_get_next_desc(struct ggsl_driver_shader_desc* pShaderDesc, struct ggsl_driver_ident_desc** ppIdentDesc){
	if (!pShaderDesc||!ppIdentDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pShaderCode = pShaderDesc->pShaderCode;
	uint64_t shaderCodeSize = pShaderDesc->shaderCodeSize;
	uint64_t shaderCodeOffset = pShaderDesc->shaderCodeOffset;
	uint64_t identStart = shaderCodeOffset;
	for (;shaderCodeOffset<shaderCodeSize;shaderCodeOffset++){
		static const unsigned char identEndList[256]={
			[' ']=0x01,
			['\n']=0x01,
		};
		if (!identEndList[pShaderCode[shaderCodeOffset]])
			continue;
		uint64_t identLength = shaderCodeOffset-identStart;
		uint16_t hash = 0x00;
		ggsl_driver_hash16((unsigned char*)(pShaderCode+identStart), identLength, &hash);
		struct ggsl_driver_ident_desc* pIdentDesc = (struct ggsl_driver_ident_desc*)0x00;
		struct ggsl_driver_ident_info identInfo = {0};
		memset((void*)&identInfo, 0, sizeof(struct ggsl_driver_ident_info));
		identInfo.offset = identStart;
		identInfo.length = identLength;
		identInfo.identType = GGSL_DRIVER_IDENT_TYPE_OPCODE;
		printf("registering GPU host controller GGSL identifier\r\n");
		if (ggsl_driver_ident_register(pShaderDesc, &identInfo, &pIdentDesc)!=0){
			printf("failed to register GPU host controller GGSL shader identifier descriptor\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		printf("getting GPU host controller GGSL identifier descriptor\r\n");
		uint64_t startTime = get_time_us();
		if (ggsl_driver_ident_get_desc(pShaderDesc, pShaderCode+identStart, identLength, &pIdentDesc)!=0){
			printf("unknown reference: ");
			for (uint64_t i = 0;i<identLength;i++){
				putchar(pShaderCode[identStart+i]);
			}
			putchar('\n');
			mutex_unlock(&mutex);
			return -1;
		}
		uint64_t elapsedTime = get_time_us()-startTime;
		printf("elapsed time: %fms\r\n", ((double)elapsedTime)/1000.0);
		pShaderDesc->shaderCodeOffset = shaderCodeOffset+0x01;
		mutex_unlock(&mutex);
		return 0;
	}
	mutex_unlock(&mutex);
	return -1;
}
int ggsl_driver_instruction_list_reset(struct ggsl_driver_shader_desc* pShaderDesc){
	if (!pShaderDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pShaderDesc->shaderCodeOffset = sizeof(uint32_t)+0x01;
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_instruction_get_info(struct ggsl_driver_shader_desc* pShaderDesc, struct gpu_instruction_info* pInstructionInfo){
	if (!pShaderDesc||!pInstructionInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pInstructionInfo->opcode = 0x00;
	uint64_t identCount = 0x00;
	for (identCount = 0x00;;identCount++){
		struct ggsl_driver_ident_desc* pIdentDesc = (struct ggsl_driver_ident_desc*)0x00;
		if (ggsl_driver_ident_get_next_desc(pShaderDesc, &pIdentDesc)!=0){
			printf("failed to get GPU host controller GGSL shader protocol driver identifier descriptor\r\n");
			mutex_unlock(&mutex);
			return 0;
		}
	}
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_subsystem_driver_init(uint64_t driverId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_shader_driver_desc* pDriverDesc = (struct gpu_shader_driver_desc*)0x00;
	if (gpu_shader_driver_get_desc(driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller shader driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_subsystem_driver_deinit(uint64_t driverId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_shader_driver_desc* pDriverDesc = (struct gpu_shader_driver_desc*)0x00;
	if (gpu_shader_driver_get_desc(driverId, &pDriverDesc)!=0){
		printf("failde to get GPU host controller shader driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_subsystem_shader_init(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateObjectInfo){
	if (!pCreateObjectInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x00;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)0x00;
	if (gpu_context_get_desc(gpuId, contextId, &pContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_shader_object_desc* pShaderObjectDesc = (struct gpu_shader_object_desc*)0x00;
	if (gpu_object_get_desc(gpuId, objectId, (struct gpu_object_desc**)&pShaderObjectDesc)!=0){
		printf("failed to GPU host controller shader object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (*(uint32_t*)pCreateObjectInfo->pShaderCode!=GGSL_DRIVER_SIGNATURE){
		printf("invalid GPU host controller GGSL signature 0x%x\r\n", (uint64_t)*(uint32_t*)pCreateObjectInfo->pShaderCode);
		mutex_unlock(&mutex);
		return -1;
	}
	struct ggsl_driver_shader_desc* pShaderDesc = (struct ggsl_driver_shader_desc*)kmalloc(sizeof(struct ggsl_driver_shader_desc));
	if (!pShaderDesc){
		printf("failed to allocate GPU host controller GGSL shader object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pShaderDesc, 0, sizeof(struct ggsl_driver_shader_desc));
	struct subsystem_desc* pIdentSubsystemDesc = (struct subsystem_desc*)0x00;
	uint64_t maxIdentCount = GGSL_DRIVER_MAX_IDENT_COUNT;
	if (subsystem_init(&pIdentSubsystemDesc, maxIdentCount)!=0){
		printf("failed to initialize GPU host controller GGSL shader identifier subsystem\r\n");
		subsystem_deinit(pIdentSubsystemDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct ggsl_driver_ident_list_desc* pIdentListDescList = (struct ggsl_driver_ident_list_desc*)0x00;
	uint64_t identListDescListSize = sizeof(struct ggsl_driver_ident_list_desc)*GGSL_DRIVER_IDENT_LIST_COUNT;
	if (virtualAlloc((uint64_t*)&pIdentListDescList, identListDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical pages for GPU host controller GGSL shader identifier list descriptor list\r\n");
		subsystem_deinit(pIdentSubsystemDesc);
		mutex_unlock(&mutex);	
		return -1;
	}
	pShaderDesc->shaderObjectId = objectId;
	pShaderDesc->pShaderCode = pCreateObjectInfo->pShaderCode;
	pShaderDesc->shaderCodeSize = pCreateObjectInfo->shaderCodeSize;
	pShaderDesc->shaderType = pCreateObjectInfo->shaderType;
	pShaderDesc->pIdentSubsystemDesc = pIdentSubsystemDesc;
	pShaderDesc->maxIdentCount = maxIdentCount;
	pShaderDesc->pIdentListDescList = pIdentListDescList;
	pShaderDesc->identListDescListSize = identListDescListSize;
	pShaderObjectDesc->shaderDriverExtra = (uint64_t)pShaderDesc;
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_subsystem_shader_deinit(uint64_t gpuId, uint64_t contextId, uint64_t objectId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x00;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)0x00;
	if (gpu_context_get_desc(gpuId, contextId, &pContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_shader_object_desc* pShaderObjectDesc = (struct gpu_shader_object_desc*)0x00;
	if (gpu_object_get_desc(gpuId, objectId, (struct gpu_object_desc**)&pShaderObjectDesc)!=0){
		printf("failed to get GPU host controller shader object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ggsl_driver_shader_desc* pShaderDesc = (struct ggsl_driver_shader_desc*)pShaderObjectDesc->shaderDriverExtra;
	if (!pShaderDesc){
		printf("GPU host controller GGSL shader object descriptor not linked wtih GGSL shader descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	subsystem_deinit(pShaderDesc->pIdentSubsystemDesc);
	virtualFree((uint64_t)pShaderDesc->pIdentListDescList, pShaderDesc->identListDescListSize);
	kfree((void*)pShaderDesc);
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_subsystem_instruction_list_reset(uint64_t gpuId, uint64_t contextId, uint64_t objectId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x00;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return-1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)0x00;
	if (gpu_context_get_desc(gpuId, contextId, &pContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_shader_object_desc* pShaderObjectDesc = (struct gpu_shader_object_desc*)0x00;
	if (gpu_object_get_desc(gpuId, objectId, (struct gpu_object_desc**)&pShaderObjectDesc)!=0){
		printf("failed to get GPU host controller shader object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ggsl_driver_shader_desc* pShaderDesc = (struct ggsl_driver_shader_desc*)pShaderObjectDesc->shaderDriverExtra;
	if (!pShaderDesc){
		printf("GPU host controller shader object descriptor not linked with GPU host controller GGSL shader descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (ggsl_driver_instruction_list_reset(pShaderDesc)!=0){
		printf("failed to reset GPU host controller GGSL shader descriptor instruction list\r\n");	
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int ggsl_driver_subsystem_instruction_get_info(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_instruction_info* pInstructionInfo){
	if (!pInstructionInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x00;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)0x00;
	if (gpu_context_get_desc(gpuId, contextId, &pContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_shader_object_desc* pShaderObjectDesc = (struct gpu_shader_object_desc*)0x00;
	if (gpu_object_get_desc(gpuId, objectId, (struct gpu_object_desc**)&pShaderObjectDesc)!=0){
		printf("failed to get GPU host controller shader object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct ggsl_driver_shader_desc* pShaderDesc = (struct ggsl_driver_shader_desc*)pShaderObjectDesc->shaderDriverExtra;
	if (!pShaderDesc){
		printf("GPU host controller shader object descriptor not linked with GPU host controller GGSL driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (ggsl_driver_instruction_get_info(pShaderDesc, pInstructionInfo)!=0){
		printf("failed to get GPU host controller GGSL shader instruction info\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
