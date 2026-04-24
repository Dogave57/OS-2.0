#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/serial.h"
#include "cpu/mutex.h"
#include "panic.h"
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
static struct gpu_driver_subsystem_info gpuDriverSubsystemInfo = {0};
static struct gpu_subsystem_info gpuSubsystemInfo = {0};
static struct gpu_monitor_subsystem_info gpuMonitorSubsystemInfo = {0};
int gpu_subsystem_init(void){
	if (subsystem_init(&gpuDriverSubsystemInfo.pSubsystemDesc, 16384)!=0)
		return -1;
	if (subsystem_init(&gpuSubsystemInfo.pSubsystemDesc, 16384)!=0)
		return -1;
	if (subsystem_init(&gpuMonitorSubsystemInfo.pSubsystemDesc, 16384)!=0){
		printf("failed to initialize monitor subsystem\r\n");
		return -1;
	}
	return 0;
}
int gpu_driver_register(struct gpu_driver_vtable vtable, struct gpu_driver_info driverInfo, uint64_t* pDriverId){
	if (!pDriverId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)kmalloc(sizeof(struct gpu_driver_desc));
	if (!pDriverDesc){
		printf("failed to allocate GPU driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pDriverDesc, 0, sizeof(struct gpu_driver_desc));
	uint64_t driverId = 0;
	if (subsystem_alloc_entry(gpuDriverSubsystemInfo.pSubsystemDesc, (unsigned char*)pDriverDesc, &driverId)!=0){
		printf("failed to allocate subsystem entry for GPU driver descriptor\r\n");
		kfree((void*)pDriverDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pDriverDesc->vtable = vtable;
	pDriverDesc->driverInfo = driverInfo;
	pDriverDesc->driverId = driverId;
	if (!gpuDriverSubsystemInfo.pFirstDriverDesc)
		gpuDriverSubsystemInfo.pFirstDriverDesc = pDriverDesc;
	if (gpuDriverSubsystemInfo.pLastDriverDesc){
		gpuDriverSubsystemInfo.pLastDriverDesc->pFlink = pDriverDesc;
		pDriverDesc->pBlink = gpuDriverSubsystemInfo.pLastDriverDesc;
	}
	gpuDriverSubsystemInfo.pLastDriverDesc = pDriverDesc;
	*pDriverId = driverId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_driver_unregister(uint64_t driverId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (subsystem_get_entry(gpuDriverSubsystemInfo.pSubsystemDesc, driverId, (uint64_t*)&pDriverDesc)!=0){
		printf("failed to get GPU driver descriptor subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(gpuDriverSubsystemInfo.pSubsystemDesc, driverId)!=0){
		printf("failed to free GPU driver descriptor subsystem entry\r\n");
		kfree((void*)pDriverDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc==gpuDriverSubsystemInfo.pLastDriverDesc)
		gpuDriverSubsystemInfo.pLastDriverDesc = pDriverDesc->pBlink;
	if (pDriverDesc==gpuDriverSubsystemInfo.pFirstDriverDesc)
		gpuDriverSubsystemInfo.pFirstDriverDesc = pDriverDesc->pFlink;
	kfree((void*)pDriverDesc);
	mutex_unlock(&mutex);
	return 0;
}
int gpu_driver_get_desc(uint64_t driverId, struct gpu_driver_desc** ppDriverDesc){
	if (!ppDriverDesc)
		return -1;
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (subsystem_get_entry(gpuDriverSubsystemInfo.pSubsystemDesc, driverId, (uint64_t*)&pDriverDesc)!=0)
		return -1;
	if (!pDriverDesc)
		return -1;
	*ppDriverDesc = pDriverDesc;
	return 0;
}
int gpu_driver_get_first_desc(struct gpu_driver_desc** ppDriverDesc){
	if (!ppDriverDesc)
		return -1;
	struct gpu_driver_desc* pDriverDesc = gpuDriverSubsystemInfo.pFirstDriverDesc;
	*ppDriverDesc = pDriverDesc;
	return 0;
}
int gpu_register(uint64_t driverId, struct gpu_info gpuInfo, uint64_t* pGpuId){
	if (!pGpuId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)kmalloc(sizeof(struct gpu_desc));
	if (!pGpuDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pGpuDesc, 0, sizeof(struct gpu_desc));
	uint64_t gpuId = 0;
	if (subsystem_alloc_entry(gpuSubsystemInfo.pSubsystemDesc, (unsigned char*)pGpuDesc, &gpuId)!=0){
		printf("failed to allocate subsystem entry for GPU descriptor\r\n");
		kfree((void*)pGpuDesc);
		mutex_unlock(&mutex);
		return -1;
	}	
	struct subsystem_desc* pCmdContextSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pCmdContextSubsystemDesc, GPU_MAX_CMD_CONTEXT_COUNT)!=0){
		printf("failed to initialize GPU command context subsystem\r\n");
		subsystem_free_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId);
		kfree((void*)pGpuDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct subsystem_desc* pObjectSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pObjectSubsystemDesc, GPU_MAX_OBJECT_COUNT)!=0){
		printf("failed to initialize GPU object subsystem\r\n");
		subsystem_free_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId);
		kfree((void*)pGpuDesc);	
		subsystem_deinit(pCmdContextSubsystemDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct subsystem_desc* pContextSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pContextSubsystemDesc, GPU_MAX_CONTEXT_COUNT)!=0){
		printf("failed to initialize GPU context subsystem\r\n");
		subsystem_free_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId);
		kfree((void*)pGpuDesc);
		subsystem_deinit(pObjectSubsystemDesc);
		subsystem_deinit(pCmdContextSubsystemDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct subsystem_desc* pResourceSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pResourceSubsystemDesc, GPU_MAX_RESOURCE_COUNT)!=0){
		printf("failed to initialize GPU resource subsystem\r\n");
		subsystem_free_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId);
		kfree((void*)pGpuDesc);
		subsystem_deinit(pObjectSubsystemDesc);
		subsystem_deinit(pContextSubsystemDesc);
		subsystem_deinit(pCmdContextSubsystemDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	struct subsystem_desc* pResourceBlobSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pResourceBlobSubsystemDesc, GPU_MAX_RESOURCE_BLOB_COUNT)!=0){
		printf("failed to initialize GPU resource blob subsystem\r\n");
		subsystem_deinit(pResourceSubsystemDesc);
		subsystem_deinit(pObjectSubsystemDesc);
		subsystem_deinit(pContextSubsystemDesc);
		subsystem_deinit(pCmdContextSubsystemDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pGpuDesc->cmdContextSubsystemInfo.pSubsystemDesc = pCmdContextSubsystemDesc;
	pGpuDesc->objectSubsystemInfo.pSubsystemDesc = pObjectSubsystemDesc;
	pGpuDesc->contextSubsystemInfo.pSubsystemDesc = pContextSubsystemDesc;
	pGpuDesc->resourceSubsystemInfo.pSubsystemDesc = pResourceSubsystemDesc;
	pGpuDesc->resourceBlobSubsystemInfo.pSubsystemDesc = pResourceBlobSubsystemDesc;
	pGpuDesc->driverId = driverId;
	pGpuDesc->gpuId = gpuId;
	pGpuDesc->gpuInfo = gpuInfo;
	if (!gpuSubsystemInfo.pFirstGpuDesc)
		gpuSubsystemInfo.pFirstGpuDesc = pGpuDesc;
	if (gpuSubsystemInfo.pLastGpuDesc){
		gpuSubsystemInfo.pLastGpuDesc->pFlink = pGpuDesc;
		pGpuDesc->pBlink = gpuSubsystemInfo.pLastGpuDesc;
	}
	gpuSubsystemInfo.pLastGpuDesc = pGpuDesc;
	*pGpuId = gpuId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_unregister(uint64_t gpuId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (subsystem_get_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId, (uint64_t*)&pGpuDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId)!=0){
		kfree((void*)pGpuDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pGpuDesc==gpuSubsystemInfo.pFirstGpuDesc)
		gpuSubsystemInfo.pFirstGpuDesc = pGpuDesc->pFlink;
	if (pGpuDesc==gpuSubsystemInfo.pLastGpuDesc)
		gpuSubsystemInfo.pLastGpuDesc = pGpuDesc->pBlink;
	if (pGpuDesc->cmdContextSubsystemInfo.pSubsystemDesc)
		subsystem_deinit(pGpuDesc->cmdContextSubsystemInfo.pSubsystemDesc);
	if (pGpuDesc->objectSubsystemInfo.pSubsystemDesc)
		subsystem_deinit(pGpuDesc->objectSubsystemInfo.pSubsystemDesc);
	if (pGpuDesc->contextSubsystemInfo.pSubsystemDesc)
		subsystem_deinit(pGpuDesc->contextSubsystemInfo.pSubsystemDesc);
	if (pGpuDesc->resourceSubsystemInfo.pSubsystemDesc)
		subsystem_deinit(pGpuDesc->resourceSubsystemInfo.pSubsystemDesc);
	if (pGpuDesc->resourceBlobSubsystemInfo.pSubsystemDesc)
		subsystem_deinit(pGpuDesc->resourceBlobSubsystemInfo.pSubsystemDesc);
	kfree((void*)pGpuDesc);
	mutex_unlock(&mutex);
	return 0;
}
int gpu_get_desc(uint64_t gpuId, struct gpu_desc** ppGpuDesc){
	if (!ppGpuDesc)
		return -1;
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (subsystem_get_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId, (uint64_t*)&pGpuDesc)!=0)
		return -1;
	if (!pGpuDesc)
		return -1;
	*ppGpuDesc = pGpuDesc;
	return 0;
}
int gpu_monitor_register(uint64_t gpuId, struct gpu_monitor_info monitorInfo, uint64_t* pMonitorId){
	if (!pMonitorId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t driverId = pGpuDesc->driverId;
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)kmalloc(sizeof(struct gpu_monitor_desc));
	if (!pMonitorDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pMonitorDesc, 0, sizeof(struct gpu_monitor_desc));
	uint64_t monitorId = 0;
	if (subsystem_alloc_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, (unsigned char*)pMonitorDesc, &monitorId)!=0){
		printf("failed to allocate subsystem entry for monitor descriptor\r\n");
		kfree((void*)pMonitorDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pMonitorDesc->gpuId = gpuId;
	pMonitorDesc->driverId = driverId;
	pMonitorDesc->monitorId = monitorId;
	pMonitorDesc->monitorInfo = monitorInfo;
	if (gpuMonitorSubsystemInfo.pLastMonitorDesc){
		pMonitorDesc->pBlink = gpuMonitorSubsystemInfo.pLastMonitorDesc;
		gpuMonitorSubsystemInfo.pLastMonitorDesc->pFlink = pMonitorDesc;
	}
	if (!gpuMonitorSubsystemInfo.pFirstMonitorDesc)
		gpuMonitorSubsystemInfo.pFirstMonitorDesc = pMonitorDesc;
	gpuMonitorSubsystemInfo.pLastMonitorDesc = pMonitorDesc;
	*pMonitorId = monitorId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_monitor_unregister(uint64_t monitorId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (subsystem_get_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, monitorId, (uint64_t*)&pMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, monitorId)!=0){
		kfree((void*)pMonitorDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pMonitorDesc==gpuMonitorSubsystemInfo.pFirstMonitorDesc)
		gpuMonitorSubsystemInfo.pFirstMonitorDesc = pMonitorDesc->pFlink;
	if (pMonitorDesc==gpuMonitorSubsystemInfo.pLastMonitorDesc)
		gpuMonitorSubsystemInfo.pLastMonitorDesc = pMonitorDesc->pBlink;
	kfree((void*)pMonitorDesc);
	mutex_unlock(&mutex);
	return 0;
}
int gpu_monitor_get_desc(uint64_t monitorId, struct gpu_monitor_desc** ppMonitorDesc){
	if (!ppMonitorDesc||!gpuMonitorSubsystemInfo.pSubsystemDesc)
		return -1;
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (subsystem_get_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, monitorId, (uint64_t*)&pMonitorDesc)!=0)
		return -1;
	if (!pMonitorDesc)
		return -1;
	*ppMonitorDesc = pMonitorDesc;
	return 0;
}
int gpu_monitor_get_info(uint64_t monitorId, struct gpu_monitor_info* pMonitorInfo){
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0)
		return -1;
	struct gpu_monitor_info monitorInfo = pMonitorDesc->monitorInfo;
	*pMonitorInfo = monitorInfo;
	return 0;
}
int gpu_monitor_exists(uint64_t monitorId){
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0)
		return -1;
	return (pMonitorDesc) ? 0 : -1;
}
int gpu_cmd_context_register(uint64_t gpuId, uint64_t* pCmdContextId){
	if (!pCmdContextId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pCmdContextDesc = (struct gpu_cmd_context_desc*)kmalloc(sizeof(struct gpu_cmd_context_desc));
	if (!pCmdContextDesc){
		printf("failed to allocate GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCmdContextDesc, 0, sizeof(struct gpu_cmd_context_desc));
	uint64_t cmdContextId = 0;
	if (subsystem_alloc_entry(pGpuDesc->cmdContextSubsystemInfo.pSubsystemDesc, (unsigned char*)pCmdContextDesc, &cmdContextId)!=0){
		printf("failed to allocate GPU host controller command context subsystem entry\r\n");	
		mutex_unlock(&mutex);
		return -1;	
	}
	pCmdContextDesc->cmdContextId = cmdContextId;
	if (!pGpuDesc->cmdContextSubsystemInfo.pFirstCmdContextDesc){
		pGpuDesc->cmdContextSubsystemInfo.pFirstCmdContextDesc = pCmdContextDesc;
	}
	if (pGpuDesc->cmdContextSubsystemInfo.pLastCmdContextDesc){
		pGpuDesc->cmdContextSubsystemInfo.pLastCmdContextDesc->pFlink = pCmdContextDesc;
		pGpuDesc->cmdContextSubsystemInfo.pLastCmdContextDesc = pCmdContextDesc;
	}
	*pCmdContextId = cmdContextId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_cmd_context_unregister(uint64_t gpuId, uint64_t cmdContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pCmdContextDesc->pBlink)
		pCmdContextDesc->pBlink->pFlink = pCmdContextDesc->pFlink;
	if (pCmdContextDesc->pFlink)
		pCmdContextDesc->pFlink->pBlink = pCmdContextDesc->pBlink;
	if (subsystem_free_entry(pGpuDesc->cmdContextSubsystemInfo.pSubsystemDesc, cmdContextId)!=0){
		printf("failed to free GPU host controller command context descriptor subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	kfree((void*)pCmdContextDesc);
	mutex_unlock(&mutex);
	return 0;
}
int gpu_cmd_context_get_desc(uint64_t gpuId, uint64_t cmdContextId, struct gpu_cmd_context_desc** ppCmdContextDesc){
	if (!ppCmdContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (subsystem_get_entry(pGpuDesc->cmdContextSubsystemInfo.pSubsystemDesc, cmdContextId, (uint64_t*)&pCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pCmdContextDesc){
		printf("GPU host controller command context descriptor ID not linked to any command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	*ppCmdContextDesc = pCmdContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_object_register(uint64_t gpuId, uint64_t objectType, uint64_t* pObjectId){
	if (!pObjectId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_object_desc* pObjectDesc = (struct gpu_object_desc*)kmalloc(sizeof(struct gpu_object_desc));
	if (!pObjectDesc){
		printf("failed to allocate GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pObjectDesc, 0, sizeof(struct gpu_object_desc));
	uint64_t objectId = 0;
	if (subsystem_alloc_entry(pGpuDesc->objectSubsystemInfo.pSubsystemDesc, (unsigned char*)pObjectDesc, &objectId)!=0){
		printf("failed to allocate GPU host controller object subsystem entry\r\n");
		kfree((void*)pObjectDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pObjectDesc->objectId = objectId;
	if (!pGpuDesc->objectSubsystemInfo.pFirstObjectDesc){
		pGpuDesc->objectSubsystemInfo.pFirstObjectDesc = pObjectDesc;
	}
	if (pGpuDesc->objectSubsystemInfo.pLastObjectDesc){
		pGpuDesc->objectSubsystemInfo.pLastObjectDesc->pFlink = pObjectDesc;
		pObjectDesc->pBlink = pGpuDesc->objectSubsystemInfo.pLastObjectDesc;	
	}
	pGpuDesc->objectSubsystemInfo.pLastObjectDesc = pObjectDesc;
	*pObjectId = objectId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_object_unregister(uint64_t gpuId, uint64_t objectId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_object_desc* pObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuId, objectId, &pObjectDesc)!=0){
		printf("failed to get GPU host controller object descriptor\r\n");
		kfree((void*)pObjectDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(pGpuDesc->objectSubsystemInfo.pSubsystemDesc, objectId)!=0){
		printf("failed to free GPU host controller object subsystem entry\r\n");
		kfree((void*)pObjectDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pObjectDesc->pBlink)
		pObjectDesc->pBlink->pFlink = pObjectDesc->pFlink;
	if (pObjectDesc->pFlink)
		pObjectDesc->pFlink->pBlink = pObjectDesc->pBlink;
	kfree((void*)pObjectDesc);
	mutex_unlock(&mutex);
	return 0;
}
int gpu_object_get_desc(uint64_t gpuId, uint64_t objectId, struct gpu_object_desc** ppObjectDesc){
	if (!ppObjectDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_object_desc* pObjectDesc = (struct gpu_object_desc*)0x0;
	if (subsystem_get_entry(pGpuDesc->objectSubsystemInfo.pSubsystemDesc, objectId, (uint64_t*)&pObjectDesc)!=0){
		printf("failed to get object subsystem entry with ID: %d\r\n", objectId);
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pObjectDesc){
		printf("object subsystem entry not linked with object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	*ppObjectDesc = pObjectDesc;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_context_register(uint64_t gpuId, uint64_t* pContextId){
	if (!pContextId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)kmalloc(sizeof(struct gpu_context_desc));
	if (!pContextDesc){
		printf("failed to allocate GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pContextDesc, 0, sizeof(struct gpu_context_desc));
	uint64_t contextId = 0;
	if (subsystem_alloc_entry(pGpuDesc->contextSubsystemInfo.pSubsystemDesc, (unsigned char*)pContextDesc, &contextId)!=0){
		printf("failed to allocate GPU host controller context subsystem entry for context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pContextDesc->contextId = contextId;
	if (!pGpuDesc->contextSubsystemInfo.pFirstContextDesc){
		pGpuDesc->contextSubsystemInfo.pFirstContextDesc = pContextDesc;
	}
	if (pGpuDesc->contextSubsystemInfo.pLastContextDesc){
		pGpuDesc->contextSubsystemInfo.pLastContextDesc->pFlink = pContextDesc;
		pContextDesc->pBlink = pGpuDesc->contextSubsystemInfo.pLastContextDesc;
	}
	pGpuDesc->contextSubsystemInfo.pLastContextDesc = pContextDesc;
	*pContextId = contextId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_context_unregister(uint64_t gpuId, uint64_t contextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)0x0;
	if (subsystem_get_entry(pGpuDesc->contextSubsystemInfo.pSubsystemDesc, contextId, (uint64_t*)&pContextDesc)!=0){
		printf("failed to get GPU host controller context subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(pGpuDesc->contextSubsystemInfo.pSubsystemDesc, contextId)!=0){
		printf("failed to free GPU host controller context subsystem entry\r\n");
		kfree((void*)pContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pContextDesc->pBlink){
		pContextDesc->pBlink->pFlink = pContextDesc->pFlink;
	}
	if (pContextDesc->pFlink){
		pContextDesc->pFlink->pBlink = pContextDesc->pBlink;
	}
	if (pContextDesc==pGpuDesc->contextSubsystemInfo.pFirstContextDesc){
		pGpuDesc->contextSubsystemInfo.pFirstContextDesc = pContextDesc->pFlink;
	}
	if (pContextDesc==pGpuDesc->contextSubsystemInfo.pLastContextDesc){
		pGpuDesc->contextSubsystemInfo.pLastContextDesc = pContextDesc->pBlink;
	}
	kfree((void*)pContextDesc);
	mutex_unlock(&mutex);
	return 0;
}
int gpu_context_get_desc(uint64_t gpuId, uint64_t contextId, struct gpu_context_desc** ppContextDesc){
	if (!ppContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)0x0;
	if (subsystem_get_entry(pGpuDesc->contextSubsystemInfo.pSubsystemDesc, contextId, (uint64_t*)&pContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor from context subsystem\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pContextDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	*ppContextDesc = pContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_cmd_context_init(uint64_t gpuId, uint64_t commandListSize, uint64_t* pCmdContextId){
	if (!pCmdContextId)
		return -1;
	if (!commandListSize)
		commandListSize = GPU_DEFAULT_CMD_LIST_SIZE;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.cmdContextInit){
		printf("GPU host controller driver lacks ability of initializing GPU host controller command contexts\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t cmdContextId = 0;
	if (gpu_cmd_context_register(gpuId, &cmdContextId)!=0){
		printf("failed to register GPU host controller command context into command context subsystem\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		gpu_cmd_context_unregister(gpuId, cmdContextId);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.cmdContextInit(gpuId, cmdContextId, commandListSize)!=0){
		printf("failed to initialize GPU host controller command context\r\n");
		gpu_cmd_context_unregister(gpuId, cmdContextId);
		mutex_unlock(&mutex);
		return -1;
	}
	pCmdContextDesc->commandListSize = commandListSize;
	*pCmdContextId = cmdContextId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_cmd_context_deinit(uint64_t gpuId, uint64_t cmdContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.cmdContextDeinit){
		printf("GPU host controller lacks ability of deinitializing command context descriptors\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_cmd_context_unregister(gpuId, cmdContextId)!=0){
		printf("failed to unregister GPU host controller command context descriptor from command context subsystem\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_cmd_context_reset(uint64_t gpuId, uint64_t cmdContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.cmdContextReset){
		printf("GPU host controller driver lacks ability of resetting command contexts\r\n");
		mutex_unlock(&mutex);
		return -1;	
	}
	if (pDriverDesc->vtable.cmdContextReset(gpuId, cmdContextId)!=0){
		printf("GPU host controller driver failed to reset command context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_cmd_context_push_cmd(uint64_t gpuId, uint64_t cmdContextId, struct gpu_cmd_info_header* pCommandInfo){
	if (!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pCmdContextDesc)!=0){
		printf("failed to get GPU host contorller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.cmdContextPushCmd){
		printf("GPU host controller driver lacks ability to push commands to command context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.cmdContextPushCmd(gpuId, cmdContextId, pCommandInfo)!=0){
		printf("GPU host controller driver failed to push command to command context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_cmd_context_submit(uint64_t gpuId, uint64_t contextId, uint64_t cmdContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pContextDesc = (struct gpu_context_desc*)0x0;
	if (gpu_context_get_desc(gpuId, contextId, &pContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.cmdContextSubmit){
		printf("GPU host controller does lacks ability of submitting command lists to GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.cmdContextSubmit(gpuId, contextId, cmdContextId)!=0){
		printf("failed to submit command list to GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_resource_register(uint64_t gpuId, struct gpu_resource_info resourceInfo, uint64_t* pResourceId){
	if (!pResourceId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pResourceDesc = (struct gpu_resource_desc*)kmalloc(sizeof(struct gpu_resource_desc));
	if (!pResourceDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pResourceDesc, 0, sizeof(struct gpu_resource_desc));
	pResourceDesc->gpuId = gpuId;
	pResourceDesc->resourceInfo = resourceInfo;
	uint64_t resourceId = 0;
	if (subsystem_alloc_entry(pGpuDesc->resourceSubsystemInfo.pSubsystemDesc, (unsigned char*)pResourceDesc, &resourceId)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	pResourceDesc->resourceId = resourceId;
	if (!pGpuDesc->resourceSubsystemInfo.pFirstResourceDesc){
		pGpuDesc->resourceSubsystemInfo.pFirstResourceDesc = pResourceDesc;
		pGpuDesc->resourceSubsystemInfo.pLastResourceDesc = pResourceDesc;
	}
	if (pGpuDesc->resourceSubsystemInfo.pLastResourceDesc){
		pGpuDesc->resourceSubsystemInfo.pLastResourceDesc->pFlink = pResourceDesc;
		pResourceDesc->pBlink = pGpuDesc->resourceSubsystemInfo.pLastResourceDesc;
	}
	pGpuDesc->resourceSubsystemInfo.pLastResourceDesc = pResourceDesc;
	*pResourceId = resourceId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_resource_unregister(uint64_t gpuId, uint64_t resourceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pResourceDesc = (struct gpu_resource_desc*)0x0;
	if (subsystem_get_entry(pGpuDesc->resourceSubsystemInfo.pSubsystemDesc, resourceId, (uint64_t*)&pResourceDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(pGpuDesc->resourceSubsystemInfo.pSubsystemDesc, resourceId)!=0){
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pResourceDesc->pBlink)
		pResourceDesc->pBlink->pFlink = pResourceDesc->pFlink;
	if (pResourceDesc->pFlink)
		pResourceDesc->pFlink->pBlink = pResourceDesc->pBlink;
	if (pResourceDesc==pGpuDesc->resourceSubsystemInfo.pFirstResourceDesc){
		pGpuDesc->resourceSubsystemInfo.pFirstResourceDesc = pResourceDesc->pFlink;
	}
	if (pResourceDesc==pGpuDesc->resourceSubsystemInfo.pLastResourceDesc){
		pGpuDesc->resourceSubsystemInfo.pLastResourceDesc = pResourceDesc->pBlink;
	}
	kfree((void*)pResourceDesc);
	mutex_unlock(&mutex);
	return 0;
}
int gpu_resource_get_desc(uint64_t gpuId, uint64_t resourceId, struct gpu_resource_desc** ppResourceDesc){
	if (!ppResourceDesc)
		return -1;
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0)
		return -1;
	struct gpu_resource_desc* pResourceDesc = (struct gpu_resource_desc*)0x0;
	if (subsystem_get_entry(pGpuDesc->resourceSubsystemInfo.pSubsystemDesc, resourceId, (uint64_t*)&pResourceDesc)!=0)
		return -1;
	if (!pResourceDesc)
		return -1;
	*ppResourceDesc = pResourceDesc;
	return 0;
}
int gpu_read_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pPixel){
	if (!pPixel)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.readPixel){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.readPixel(monitorId, position, pPixel)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_write_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8 pixel){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0){
		serial_print(0, "failed to get monitor descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		serial_print(0, "failed to get driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.writePixel){
		serial_print(0, "invalid write pixel routine\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.writePixel(monitorId, position, pixel)!=0){
		serial_print(0, "subsystem write pixel routine failure\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_sync(uint64_t monitorId, struct uvec4 rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.sync){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.sync(monitorId, rect)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_commit(uint64_t monitorId, struct uvec4 rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.commit){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.commit(monitorId, rect)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_push(uint64_t monitorId, struct uvec4 rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.push){
		printf("GPU host controller driver lacks ability of pushing pixel data to the framebuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.push(monitorId, rect)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_set_scanout(uint64_t monitorId, uint64_t resourceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pMonitorDesc)!=0){
		printf("failed to get GPU host controller monitor descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.setScanout){
		printf("GPU host controller driver lacks ability of setting scanouts\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.setScanout(monitorId, resourceId)!=0){
		printf("GPU host controller driver failed to set monitor %d scanout to resource %d\r\n", monitorId, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_object_create(uint64_t gpuId, uint64_t contextId, struct gpu_create_object_info* pCreateObjectInfo, uint64_t* pObjectId){
	if (!pCreateObjectInfo||!pObjectId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.objectCreate){
		printf("GPU host controller drivers lacks ability of creating objects\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t objectType = pCreateObjectInfo->header.objectType;
	uint64_t objectId = 0;
	if (gpu_object_register(gpuId, objectType, &objectId)!=0){
		printf("failed to register GPU host controller object into GPU host controller object subsystem\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.objectCreate(gpuId, contextId, objectId, pCreateObjectInfo)!=0){
		printf("GPU host controller driver failed to create object with type 0x%x\r\n", objectType);
		mutex_unlock(&mutex);
		return -1;
	}
	*pObjectId = objectId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_object_delete(uint64_t gpuId, uint64_t objectId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.objectDelete){
		printf("GPU host controller drivers lacks ability of deleting objects\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.objectDelete(gpuId, objectId)!=0){
		printf("GPU host controller driver failed to delete object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_object_unregister(gpuId, objectId)!=0){
		printf("failed to unregister GPU host controller object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_object_bind(uint64_t gpuId, uint64_t objectId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.objectBind){
		printf("GPU host controller driver lacks ability of binding objects\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.objectBind(gpuId, objectId)!=0){
		printf("failed to bind virtual I/O GPU host controller object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_context_create(uint64_t gpuId, uint64_t* pContextId){
	if (!pContextId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.contextCreate){
		printf("GPU host controller driver has lack of ability to create contexts\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t contextId = 0;
	if (gpu_context_register(gpuId, &contextId)!=0){
		printf("failed to register GPU host controller context in GPU subsystem\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.contextCreate(gpuId, contextId)!=0){
		printf("failed to create GPU host controller context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	*pContextId = contextId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_context_delete(uint64_t gpuId, uint64_t contextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.contextDelete){
		printf("GPU host controller driver has lack of ability to delete contexts\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.contextDelete(gpuId, contextId)!=0){
		printf("failed to delete GPU host controller context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_context_unregister(gpuId, contextId)!=0){
		printf("failed to unregister GPU host controller context from context subsystem\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_context_attach_resource(uint64_t gpuId, uint64_t contextId, uint64_t resourceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.contextAttachResource){
		printf("GPU host contorller driver lacks ability of attaching resources to contexts\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.contextAttachResource(gpuId, contextId, resourceId)!=0){
		printf("failed to attach GPU host controller context with resource\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_resource_create(uint64_t gpuId, struct gpu_create_resource_info createResourceInfo, uint64_t* pResourceId){
	if (!pResourceId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.resourceCreate){
		printf("GPU host controller driver lacks support of resource creation\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t resourceId = 0;
	if (gpu_resource_register(gpuId, createResourceInfo.resourceInfo, &resourceId)!=0){
		printf("failed to register resource into resource subsystem\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.resourceCreate(gpuId, resourceId, createResourceInfo)!=0){
		printf("failed to create GPU host controller resource\r\n");
		gpu_resource_unregister(gpuId, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}
	*pResourceId = resourceId;
	mutex_unlock(&mutex);
	return 0;
}
int gpu_resource_delete(uint64_t gpuId, uint64_t resourceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.resourceDelete){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.resourceDelete(gpuId, resourceId)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (gpu_resource_unregister(gpuId, resourceId)!=0){
		printf("failed to unregister GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;	
}
int gpu_resource_attach_backing(uint64_t gpuId, uint64_t resourceId, unsigned char* pBuffer, uint64_t bufferSize){
	if (!pBuffer||!bufferSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.resourceAttachBacking){
		printf("GPU host controller driver lacks ability of attaching physical pages to resources\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.resourceAttachBacking(gpuId, resourceId, pBuffer, bufferSize)!=0){
		printf("failed to attach physical pages to GPU host controller resource\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_resource_flush(uint64_t gpuId, uint64_t resourceId, struct gpu_rect rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.resourceFlush){
		printf("GPU host controller driver lacks ability to flush physical pages to resources\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.resourceFlush(gpuId, resourceId, rect)!=0){
		printf("failed to flush resource data to GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_transfer_to_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_to_device_info transferToDeviceInfo){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.transferToDevice){
		printf("GPU host controller driver lacks ability to send resource data to GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.transferToDevice(gpuId, resourceId, transferToDeviceInfo)!=0){
		printf("failed to transfer resource data to GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_transfer_from_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_from_device_info transferFromDeviceInfo){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pGpuDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.transferFromDevice){
		printf("GPU host controller driver lacks ability to transfer resouce data from GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.transferFromDevice(gpuId, resourceId, transferFromDeviceInfo)!=0){
		printf("failed to transfer GPU host controller resource data from GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_panic(void){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_driver_desc* pCurrentDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_first_desc(&pCurrentDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	while (pCurrentDriverDesc){
		if (!pCurrentDriverDesc->vtable.panic){
			pCurrentDriverDesc = pCurrentDriverDesc->pFlink;
			continue;
		}
		if (pCurrentDriverDesc->vtable.panic(pCurrentDriverDesc->driverId)!=0){
			panic("failed to put GPU host controller driver into panic mode\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		pCurrentDriverDesc = pCurrentDriverDesc->pFlink;
	}
	mutex_unlock(&mutex);
	return 0;
}
