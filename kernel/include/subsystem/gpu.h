#ifndef _SUBSYSTEM_GPU
#define _SUBSYSTEM_GPU
#include "kernel_include.h"
#include "math/vector.h"
struct gpu_create_resource_info;
struct gpu_rect;
struct gpu_transfer_to_device_info;
struct gpu_transfer_from_device_info;
struct gpu_cmd_info_header;
typedef int(*gpuReadPixelFunc)(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pPixel);
typedef int(*gpuWritePixelFunc)(uint64_t monitorId, struct uvec2 position, struct uvec4_8 pixel);
typedef int(*gpuSyncFunc)(uint64_t monitorId, struct uvec4 rect);
typedef int(*gpuCommitFunc)(uint64_t monitorId, struct uvec4 rect);
typedef int(*gpuPushFunc)(uint64_t monitorId, struct uvec4 rect);
typedef int(*gpuCreateObjectFunc)(uint64_t gpuId, uint64_t objectType, uint64_t* pObjectId);
typedef int(*gpuDeleteObjectFunc)(uint64_t gpuId, uint64_t objectId);
typedef int(*gpuBindObjectFunc)(uint64_t gpuId, uint64_t objectId);
typedef int(*gpuCmdContextInitFunc)(uint64_t gpuId, uint64_t commandListSize, uint64_t* pCmdContextId);
typedef int(*gpuCmdContextDeinitFunc)(uint64_t gpuId, uint64_t cmdContextId);
typedef int(*gpuCmdContextResetFunc)(uint64_t gpuId, uint64_t cmdContextId);
typedef int(*gpuCmdContextPushCmdFunc)(uint64_t gpuId, uint64_t cmdContextId, struct gpu_cmd_info_header* pCommandInfo);
typedef int(*gpuCmdContextSubmitFunc)(uint64_t gpuId, uint64_t contextId, uint64_t cmdContextId);
typedef int(*gpuCreateContextFunc)(uint64_t gpuId, uint64_t* pContextId);
typedef int(*gpuDeleteContextFunc)(uint64_t gpuId, uint64_t contextId);
typedef int(*gpuContextAttachResourceFunc)(uint64_t gpuId, uint64_t contextId, uint64_t resourceId);
typedef int(*gpuCreateResourceFunc)(uint64_t gpuId, struct gpu_create_resource_info createResourceInfo, uint64_t* pResourceId);
typedef int(*gpuDeleteResourceFunc)(uint64_t gpuId, uint64_t resourceId);
typedef int(*gpuResourceAttachBackingFunc)(uint64_t gpuId, uint64_t resourceId, unsigned char* pBuffer, uint64_t bufferSize);
typedef int(*gpuResourceFlushFunc)(uint64_t gpuId, uint64_t resourceId, struct gpu_rect rect);
typedef int(*gpuTransferToDeviceFunc)(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_to_device_info transferToDeviceInfo);
typedef int(*gpuTransferFromDeviceFunc)(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_from_device_info transferFromDeviceInfo);
typedef int(*gpuPanicFunc)(uint64_t driverId);
#define GPU_FORMAT_INVALID (0)
#define GPU_FORMAT_B8G8R8A8_UNORM (1)
#define GPU_FORMAT_B8G8R8X8_UNORM (2)
#define GPU_FORMAT_R8G8B8A8_UNORM (67)
#define GPU_FORMAT_R32G32B32A32_FLOAT (31)
#define GPU_FORMAT_R32G32B32_FLOAT (30)
#define GPU_FORMAT_R32G32_FLOAT (29)

#define GPU_TARGET_BUFFER (0x00)
#define GPU_TARGET_TEXTURE_1D (0x01)
#define GPU_TARGET_TEXTURE_2D (0x02)
#define GPU_TARGET_TEXTURE_3D (0x03)
#define GPU_TARGET_TEXTURE_CUBE (0x04)
#define GPU_TARGET_TEXTURE_RECT (0x05)
#define GPU_TARGET_TEXTURE_1D_ARRAY (0x06)
#define GPU_TARGET_TEXTURE_2D_ARRAY (0x07)
#define GPU_TARGET_TEXTURE_CUBE_ARRAY (0x08)

#define GPU_BIND_DEPTH_STENCIL (1<<0)
#define GPU_BIND_RENDER_TARGET (1<<1)
#define GPU_BIND_SAMPLER_VIEW (1<<3)
#define GPU_BIND_VERTEX_BUFFER (1<<4)
#define GPU_BIND_INDEX_BUFFER (1<<5)
#define GPU_BIND_CONSTANT_BUFFER (1<<6)
#define GPU_BIND_DISPLAY_TARGET (1<<7)
#define GPU_BIND_COMMAND_ARGS (1<<8)
#define GPU_BIND_SHADER_BUFFER (1<<14)
#define GPU_BIND_CURSOR (1<<16)
#define GPU_BIND_SCANOUT (1<<18)

#define GPU_PRIMITIVE_POINTS (0x00)
#define GPU_PRIMITIVE_LINES (0x01)
#define GPU_PRIMITIVE_LINES_LOOP (0x02)
#define GPU_PRIMITIVE_LINES_STRIP (0x03)
#define GPU_PRIMITIVE_TRIANGLES (0x04)
#define GPU_PRIMITIVE_TRIANGLES_STRIP (0x05)
#define GPU_PRIMITIVE_TRIANGLES_FAN (0x06)
#define GPU_PRIMITIVE_QUADS (0x07)
#define GPU_PRIMITIVE_QUADS_TRIP (0x08)
#define GPU_PRIMITIVE_POLYGON (0x09)
#define GPU_PRIMITIVE_LINES_ADJACENCY (0x0A)
#define GPU_PRIMITIVE_LINE_STRIP_ADJACENCY (0x0B)

#define GPU_RESOURCE_TYPE_INVALID (0x00)
#define GPU_RESOURCE_TYPE_2D (0x01)
#define GPU_RESOURCE_TYPE_3D (0x02)

#define GPU_CMD_TYPE_INVALID (0x00)
#define GPU_CMD_TYPE_CLEAR (0x01)

#define GPU_OBJECT_TYPE_INVALID (0x00)
#define GPU_OBJECT_TYPE_SURFACE (0x01)

#define GPU_DEFAULT_CMD_LIST_SIZE (16384)
#define GPU_MAX_CMD_CONTEXT_COUNT (16384)
#define GPU_MAX_OBJECT_COUNT (16384)
#define GPU_MAX_RESOURCE_COUNT (16384)
#define GPU_MAX_CONTEXT_COUNT (16384)

struct gpu_resolution{
	uint32_t width;
	uint32_t height;	
};
struct gpu_rect{
	uint32_t width;
	uint32_t height;
	uint32_t x;
	uint32_t y;
};
struct gpu_box{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
};
struct gpu_info{
	uint64_t maxMonitorCount;
};
struct gpu_monitor_info{
	struct gpu_resolution resolution;
	volatile struct uvec4_8* pFramebuffer;
};
struct gpu_driver_vtable{
	gpuReadPixelFunc readPixel;
	gpuWritePixelFunc writePixel;
	gpuSyncFunc sync;
	gpuCommitFunc commit;
	gpuPushFunc push;
	gpuCreateObjectFunc objectCreate;
	gpuDeleteObjectFunc objectDelete;
	gpuBindObjectFunc objectBind;
	gpuCmdContextInitFunc cmdContextInit;
	gpuCmdContextDeinitFunc cmdContextDeinit;
	gpuCmdContextResetFunc cmdContextReset;
	gpuCmdContextPushCmdFunc cmdContextPushCmd;
	gpuCmdContextSubmitFunc cmdContextSubmit;
	gpuCreateContextFunc contextCreate;
	gpuDeleteContextFunc contextDelete;
	gpuContextAttachResourceFunc contextAttachResource;
	gpuCreateResourceFunc resourceCreate;
	gpuDeleteResourceFunc resourceDelete;
	gpuResourceAttachBackingFunc resourceAttachBacking;
	gpuResourceFlushFunc resourceFlush;
	gpuTransferToDeviceFunc transferToDevice;
	gpuTransferFromDeviceFunc transferFromDevice;
	gpuPanicFunc panic;
};
struct gpu_cmd_info_header{
	uint64_t commandType;
	unsigned char commandData[];
};
struct gpu_clear_cmd_info{
	struct gpu_cmd_info_header header;
	struct fvec4_32 color;
	uint32_t depth;
	uint32_t stencil;
};
struct gpu_cmd_context_desc{
	uint64_t driverCmdContextId;
	uint64_t subsystemCmdContextId;
	uint64_t commandListSize;
	struct gpu_cmd_context_desc* pFlink;
	struct gpu_cmd_context_desc* pBlink;
};
struct gpu_object_desc{
	uint64_t driverObjectId;
	uint64_t subsystemObjectId;
	uint64_t objectType;
	struct gpu_object_desc* pFlink;
	struct gpu_object_desc* pBlink;
};
struct gpu_context_desc{
	uint64_t driverContextId;
	uint64_t subsystemContextId;
	struct gpu_context_desc* pFlink;
	struct gpu_context_desc* pBlink;
};
struct gpu_resource_info{
	uint64_t contextId;
	uint8_t resourceType;
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t arraySize;
	uint32_t target;
	uint32_t flags;
};
struct gpu_resource_desc{
	uint64_t driverResourceId;
	uint64_t subsystemResourceId;
	uint64_t gpuId;
	struct gpu_resource_info resourceInfo;
	unsigned char* pBackingBuffer;
	uint64_t backingBufferSize;
	struct gpu_resource_desc* pFlink;
	struct gpu_resource_desc* pBlink;
};
struct gpu_create_resource_info{
	struct gpu_resource_info resourceInfo;
};
struct gpu_transfer_to_device_info{
	struct gpu_rect rect;
	struct gpu_box boxRect;
	uint64_t offset;
};
struct gpu_transfer_from_device_info{
	struct gpu_rect rect;
	struct gpu_box boxRect;
	uint64_t offset;
};
struct gpu_driver_features{
	uint64_t acceleration:1;
	uint64_t reserved0:63;
};
struct gpu_driver_info{
	struct gpu_driver_features features;
};
struct gpu_driver_desc{
	struct gpu_driver_vtable vtable;
	struct gpu_driver_info driverInfo;
	uint64_t driverId;
	uint64_t extra;
	struct gpu_driver_desc* pFlink;
	struct gpu_driver_desc* pBlink;
};
struct gpu_cmd_context_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_cmd_context_desc* pFirstCmdContextDesc;
	struct gpu_cmd_context_desc* pLastCmdContextDesc;
};
struct gpu_object_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_object_desc* pFirstObjectDesc;
	struct gpu_object_desc* pLastObjectDesc;
};
struct gpu_context_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_context_desc* pFirstContextDesc;
	struct gpu_context_desc* pLastContextDesc;
};
struct gpu_resource_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_resource_desc* pFirstResourceDesc;
	struct gpu_resource_desc* pLastResourceDesc;
};
struct gpu_desc{
	uint64_t driverId;
	uint64_t gpuId;
	uint64_t extra;
	struct gpu_info gpuInfo;
	struct gpu_cmd_context_subsystem_info cmdContextSubsystemInfo;
	struct gpu_object_subsystem_info objectSubsystemInfo;
	struct gpu_context_subsystem_info contextSubsystemInfo;
	struct gpu_resource_subsystem_info resourceSubsystemInfo;
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
struct gpu_resource_subsystem_info{
	struct subsystem_desc* pSubsystemDesc;
	struct gpu_resource_desc* pFirstResourceDesc;
	struct gpu_resource_desc* pLastResourceDesc;
};
int gpu_subsystem_init(void);
int gpu_driver_register(struct gpu_driver_vtable vtable, struct gpu_driver_info driverInfo, uint64_t* pDriverId);
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
int gpu_cmd_context_register(uint64_t gpuId, uint64_t* pCmdContextId);
int gpu_cmd_context_unregister(uint64_t gpuId, uint64_t cmdContextId);
int gpu_cmd_context_get_desc(uint64_t gpuId, uint64_t cmdContextId, struct gpu_cmd_context_desc** ppCmdContextDesc);
int gpu_object_register(uint64_t gpuId, uint64_t objectType, uint64_t* pObjectId);
int gpu_object_unregister(uint64_t gpuId, uint64_t objectId);
int gpu_object_get_desc(uint64_t gpuId, uint64_t objectId, struct gpu_object_desc** ppObjectDesc);
int gpu_context_register(uint64_t gpuId, uint64_t* pContextId);
int gpu_context_unregister(uint64_t gpuId, uint64_t contextId);
int gpu_context_get_desc(uint64_t gpuId, uint64_t contextId, struct gpu_context_desc** ppContextDesc);
int gpu_cmd_context_init(uint64_t gpuId, uint64_t commandListSize, uint64_t* pCmdContextId);
int gpu_cmd_context_deinit(uint64_t gpuId, uint64_t cmdContextId);
int gpu_cmd_context_reset(uint64_t gpuId, uint64_t cmdContextId);
int gpu_cmd_context_push_cmd(uint64_t gpuId, uint64_t cmdContextId, struct gpu_cmd_info_header* pCommandInfo);
int gpu_cmd_context_submit(uint64_t gpuId, uint64_t contextId, uint64_t cmdContextId);
int gpu_resource_register(uint64_t gpuId, struct gpu_resource_info resourceInfo, uint64_t* pResourceId);
int gpu_resource_unregister(uint64_t gpuId, uint64_t resourceId);
int gpu_resource_get_desc(uint64_t gpuId, uint64_t resourceId, struct gpu_resource_desc** ppResourceDesc);
int gpu_read_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pPixel);
int gpu_write_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8 pixel);
int gpu_sync(uint64_t monitorId, struct uvec4 rect);
int gpu_commit(uint64_t monitorId, struct uvec4 rect);
int gpu_push(uint64_t monitorId, struct uvec4 rect);
int gpu_object_create(uint64_t gpuId, uint64_t objectType, uint64_t* pObjectId);
int gpu_object_delete(uint64_t gpuId, uint64_t objectId);
int gpu_object_bind(uint64_t gpuId, uint64_t objectId);
int gpu_context_create(uint64_t gpuId, uint64_t* pContextId);
int gpu_context_delete(uint64_t gpuId, uint64_t contextId);
int gpu_context_attach_resource(uint64_t gpuId, uint64_t contextId, uint64_t resourceId);
int gpu_resource_create(uint64_t gpuId, struct gpu_create_resource_info createResourceInfo, uint64_t* pResourceId);
int gpu_resource_delete(uint64_t gpuId, uint64_t resourceId);
int gpu_resource_attach_backing(uint64_t gpuId, uint64_t resourceId, unsigned char* pBuffer, uint64_t bufferSize);
int gpu_resource_flush(uint64_t gpuId, uint64_t resourceId, struct gpu_rect rect);
int gpu_transfer_to_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_to_device_info transferToDeviceInfo);
int gpu_transfer_from_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_from_device_info transferFromDeviceInfo);
int gpu_clear(uint64_t monitorId, struct fvec4_32 color);
int gpu_panic(void);
#endif
