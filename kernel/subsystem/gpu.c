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
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)kmalloc(sizeof(struct gpu_driver_desc));
	if (!pDriverDesc){
		printf("failed to allocate GPU driver descriptor\r\n");
		return -1;
	}
	memset((void*)pDriverDesc, 0, sizeof(struct gpu_driver_desc));
	uint64_t driverId = 0;
	if (subsystem_alloc_entry(gpuDriverSubsystemInfo.pSubsystemDesc, (unsigned char*)pDriverDesc, &driverId)!=0){
		printf("failed to allocate subsystem entry for GPU driver descriptor\r\n");
		kfree((void*)pDriverDesc);
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
	return 0;
}
int gpu_driver_unregister(uint64_t driverId){
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (subsystem_get_entry(gpuDriverSubsystemInfo.pSubsystemDesc, driverId, (uint64_t*)&pDriverDesc)!=0){
		printf("failed to get GPU driver descriptor subsystem entry\r\n");
		return -1;
	}
	if (subsystem_free_entry(gpuDriverSubsystemInfo.pSubsystemDesc, driverId)!=0){
		printf("failed to free GPU driver descriptor subsystem entry\r\n");
		kfree((void*)pDriverDesc);
		return -1;
	}
	if (pDriverDesc==gpuDriverSubsystemInfo.pLastDriverDesc)
		gpuDriverSubsystemInfo.pLastDriverDesc = pDriverDesc->pBlink;
	if (pDriverDesc==gpuDriverSubsystemInfo.pFirstDriverDesc)
		gpuDriverSubsystemInfo.pFirstDriverDesc = pDriverDesc->pFlink;
	kfree((void*)pDriverDesc);
	return 0;
}
int gpu_driver_get_desc(uint64_t driverId, struct gpu_driver_desc** ppDriverDesc){
	if (!ppDriverDesc)
		return -1;
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (subsystem_get_entry(gpuDriverSubsystemInfo.pSubsystemDesc, driverId, (uint64_t*)&pDriverDesc)!=0)
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
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)kmalloc(sizeof(struct gpu_desc));
	if (!pGpuDesc)
		return -1;
	memset((void*)pGpuDesc, 0, sizeof(struct gpu_desc));
	uint64_t gpuId = 0;
	if (subsystem_alloc_entry(gpuSubsystemInfo.pSubsystemDesc, (unsigned char*)pGpuDesc, &gpuId)!=0){
		printf("failed to allocate subsystem entry for GPU descriptor\r\n");
		kfree((void*)pGpuDesc);
		return -1;
	}	
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
	return 0;
}
int gpu_unregister(uint64_t gpuId){
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (subsystem_get_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId, (uint64_t*)&pGpuDesc)!=0)
		return -1;
	if (subsystem_free_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId)!=0){
		kfree((void*)pGpuDesc);
		return -1;
	}
	if (pGpuDesc==gpuSubsystemInfo.pFirstGpuDesc)
		gpuSubsystemInfo.pFirstGpuDesc = pGpuDesc->pFlink;
	if (pGpuDesc==gpuSubsystemInfo.pLastGpuDesc)
		gpuSubsystemInfo.pLastGpuDesc = pGpuDesc->pBlink;
	kfree((void*)pGpuDesc);
	return 0;
}
int gpu_get_desc(uint64_t gpuId, struct gpu_desc** ppGpuDesc){
	if (!ppGpuDesc)
		return -1;
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (subsystem_get_entry(gpuSubsystemInfo.pSubsystemDesc, gpuId, (uint64_t*)&pGpuDesc)!=0)
		return -1;
	*ppGpuDesc = pGpuDesc;
	return 0;
}
int gpu_monitor_register(uint64_t driverId, struct gpu_monitor_info monitorInfo, uint64_t* pMonitorId){
	if (!pMonitorId)
		return -1;
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)kmalloc(sizeof(struct gpu_monitor_desc));
	if (!pMonitorDesc)
		return -1;
	memset((void*)pMonitorDesc, 0, sizeof(struct gpu_monitor_desc));
	uint64_t monitorId = 0;
	if (subsystem_alloc_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, (unsigned char*)pMonitorDesc, &monitorId)!=0){
		printf("failed to allocate subsystem entry for monitor descriptor\r\n");
		kfree((void*)pMonitorDesc);
		return -1;
	}
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
	return 0;
}
int gpu_monitor_unregister(uint64_t monitorId){
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (subsystem_get_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, monitorId, (uint64_t*)&pMonitorDesc)!=0)
		return -1;
	if (subsystem_free_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, monitorId)!=0){
		kfree((void*)pMonitorDesc);
		return -1;
	}
	if (pMonitorDesc==gpuMonitorSubsystemInfo.pFirstMonitorDesc)
		gpuMonitorSubsystemInfo.pFirstMonitorDesc = pMonitorDesc->pFlink;
	if (pMonitorDesc==gpuMonitorSubsystemInfo.pLastMonitorDesc)
		gpuMonitorSubsystemInfo.pLastMonitorDesc = pMonitorDesc->pBlink;
	kfree((void*)pMonitorDesc);
	return 0;
}
int gpu_monitor_get_desc(uint64_t monitorId, struct gpu_monitor_desc** ppMonitorDesc){
	if (!ppMonitorDesc||!gpuMonitorSubsystemInfo.pSubsystemDesc)
		return -1;
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (subsystem_get_entry(gpuMonitorSubsystemInfo.pSubsystemDesc, monitorId, (uint64_t*)&pMonitorDesc)!=0)
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
	if (pDriverDesc->vtable.push(monitorId, rect)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int gpu_clear(uint64_t monitorId, struct fvec4_32 color){
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
	if (!pDriverDesc->driverInfo.features.acceleration){
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.clear){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.clear(monitorId, color)!=0){
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
