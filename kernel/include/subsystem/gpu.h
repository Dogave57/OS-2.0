#ifndef _SUBSYSTEM_GPU
#define _SUBSYSTEM_GPU
#include "math/vector.h"
typedef int(*gpuReadPixelFunc)(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pPixel);
typedef int(*gpuWritePixelFunc)(uint64_t monitorId, struct uvec2 position, struct uvec4_8 pixel);
typedef int(*gpuSyncFunc)(uint64_t monitorId, struct uvec4 rect);
typedef int(*gpuCommitFunc)(uint64_t monitorId, struct uvec4 rect);
typedef int(*gpuPushFunc)(uint64_t monitorId, struct uvec4 rect);
typedef int(*gpuPanicFunc)(uint64_t driverId);
struct gpu_resolution{
	uint32_t width;
	uint32_t height;	
};
struct gpu_info{
	uint64_t maxMonitorCount;
};
struct gpu_monitor_info{
	struct gpu_resolution resolution;
	volatile struct uvec4_8* pFramebuffer;
};
struct gpu_resource_desc{
	uint64_t resourceId;
	uint64_t resourceType;
	unsigned char* pBuffer;
	uint64_t bufferSize;
};
struct gpu_driver_vtable{
	gpuReadPixelFunc readPixel;
	gpuWritePixelFunc writePixel;
	gpuSyncFunc sync;
	gpuCommitFunc commit;
	gpuPushFunc push;
	gpuPanicFunc panic;
};
struct gpu_driver_desc{
	struct gpu_driver_vtable vtable;
	uint64_t driverId;
	uint64_t extra;
	struct gpu_driver_desc* pFlink;
	struct gpu_driver_desc* pBlink;
};
struct gpu_desc{
	uint64_t driverId;
	uint64_t gpuId;
	uint64_t extra;
	struct gpu_info gpuInfo;
	uint64_t monitorCount;
	struct gpu_desc* pFlink;
	struct gpu_desc* pBlink;
};
struct gpu_monitor_desc{
	uint64_t driverId;
	uint64_t monitorId;
	uint64_t extra;
	struct gpu_monitor_info monitorInfo;
	struct gpu_monitor_desc* pFlink;
	struct gpu_monitor_desc* pBlink;
};
struct gpu_driver_subsystem_info{	
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_driver_desc* pFirstDriverDesc;
	struct gpu_driver_desc* pLastDriverDesc;
};
struct gpu_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_desc* pFirstGpuDesc;
	struct gpu_desc* pLastGpuDesc;
};
struct gpu_monitor_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_monitor_desc* pFirstMonitorDesc;
	struct gpu_monitor_desc* pLastMonitorDesc;
};
int gpu_subsystem_init(void);
int gpu_driver_register(struct gpu_driver_vtable vtable, uint64_t* pDriverId);
int gpu_driver_unregister(uint64_t driverId);
int gpu_driver_get_desc(uint64_t driverId, struct gpu_driver_desc** ppDriverDesc);
int gpu_driver_get_first_desc(struct gpu_driver_desc** ppDriverDesc);
int gpu_register(uint64_t driverId, struct gpu_info gpuInfo, uint64_t* pGpuId);
int gpu_unregister(uint64_t gpuId);
int gpu_get_desc(uint64_t gpuId, struct gpu_desc** ppGpuDesc);
int gpu_monitor_register(uint64_t gpuId, struct gpu_monitor_info monitorInfo, uint64_t* pMonitorId);
int gpu_monitor_unregister(uint64_t monitorId);
int gpu_monitor_get_desc(uint64_t monitorId, struct gpu_monitor_desc** ppMonitorDesc);
int gpu_monitor_get_info(uint64_t monitorId, struct gpu_monitor_info* pMonitorInfo);
int gpu_monitor_exists(uint64_t monitorId);
int gpu_read_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pPixel);
int gpu_write_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8 pixel);
int gpu_sync(uint64_t monitorId, struct uvec4 rect);
int gpu_commit(uint64_t monitorId, struct uvec4 rect);
int gpu_push(uint64_t monitorId, struct uvec4 rect);
int gpu_panic(void);
#endif
