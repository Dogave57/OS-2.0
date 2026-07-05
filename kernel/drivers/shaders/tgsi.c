#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/mutex.h"
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
#include "drivers/shaders/tgsi.h"
struct tgsi_driver_info driverInfo = {0};
int tgsi_driver_init(void){
	static const unsigned char driverIdent[16] = TGSI_DRIVER_IDENT;
	struct gpu_shader_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct gpu_shader_driver_vtable));
	vtable.driverInit = tgsi_driver_subsystem_driver_init;
	vtable.driverDeinit = tgsi_driver_subsystem_driver_deinit;
	vtable.shaderInit = tgsi_driver_subsystem_shader_init;
	vtable.shaderDeinit = tgsi_driver_subsystem_shader_deinit;
	vtable.instructionListReset = tgsi_driver_subsystem_instruction_list_reset;
	vtable.instructionGetInfo = tgsi_driver_subsystem_instruction_get_info;
	struct gpu_shader_driver_info subsystemDriverInfo = {0};
	memset((void*)&subsystemDriverInfo, 0, sizeof(struct gpu_shader_driver_info));
	memcpy((void*)subsystemDriverInfo.ident, (void*)driverIdent, sizeof(uint8_t)*16);
	uint64_t driverId = 0x00;
	if (gpu_shader_driver_register(vtable, subsystemDriverInfo, &driverId)!=0){
		printf("failed to register GPU host controller TGSI shader driver\r\n");
		return -1;
	}
	driverInfo.driverId = driverId;
	driverInfo.vtable = vtable;
	return 0;
}
int tgsi_driver_deinit(void){
	if (gpu_shader_driver_unregister(driverInfo.driverId)!=0){
		printf("failed to unregister GPU host controller TGSI shader driver\r\n");
		return -1;
	}
	return 0;
}
int tgsi_driver_instruction_list_reset(struct tgsi_driver_shader_desc* pShaderDesc){
	if (!pShaderDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pShaderDesc->shaderCodeOffset = 0x00;
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_driver_subsystem_driver_init(uint64_t driverId){
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
int tgsi_driver_subsystem_driver_deinit(uint64_t driverId){
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
int tgsi_driver_subsystem_shader_init(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_shader_object_info* pCreateShaderObjectInfo){
	if (!pCreateShaderObjectInfo)
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
	struct tgsi_driver_shader_desc* pShaderDesc = (struct tgsi_driver_shader_desc*)kmalloc(sizeof(struct tgsi_driver_shader_desc));
	if (!pShaderDesc){
		printf("failed to allocate GPU host controller TGSI shader descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pShaderDesc, 0, sizeof(struct tgsi_driver_shader_desc));
	pShaderDesc->pShaderCode = pCreateShaderObjectInfo->pShaderCode;
	pShaderDesc->shaderCodeSize = pCreateShaderObjectInfo->shaderCodeSize;
	pShaderDesc->shaderType = pCreateShaderObjectInfo->shaderType;
	pShaderObjectDesc->shaderDriverExtra = (uint64_t)pShaderDesc;
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_driver_subsystem_shader_deinit(uint64_t gpuId, uint64_t contextId, uint64_t objectId){
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
	struct tgsi_driver_shader_desc* pShaderDesc = (struct tgsi_driver_shader_desc*)pShaderObjectDesc->shaderDriverExtra;
	if (!pShaderDesc){
		printf("GPU host controller shader descriptor not linked with GPU host controller TGSI driver shader descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	kfree((void*)pShaderDesc);
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_driver_subsystem_instruction_list_reset(uint64_t gpuId, uint64_t contextId, uint64_t objectId){
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
	struct tgsi_driver_shader_desc* pShaderDesc = (struct tgsi_driver_shader_desc*)pShaderObjectDesc->shaderDriverExtra;
	if (!pShaderDesc){
		printf("GPU host controller shader object descriptor not linked with GPU host controller TGSI shader driver shader descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (tgsi_driver_instruction_list_reset(pShaderDesc)!=0){
		printf("failed to reset GPU host controller TGSI shader object descriptor instruction list\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int tgsi_driver_subsystem_instruction_get_info(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_get_instruction_info* pGetInstructionInfo){
	if (!pGetInstructionInfo)
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
		return-1;
	}
	struct tgsi_driver_shader_desc* pShaderDesc = (struct tgsi_driver_shader_desc*)pShaderObjectDesc->shaderDriverExtra;
	if (!pShaderDesc){
		printf("GPU host controller shader object descriptor not linked with GPU host controller TGSI shader descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
