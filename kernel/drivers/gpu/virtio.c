#include "stdlib/stdlib.h"
#include "subsystem/subsystem.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "crypto/random.h"
#include "cpu/idt.h"
#include "cpu/interrupt.h"
#include "cpu/port.h"
#include "cpu/mutex.h"
#include "align.h"
#include "math/vector.h"
#include "math/math.h"
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include "drivers/apic.h"
#include "drivers/pcie.h"
#include "drivers/virtio.h"
#include "drivers/gpu/virtio.h"
static struct virtio_gpu_info gpuInfo = {0};
int virtio_gpu_init(void){
	if (virtio_gpu_get_info(&gpuInfo)!=0){
		return -1;
	}	
	if (virtualMapPage((uint64_t)gpuInfo.pBaseRegisters, (uint64_t)gpuInfo.pBaseRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map base registers\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)gpuInfo.pNotifyRegisters, (uint64_t)gpuInfo.pNotifyRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map notify registers\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)gpuInfo.pInterruptRegisters, (uint64_t)gpuInfo.pInterruptRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map interrupt registers\r\n");
		return -1;
	}	
	if (virtualMapPage((uint64_t)gpuInfo.pDeviceRegisters, (uint64_t)gpuInfo.pDeviceRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map device registers\r\n");
		return -1;
	}
	if (virtio_gpu_init_isr_mapping_table()!=0){
		printf("failed to initialize ISR mapping table\r\n");
		return -1;
	}	
	if (virtio_gpu_init_cmd_context_subsystem()!=0){
		printf("failed to initialize command context subsystem\r\n");
		return -1;
	}
	if (virtio_gpu_init_object_subsystem()!=0){
		printf("failed to initialize object subsystem\r\n");
		return -1;
	}
	printf("VIRTIO GPU driver bus: %d, dev: %d, func: %d\r\n", gpuInfo.location.bus, gpuInfo.location.dev, gpuInfo.location.func);
	if (virtio_gpu_disable_legacy_vga()!=0){
		printf("failed to disable legacy VGA GPU host controller\r\n");
		return -1;
	}
	volatile struct pcie_msix_msg_ctrl* pMsgControl = (volatile struct pcie_msix_msg_ctrl*)0x0;
	if (pcie_msix_get_msg_ctrl(gpuInfo.location, &pMsgControl)!=0){
		printf("failed to get virtual I/O GPU MSI-X message control\r\n");
		return -1;	
	}
	if (pcie_msix_enable(pMsgControl)!=0){
		printf("failed to enable virtual I/O GPU MSI-X\r\n");
		return -1;
	}	
	gpuInfo.pMsgControl = pMsgControl;
	if (virtio_gpu_reset()!=0){
		printf("failed to reset virtual I/O GPU\r\n");
		return -1;
	}	
	if (virtio_gpu_acknowledge()!=0){
		printf("failed to acknowledge virtual I/O GPU\r\n");
		return -1;
	}
	gpuInfo.pBaseRegisters->device_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY;
	struct virtio_gpu_features_legacy legacyFeatures = {0};	
	memset((void*)&legacyFeatures, 0, sizeof(struct virtio_gpu_features_legacy));
	legacyFeatures.virgl_support = 1;
	legacyFeatures.resource_uuid_support = 1;
	legacyFeatures.blob_support = 1;
	legacyFeatures.context_init_support = 1;
	gpuInfo.pBaseRegisters->driver_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY;
	gpuInfo.pBaseRegisters->driver_feature = *(uint32_t*)&legacyFeatures;
	*(volatile struct virtio_gpu_features_legacy*)&gpuInfo.pBaseRegisters->driver_feature = legacyFeatures;
	struct virtio_gpu_device_status deviceStatus = {0};
	memset((void*)&deviceStatus, 0, sizeof(struct virtio_gpu_device_status));	
	deviceStatus.features_ok = 1;
	gpuInfo.pBaseRegisters->device_status = deviceStatus;
	deviceStatus = gpuInfo.pBaseRegisters->device_status;
	if (!deviceStatus.features_ok){
		printf("virtual I/O GPU controller does not support legacy features set\r\n");	
		return -1;
	}
	gpuInfo.pBaseRegisters->device_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY;
	legacyFeatures = *(volatile struct virtio_gpu_features_legacy*)&gpuInfo.pBaseRegisters->device_feature;
	gpuInfo.virglSupport = legacyFeatures.virgl_support;
	gpuInfo.pBaseRegisters->device_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_MODERN;
	struct virtio_gpu_features_modern modernFeatures = {0};
	memset((void*)&modernFeatures, 0, sizeof(struct virtio_gpu_features_modern));	
	modernFeatures.version_1 = 1;
	gpuInfo.pBaseRegisters->driver_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_MODERN;
	gpuInfo.pBaseRegisters->driver_feature = *(uint32_t*)&modernFeatures;
	*(volatile struct virtio_gpu_features_modern*)&gpuInfo.pBaseRegisters->driver_feature = modernFeatures;	
	deviceStatus = gpuInfo.pBaseRegisters->device_status;
	gpuInfo.pBaseRegisters->device_status = deviceStatus;
	deviceStatus = gpuInfo.pBaseRegisters->device_status;
	if (!deviceStatus.features_ok){
		printf("virtual I/O GPU controller does not support modern features set\r\n");
		return -1;
	}
	if (virtio_gpu_validate_driver()!=0){
		printf("failed to validate driver\r\n");
		return -1;
	}
	struct gpu_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct gpu_driver_vtable));
	struct gpu_driver_info subsystemDriverInfo = {0};
	memset((void*)&subsystemDriverInfo, 0, sizeof(struct gpu_driver_info));
	subsystemDriverInfo.features.acceleration = gpuInfo.virglSupport;
	vtable.readPixel = virtio_gpu_subsystem_read_pixel;
	vtable.writePixel = virtio_gpu_subsystem_write_pixel;
	vtable.sync = virtio_gpu_subsystem_sync;
	vtable.commit = virtio_gpu_subsystem_commit;
	vtable.push = virtio_gpu_subsystem_push;
	vtable.setScanout = virtio_gpu_subsystem_set_scanout;
	vtable.cmdContextInit = virtio_gpu_subsystem_cmd_context_init;
	vtable.cmdContextDeinit = virtio_gpu_subsystem_cmd_context_deinit;
	vtable.cmdContextReset = virtio_gpu_subsystem_cmd_context_reset;
	vtable.cmdContextPushCmd = virtio_gpu_subsystem_push_cmd;
	vtable.cmdContextSubmit = virtio_gpu_subsystem_cmd_context_submit;
	vtable.objectCreate = virtio_gpu_subsystem_object_create;
	vtable.objectDelete = virtio_gpu_subsystem_object_delete;
	vtable.objectBind = virtio_gpu_subsystem_object_bind;
	vtable.contextCreate = virtio_gpu_subsystem_context_create;
	vtable.contextDelete = virtio_gpu_subsystem_context_delete;
	vtable.contextAttachResource = virtio_gpu_subsystem_context_attach_resource;
	vtable.resourceCreate = virtio_gpu_subsystem_resource_create;
	vtable.resourceDelete = virtio_gpu_subsystem_resource_delete;
	vtable.resourceAttachBacking = virtio_gpu_subsystem_resource_attach_backing;
	vtable.resourceFlush = virtio_gpu_subsystem_resource_flush;
	vtable.transferToDevice = virtio_gpu_subsystem_transfer_to_device;
	vtable.transferFromDevice = virtio_gpu_subsystem_transfer_from_device;
	vtable.panic = virtio_gpu_subsystem_panic_entry;
	uint64_t driverId = 0;
	if (gpu_driver_register(vtable, subsystemDriverInfo, &driverId)!=0){
		printf("failed to register virtual I/O GPU host controller driver\r\n");
		return -1;
	}
	gpuInfo.driverId = driverId;
	gpuInfo.driverVtable = vtable;
	gpuInfo.driverInfo = subsystemDriverInfo;
	uint64_t gpuId = 0;
	struct gpu_info subsystemGpuInfo = {0};
	memset((void*)&subsystemGpuInfo, 0, sizeof(struct gpu_info));
	subsystemGpuInfo.maxMonitorCount = VIRTIO_GPU_MAX_SCANOUT_COUNT;
	if (gpu_register(driverId, subsystemGpuInfo, &gpuId)!=0){
		printf("failed to register virtual I/O GPU\r\n");
		gpu_driver_unregister(driverId);
		return -1;
	}
	gpuInfo.gpuId = gpuId;
	if (virtio_gpu_queue_init(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to initialize virtual I/O GPU virtqueue\r\n");
		return -1;
	}
	gpuInfo.pBaseRegisters->queue_select = 0x0;
	uint16_t notifyId = gpuInfo.pBaseRegisters->queue_notify_id;	
	gpuInfo.controlQueueInfo.notifyId = notifyId;	
	printf("virtual I/O queue notify ID: %d\r\n", notifyId);
	if (gpuInfo.controlQueueInfo.maxMemoryDescCount>gpuInfo.pBaseRegisters->queue_size){
		printf("virtual I/O GPU host controller supports insufficient memory descriptor list size %d\r\n", gpuInfo.pBaseRegisters->queue_size);
		virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);	
		return -1;
	}	
	gpuInfo.pBaseRegisters->queue_size = gpuInfo.controlQueueInfo.maxMemoryDescCount;	
	gpuInfo.pBaseRegisters->queue_msix_vector = gpuInfo.controlQueueInfo.msixVector;	
	gpuInfo.pBaseRegisters->queue_memory_desc_list_base_low = (uint32_t)(gpuInfo.controlQueueInfo.pMemoryDescList_phys&0xFFFFFFFF);	
	gpuInfo.pBaseRegisters->queue_memory_desc_list_base_high = (uint32_t)((gpuInfo.controlQueueInfo.pMemoryDescList_phys>>32)&0xFFFFFFFF);
	gpuInfo.pBaseRegisters->queue_command_ring_base_low = (uint32_t)(gpuInfo.controlQueueInfo.pCommandRing_phys&0xFFFFFFFF);
	gpuInfo.pBaseRegisters->queue_command_ring_base_high = (uint32_t)((gpuInfo.controlQueueInfo.pCommandRing_phys>>32)&0xFFFFFFFF);
	gpuInfo.pBaseRegisters->queue_response_ring_base_low = (uint32_t)(gpuInfo.controlQueueInfo.pResponseRing_phys&0xFFFFFFFF);
	gpuInfo.pBaseRegisters->queue_response_ring_base_high = (uint32_t)((gpuInfo.controlQueueInfo.pResponseRing_phys>>32)&0xFFFFFFFF);
	gpuInfo.pBaseRegisters->queue_enable = 0x01;
	struct virtio_gpu_get_capset_info_response capsetInfoResponse = {0};
	memset((void*)&capsetInfoResponse, 0, sizeof(struct virtio_gpu_get_capset_info_response));
	if (gpuInfo.virglSupport){
		if (virtio_gpu_get_capset_info(VIRTIO_GPU_CAPSET_ID_VIRGL_MODERN, &capsetInfoResponse)!=0){
			printf("failed to get virtual I/O GPU host controller VirGL capability info\r\n");
			virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
			return -1;
		}
		if (capsetInfoResponse.responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_CAPSET_INFO){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(capsetInfoResponse.responseHeader.type, &responseTypeName);
			printf("failed to get virtual I/O GPU host controller VirGL capability info (%s)\r\n", responseTypeName);
			virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
			return -1;
		}
		struct virtio_gpu_get_capset_response* pResponseBuffer = (struct virtio_gpu_get_capset_response*)0x0;
		uint64_t responseBufferSize = sizeof(struct virtio_gpu_get_capset_response)+capsetInfoResponse.capset_max_size;
		if (virtualAlloc((uint64_t*)&pResponseBuffer, responseBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
			printf("failed to allocate physical pages for virtual I/O GPU host controller get capset response buffer\r\n");
			virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
			return -1;
		}
		struct virtio_gpu_capset_data_virgl_modern* pCapsetData = (struct virtio_gpu_capset_data_virgl_modern*)pResponseBuffer->capset_data;
		if (virtio_gpu_get_capset(capsetInfoResponse.capset_id, capsetInfoResponse.capset_max_version, pResponseBuffer, responseBufferSize)!=0){
			printf("failed to get virtual I/O GPU host controller VirGL capset data\r\n");
			virtualFree((uint64_t)pResponseBuffer, responseBufferSize);
			virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
			return -1;
		}
		if (pResponseBuffer->responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_CAPSET){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(pResponseBuffer->responseHeader.type, &responseTypeName);
			printf("failed to get virtual I/O GPU host controller VirGL capset data (%s)\r\n", responseTypeName);
			virtualFree((uint64_t)pResponseBuffer, responseBufferSize);
			virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
			return -1;
		}
		virtualFree((uint64_t)pResponseBuffer, responseBufferSize);
	}
	struct virtio_gpu_display_info_response* pDisplayInfo = (struct virtio_gpu_display_info_response*)0x0;
	if (virtualAllocPage((uint64_t*)&pDisplayInfo, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for display info\r\n");
		virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
		return -1;
	}
	uint64_t time_us = get_time_us();	
	if (virtio_gpu_get_display_info(pDisplayInfo)!=0){
		printf("failed to get display info\r\n");
		virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
		return -1;
	}	
	if (pDisplayInfo->responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_DISPLAY_INFO){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(pDisplayInfo->responseHeader.type, &responseTypeName);
		printf("failed to get display info (%s)\r\n", responseTypeName);	
		virtualFreePage((uint64_t)pDisplayInfo, 0);
		return -1;
	}	
	printf("took %dus to get display info\r\n", get_time_us()-time_us);
	for (uint64_t i = 0;i<VIRTIO_GPU_MAX_SCANOUT_COUNT;i++){
		struct virtio_gpu_scanout_info* pScanoutInfo = pDisplayInfo->scanoutList+i;
		if (!pScanoutInfo->enabled)
			continue;	
		printf("scanout %d resolution: %d:%d\r\n", i, pScanoutInfo->resolution.width, pScanoutInfo->resolution.height);	
 		uint64_t framebufferSize = pScanoutInfo->resolution.width*pScanoutInfo->resolution.height*sizeof(struct uvec4_8);
		struct uvec4_8* pFramebuffer = (struct uvec4_8*)0x0;
		if (virtualAlloc((uint64_t*)&pFramebuffer, framebufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
			printf("failed to allocate physical pages for virtual I/O GPU host controller framebuffer\r\n");
			continue;
		}
		for (uint64_t i = 0;i<framebufferSize/sizeof(struct uvec4_8);i++){
			struct uvec4_8* pPixel = pFramebuffer+i;
			*(uint32_t*)pPixel = 0;
		}
		struct gpu_monitor_info subsystemMonitorInfo = {0};
		uint64_t monitorId = 0;
		memset((void*)&subsystemMonitorInfo, 0, sizeof(struct gpu_monitor_info));
		subsystemMonitorInfo.resolution.width = pScanoutInfo->resolution.width;
		subsystemMonitorInfo.resolution.height = pScanoutInfo->resolution.height;
		subsystemMonitorInfo.pFramebuffer = (struct uvec4_8*)pFramebuffer;
		if (gpu_monitor_register(gpuInfo.gpuId, subsystemMonitorInfo, &monitorId)!=0){
			printf("failed to register virtual I/O GPU monitor into monitor subsystem\r\n");
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
		if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
			printf("failed to get monitor subsystem monitor descriptor\r\n");
			gpu_monitor_unregister(monitorId);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)kmalloc(sizeof(struct virtio_gpu_monitor_desc));
		if (!pMonitorDesc){
			printf("failed to allocate virutal I/O GPU monitor descriptor\r\n");
			gpu_monitor_unregister(monitorId);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		memset((void*)pMonitorDesc, 0, sizeof(struct virtio_gpu_monitor_desc));
		pMonitorDesc->scanoutId = i;
		pMonitorDesc->scanoutInfo = *pScanoutInfo;
		pSubsystemMonitorDesc->extra = (uint64_t)pMonitorDesc;
		uint64_t framebufferResourceId = 0;
		uint64_t framebufferResourceBlobId = 0;
		struct gpu_create_resource_info createResourceInfo = {0};
		memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
		createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_2D;
		createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R8G8B8A8_UNORM;
		createResourceInfo.resourceInfo.normal.width = pScanoutInfo->resolution.width;
		createResourceInfo.resourceInfo.normal.height = pScanoutInfo->resolution.height;
		if (gpu_resource_create(gpuInfo.gpuId, createResourceInfo, &framebufferResourceId)!=0){
			printf("failed to create virtual I/O GPU host controller framebuffer resource\r\n");
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
		createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_BLOB;
		createResourceInfo.resourceInfo.blob.resourceId = framebufferResourceId;
		createResourceInfo.resourceInfo.blob.memFlags = GPU_RESOURCE_BLOB_MEM_FLAG_HOST;
		createResourceInfo.resourceInfo.blob.mapFlags = GPU_RESOURCE_BLOB_MAP_FLAG_USE_SHAREABLE;
		createResourceInfo.resourceInfo.blob.pBlobBuffer = (unsigned char*)pFramebuffer;
		createResourceInfo.resourceInfo.blob.blobBufferSize = framebufferSize;
		if (gpu_resource_attach_backing(gpuInfo.gpuId, framebufferResourceId, (unsigned char*)pFramebuffer, framebufferSize)!=0){
			printf("failed to attach physical pages to virtual I/O GPU host controller framebuffer resource via GPU subsystem\r\n");
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		if (gpu_set_scanout(monitorId, framebufferResourceId)!=0){
			printf("failed to set virtual I/O GPU host controller scanout\r\n");
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		pMonitorDesc->resourceId = framebufferResourceId;
		clear();
		printf("virtual I/O GPU scanout initialized\r\n");
		printf("scanout resolution %d:%d\r\n", pScanoutInfo->resolution.width, pScanoutInfo->resolution.height);
		if (!gpuInfo.virglSupport){
			printf("no VirGL support\r\n");
			continue;
		}
		clear();
		uint64_t subsystemContextId = 0;
		if (gpu_context_create(gpuId, &subsystemContextId)!=0){
			printf("failed to create virtual I/O GPU host controller context via GPU subsystem\r\n");
			continue;
		}
		uint64_t subsystemResourceId = 0;
		uint64_t resourceBlobId = 0;
		struct gpu_create_resource_info subsystemCreateResourceInfo = {0};
		subsystemCreateResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
		subsystemCreateResourceInfo.resourceInfo.contextId = subsystemContextId;
		subsystemCreateResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R8G8B8A8_UNORM;
		subsystemCreateResourceInfo.resourceInfo.normal.width = pScanoutInfo->resolution.width;
		subsystemCreateResourceInfo.resourceInfo.normal.height = pScanoutInfo->resolution.height;
		subsystemCreateResourceInfo.resourceInfo.normal.arraySize = 1;
		subsystemCreateResourceInfo.resourceInfo.normal.depth = 1;
		subsystemCreateResourceInfo.resourceInfo.normal.target = GPU_TARGET_TEXTURE_2D;
		subsystemCreateResourceInfo.resourceInfo.normal.bind = GPU_BIND_RENDER_TARGET|GPU_BIND_SCANOUT;
		subsystemCreateResourceInfo.resourceInfo.normal.flags = GPU_RESOURCE_FLAG_Y_0_TOP;
		if (gpu_resource_create(gpuId, subsystemCreateResourceInfo, &subsystemResourceId)!=0){
			printf("failed to create resource via subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;	
		}
		if (gpu_context_attach_resource(gpuId, subsystemContextId, subsystemResourceId)!=0){
			printf("failed to attach GPU host controller resource to GPU host controller context via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
		subsystemCreateResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_BLOB;
		subsystemCreateResourceInfo.resourceInfo.contextId = subsystemContextId;
		subsystemCreateResourceInfo.resourceInfo.blob.resourceId = subsystemResourceId;
		subsystemCreateResourceInfo.resourceInfo.blob.memFlags = GPU_RESOURCE_BLOB_MEM_FLAG_HOST;
		subsystemCreateResourceInfo.resourceInfo.blob.mapFlags = GPU_RESOURCE_BLOB_MAP_FLAG_USE_SHAREABLE;
		subsystemCreateResourceInfo.resourceInfo.blob.pBlobBuffer = (unsigned char*)pFramebuffer;
		subsystemCreateResourceInfo.resourceInfo.blob.blobBufferSize = framebufferSize;
		if (gpu_resource_attach_backing(gpuId, subsystemResourceId, (unsigned char*)pFramebuffer, framebufferSize)!=0){
			printf("failed to attach physical pages to GPU host controller resource via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		uint64_t cmdContextId = 0;
		if (gpu_cmd_context_init(gpuId, 0, &cmdContextId)!=0){
			printf("failed to initialize command context via GPU subsystem\r\n");	
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		if (gpu_cmd_context_reset(gpuId, cmdContextId)!=0){
			printf("failed to reset GPU host controller command context descritptor via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		struct gpu_create_surface_object_info createSurfaceObjectInfo = {0};
		memset((void*)&createSurfaceObjectInfo, 0, sizeof(struct gpu_create_surface_object_info));
		createSurfaceObjectInfo.header.objectType = GPU_OBJECT_TYPE_SURFACE;
		createSurfaceObjectInfo.resourceId = subsystemResourceId;
		uint64_t surfaceObjectId = 0;
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createSurfaceObjectInfo, &surfaceObjectId)!=0){
			printf("failed to create surface object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		struct gpu_set_framebuffer_state_list_cmd_info setFramebufferStateCmdInfo = {0};
		memset((void*)&setFramebufferStateCmdInfo, 0, sizeof(struct gpu_set_framebuffer_state_list_cmd_info));
		uint32_t colorBufferHandle = surfaceObjectId;
		setFramebufferStateCmdInfo.header.commandType = GPU_CMD_TYPE_SET_FRAMEBUFFER_STATE_LIST;
		setFramebufferStateCmdInfo.colorBufferCount = 1;
		setFramebufferStateCmdInfo.pColorBufferHandleList = &colorBufferHandle;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setFramebufferStateCmdInfo);
		struct gpu_set_viewport_state_list_cmd_info setViewportStateCmdInfo = {0};
		memset((void*)&setViewportStateCmdInfo, 0, sizeof(struct gpu_set_viewport_state_list_cmd_info));
		struct gpu_viewport_state viewportStateList[1] = {0};
		memset((void*)viewportStateList, 0, sizeof(struct gpu_viewport_state));
		viewportStateList[0].translate.x = ((float)pScanoutInfo->resolution.width)/2.0f;
		viewportStateList[0].translate.y = ((float)pScanoutInfo->resolution.height)/2.0f;
		viewportStateList[0].translate.z = 1.0f;
		viewportStateList[0].scale.x = ((float)pScanoutInfo->resolution.width)/2.0f;
		viewportStateList[0].scale.y = ((float)pScanoutInfo->resolution.height)/2.0f;
		viewportStateList[0].scale.z = 1.0f;
		setViewportStateCmdInfo.header.commandType = GPU_CMD_TYPE_SET_VIEWPORT_STATE_LIST;
		setViewportStateCmdInfo.viewportStateCount = 1;
		setViewportStateCmdInfo.pViewportStateList = (struct gpu_viewport_state*)viewportStateList;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setViewportStateCmdInfo);
		struct gpu_set_scissor_state_list_cmd_info setScissorStateCmdInfo = {0};
		memset((void*)&setScissorStateCmdInfo, 0, sizeof(struct gpu_set_scissor_state_list_cmd_info));
		struct gpu_scissor_state scissorStateList[1] = {0};
		memset((void*)scissorStateList, 0, sizeof(struct gpu_scissor_state));
		scissorStateList[0].maxX = pScanoutInfo->resolution.width;
		scissorStateList[0].maxY = pScanoutInfo->resolution.height;
		setScissorStateCmdInfo.header.commandType = GPU_CMD_TYPE_SET_SCISSOR_STATE_LIST;
		setScissorStateCmdInfo.scissorStateCount = 1;
		setScissorStateCmdInfo.pScissorStateList = (struct gpu_scissor_state*)scissorStateList;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setScissorStateCmdInfo);
		if (gpu_cmd_context_submit(gpuId, subsystemContextId, cmdContextId)!=0){
			printf("failed to submit command list to GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		uint64_t rasterizerObjectId = 0;
		struct gpu_create_rasterizer_state_object_info createRasterizerObjectInfo = {0};
		memset((void*)&createRasterizerObjectInfo, 0, sizeof(struct gpu_create_rasterizer_state_object_info));
		struct gpu_rasterizer_state rasterizerState = {0};
		memset((void*)&rasterizerState, 0, sizeof(struct gpu_rasterizer_state));
		rasterizerState.depth_clip = 1;
		rasterizerState.scissor = 1;
		rasterizerState.front_ccw = 1;
		rasterizerState.half_pixel_center = 1;
		rasterizerState.bottom_edge_rule = 1;
		rasterizerState.line_smooth = 1;
		rasterizerState.fill_mode_front = GPU_POLYGON_MODE_FILL;
		rasterizerState.fill_mode_back = GPU_POLYGON_MODE_FILL;
		rasterizerState.point_size = 16.0f;
		rasterizerState.line_width = 16.0f;
		createRasterizerObjectInfo.header.objectType = GPU_OBJECT_TYPE_RASTERIZER_STATE;
		createRasterizerObjectInfo.surfaceObjectId = surfaceObjectId;
		createRasterizerObjectInfo.pRasterizerState = &rasterizerState;
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createRasterizerObjectInfo, &rasterizerObjectId)!=0){
			printf("failed to create GPU host controller rasterizer object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		uint64_t dsaObjectId = 0;
		struct gpu_create_dsa_state_object_info createDsaObjectInfo = {0};
		memset((void*)&createDsaObjectInfo, 0, sizeof(struct gpu_create_dsa_state_object_info));
		struct gpu_dsa_state dsaState = {0};
		memset((void*)&dsaState, 0, sizeof(struct gpu_dsa_state));
		dsaState.alpha_enable = 1;
		dsaState.alpha_func = GPU_FUNC_ALWAYS;
		createDsaObjectInfo.header.objectType = GPU_OBJECT_TYPE_DSA_STATE;
		createDsaObjectInfo.surfaceObjectId = surfaceObjectId;
		createDsaObjectInfo.pDsaState = &dsaState;
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createDsaObjectInfo, &dsaObjectId)!=0){
			printf("failed to create GPU host controller DSA state object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		uint64_t blendStateListObjectId = 0;
		struct gpu_create_blend_state_list_object_info createBlendStateListObjectInfo = {0};
		memset((void*)&createBlendStateListObjectInfo, 0, sizeof(struct gpu_create_blend_state_list_object_info));
		struct gpu_blend_state blendStateList[1] = {0};
		memset((void*)blendStateList, 0, sizeof(struct gpu_blend_state));
		blendStateList[0].blend_enable = 1;
		blendStateList[0].rgb_func = GPU_BLEND_ADD;
		blendStateList[0].rgb_src_factor = GPU_BLENDFACTOR_SRC_ALPHA;
		blendStateList[0].rgb_dest_factor = GPU_BLENDFACTOR_INV_SRC_ALPHA;
		blendStateList[0].alpha_func = GPU_BLEND_ADD;
		blendStateList[0].alpha_src_factor = GPU_BLENDFACTOR_ONE;
		blendStateList[0].alpha_dest_factor = GPU_BLENDFACTOR_INV_SRC_ALPHA;
		blendStateList[0].color_mask = GPU_COLOR_MASK_RGBA;
		createBlendStateListObjectInfo.header.objectType = GPU_OBJECT_TYPE_BLEND_STATE_LIST;
		createBlendStateListObjectInfo.surfaceObjectId = surfaceObjectId;
		createBlendStateListObjectInfo.blendStateCount = 1;
		createBlendStateListObjectInfo.pBlendStateList = (struct gpu_blend_state*)blendStateList;
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createBlendStateListObjectInfo, &blendStateListObjectId)!=0){
			printf("failed to create GPU host controller blend state list object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		uint64_t vertexElementListObjectId = 0;
		struct gpu_vertex_buffer_triangle vertexBuffer = {0};
		memset((void*)&vertexBuffer, 0, sizeof(struct gpu_vertex_buffer_triangle));
		vertexBuffer.vertex_list[0].position.x = -0.5f;
		vertexBuffer.vertex_list[0].position.y = -0.5f;
		vertexBuffer.vertex_list[0].position.z = 0.0f;
		vertexBuffer.vertex_list[0].position.w = 1.0f;
		vertexBuffer.vertex_list[1].position.x = 0.0f;
		vertexBuffer.vertex_list[1].position.y = 0.5f;
		vertexBuffer.vertex_list[1].position.z = 0.0f;
		vertexBuffer.vertex_list[1].position.w = 1.0f;
		vertexBuffer.vertex_list[2].position.x = 0.5f;
		vertexBuffer.vertex_list[2].position.y = -0.5f;
		vertexBuffer.vertex_list[2].position.z = 0.0f;
		vertexBuffer.vertex_list[2].position.w = 1.0f;
		vertexBuffer.vertex_list[0].color.x = 1.0f;
		vertexBuffer.vertex_list[0].color.y = 0.5f;
		vertexBuffer.vertex_list[0].color.z = 0.0f;
		vertexBuffer.vertex_list[0].color.w = 1.0f;
		vertexBuffer.vertex_list[1].color.x = 0.5f;
		vertexBuffer.vertex_list[1].color.y = 1.0f;
		vertexBuffer.vertex_list[1].color.z = 0.5f;
		vertexBuffer.vertex_list[1].color.w = 1.0f;
		vertexBuffer.vertex_list[2].color.x = 0.5f;
		vertexBuffer.vertex_list[2].color.y = 0.5f;
		vertexBuffer.vertex_list[2].color.z = 1.0f;
		vertexBuffer.vertex_list[2].color.w = 1.0f;
		vertexBuffer.vertex_list[0].textureCoord.x = -1.0f;
		vertexBuffer.vertex_list[0].textureCoord.y = -1.0f;
		vertexBuffer.vertex_list[1].textureCoord.x = 0.0f;
		vertexBuffer.vertex_list[1].textureCoord.y = 1.0f;
		vertexBuffer.vertex_list[2].textureCoord.x = 1.0f;
		vertexBuffer.vertex_list[2].textureCoord.y = -1.0f;
		struct gpu_vertex_element vertexElementList[3] = {0};
		memset((void*)&vertexElementList, 0, sizeof(struct gpu_vertex_element));
		vertexElementList[0].src_offset = 0;
		vertexElementList[0].vertex_buffer_index = 0;
		vertexElementList[0].src_format = GPU_FORMAT_R32G32B32A32_FLOAT;
		vertexElementList[1].vertex_buffer_index = 0;
		vertexElementList[1].src_offset = sizeof(vertexBuffer.vertex_list[0].position);
		vertexElementList[1].src_format = GPU_FORMAT_R32G32B32A32_FLOAT;
		vertexElementList[2].vertex_buffer_index = 0;
		vertexElementList[2].src_offset = vertexElementList[1].src_offset+sizeof(vertexBuffer.vertex_list[0].color);
		vertexElementList[2].src_format = GPU_FORMAT_R32G32_FLOAT;
		struct gpu_create_vertex_element_list_object_info createVertexElementListObjectInfo = {0};
		memset((void*)&createVertexElementListObjectInfo, 0, sizeof(struct gpu_create_vertex_element_list_object_info));
		createVertexElementListObjectInfo.header.objectType = GPU_OBJECT_TYPE_VERTEX_ELEMENT_LIST;
		createVertexElementListObjectInfo.surfaceObjectId = surfaceObjectId;
		createVertexElementListObjectInfo.vertexElementCount = 3;
		createVertexElementListObjectInfo.pVertexElementList = (struct gpu_vertex_element*)vertexElementList;
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createVertexElementListObjectInfo, &vertexElementListObjectId)!=0){
			printf("failed to create GPU host controller vertex element list object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		uint64_t vertexShaderObjectId = 0;
		uint64_t fragmentShaderObjectId = 0;
		const unsigned char vertexShaderCode[]="VERT\nDCL OUT[0], POSITION\nDCL OUT[1], COLOR\nDCL OUT[2], TEXCOORD\nDCL IN[0]\nDCL IN[1]\nDCL IN[2]\nDCL CONST[0]\nDCL TEMP[0]\nMOV TEMP[0], IN[0]\nDP2 TEMP[0].x, IN[0].xyxy, CONST[0].xyxy\nDP2 TEMP[0].y, IN[0].xyxy, CONST[0].zwzw\nMOV OUT[0], TEMP[0]\nMOV OUT[1], IN[1]\nMOV OUT[2], IN[2]\nEND\n";
		const unsigned char fragmentShaderCode[]="FRAG\nDCL OUT[0], COLOR\nDCL IN[0], COLOR, PERSPECTIVE\nDCL IN[1], TEXCOORD, PERSPECTIVE\nDCL SAMP[0]\nDCL SVIEW[0], 2D, FLOAT\nDCL TEMP[0]\nTEX TEMP[0], IN[1], SAMP[0], 2D\nMUL TEMP[0], TEMP[0], IN[0]\nMOV OUT[0], TEMP[0]\nEND\n";
		struct gpu_create_shader_object_info createShaderObjectInfo = {0};
		memset((void*)&createShaderObjectInfo, 0, sizeof(struct gpu_create_shader_object_info));
		createShaderObjectInfo.header.objectType = GPU_OBJECT_TYPE_SHADER;
		createShaderObjectInfo.surfaceObjectId = surfaceObjectId;
		createShaderObjectInfo.shaderType = GPU_SHADER_TYPE_VERTEX;
		createShaderObjectInfo.pShaderCode = (unsigned char*)vertexShaderCode;
		createShaderObjectInfo.shaderCodeSize = sizeof(vertexShaderCode);
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createShaderObjectInfo, &vertexShaderObjectId)!=0){
			printf("failed to create GPU host controller vertex shader object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		createShaderObjectInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
		createShaderObjectInfo.pShaderCode = (unsigned char*)fragmentShaderCode;
		createShaderObjectInfo.shaderCodeSize = sizeof(fragmentShaderCode);
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createShaderObjectInfo, &fragmentShaderObjectId)!=0){
			printf("failed to create GPU host controller fragment shader object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		gpu_cmd_context_reset(gpuId, cmdContextId);
		struct gpu_clear_cmd_info clearCmdInfo = {0};
		memset((void*)&clearCmdInfo, 0, sizeof(struct gpu_clear_cmd_info));
		clearCmdInfo.header.commandType = GPU_CMD_TYPE_CLEAR;
		clearCmdInfo.color.x = 0.25f;
		clearCmdInfo.color.y = 0.25f;
		clearCmdInfo.color.z = 0.25f;
		clearCmdInfo.color.w = 1.0f;
		clearCmdInfo.depth = 1.0;
		clearCmdInfo.buffers = (1<<0)|(1<<2);
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&clearCmdInfo);
		if (gpu_cmd_context_submit(gpuId, subsystemContextId, cmdContextId)!=0){
			printf("failed to submit command list to GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}	
		uint64_t textureResourceId = 0;
		struct uvec4_8* pTextureBuffer = (struct uvec4_8*)0x0;
		struct uvec4_32 textureRect = {0};
		memset((void*)&textureRect, 0, sizeof(struct uvec2_32));
		textureRect.x = 256;
		textureRect.y = 256;
		uint64_t textureBufferSize = textureRect.x*textureRect.y*sizeof(struct fvec4_32);
		if (virtualAlloc((uint64_t*)&pTextureBuffer, textureBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
			printf("failed to allocate physical pages for GPU host controller texture buffer\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		struct uvec4_8 pixelColor = {0};
		pixelColor.x = 0x00;
		pixelColor.y = 0x00;
		pixelColor.z = 0x00;
		pixelColor.w = 0xFF;
		struct uvec4_8 flipPixelColor = {0};
		flipPixelColor.x = 0xFF;
		flipPixelColor.y = 0x00;
		flipPixelColor.z = 0xFF;
		flipPixelColor.w = 0xFF;
		for (uint64_t i = 0;i<textureRect.x*textureRect.y;i++){
			if (!(i%((textureRect.x*textureRect.y)/16))&&0){
				pixelColor.x = urandint8(128, 255);
				pixelColor.y = urandint8(64, 128);
				pixelColor.z = urandint8(32, 64);
				flipPixelColor.x = urandint8(64, 128);
				flipPixelColor.y = pixelColor.y%64;
				flipPixelColor.z = (pixelColor.z%32)+128;
			}
			uint8_t pixelType = !((i/16)%2);
			pixelType = ((i/(textureRect.x*16))%2) ? pixelType : !pixelType;
			pTextureBuffer[i] = pixelType ? pixelColor : flipPixelColor;
		}
		memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
		createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
		createResourceInfo.resourceInfo.contextId = subsystemContextId;
		createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R32G32B32A32_FLOAT;
		createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R8G8B8A8_UNORM;
		createResourceInfo.resourceInfo.normal.width = textureRect.x;
		createResourceInfo.resourceInfo.normal.height = textureRect.y;
		createResourceInfo.resourceInfo.normal.depth = 1;
		createResourceInfo.resourceInfo.normal.arraySize = 1;
		createResourceInfo.resourceInfo.normal.target = GPU_TARGET_TEXTURE_2D;
		createResourceInfo.resourceInfo.normal.bind = GPU_BIND_SAMPLER_VIEW|GPU_BIND_RENDER_TARGET;
		if (gpu_resource_create(gpuId, createResourceInfo, &textureResourceId)!=0){
			printf("failed to create GPU host controller texture resource via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			continue;
		}
		if (gpu_context_attach_resource(gpuId, subsystemContextId, textureResourceId)!=0){
			printf("failed to attach GPU host controller resource to GPU host controller context via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			continue;
		}
		if (gpu_resource_attach_backing(gpuId, textureResourceId, (unsigned char*)pTextureBuffer, textureBufferSize)!=0){
			printf("failed to attach physical pages to GPU host controller resource via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			continue;	
		}
		struct gpu_transfer_to_device_info textureTransferToDeviceInfo = {0};
		memset((void*)&textureTransferToDeviceInfo, 0, sizeof(struct gpu_transfer_to_device_info));
		textureTransferToDeviceInfo.boxRect.x = 0;
		textureTransferToDeviceInfo.boxRect.y = 0;
		textureTransferToDeviceInfo.boxRect.width = textureRect.x;
		textureTransferToDeviceInfo.boxRect.height = textureRect.y;
		textureTransferToDeviceInfo.boxRect.depth = 1;
		if (gpu_transfer_to_device(gpuId, textureResourceId, textureTransferToDeviceInfo)!=0){
			printf("failed to transfer GPU host controller texture resource data to GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			continue;
		}
		uint64_t samplerViewObjectId = 0;
		struct gpu_create_sampler_view_object_info createSamplerViewObjectInfo = {0};
		memset((void*)&createSamplerViewObjectInfo, 0, sizeof(struct gpu_create_sampler_view_object_info));
		createSamplerViewObjectInfo.header.objectType = GPU_OBJECT_TYPE_SAMPLER_VIEW;
		createSamplerViewObjectInfo.resourceId = textureResourceId;
		createSamplerViewObjectInfo.format = GPU_FORMAT_R32G32B32A32_FLOAT;
		createSamplerViewObjectInfo.format = GPU_FORMAT_R8G8B8A8_UNORM;
		createSamplerViewObjectInfo.firstLevel = 0;
		createSamplerViewObjectInfo.lastLevel = 0;
		createSamplerViewObjectInfo.swizzle.r = 0;
		createSamplerViewObjectInfo.swizzle.g = 1;
		createSamplerViewObjectInfo.swizzle.b = 2;
		createSamplerViewObjectInfo.swizzle.a = 3;
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createSamplerViewObjectInfo, &samplerViewObjectId)!=0){
			printf("failed to create GPU host controller sampler view object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			continue;
		}
		uint64_t samplerStateObjectId = 0;
		struct gpu_create_sampler_state_object_info createSamplerStateObjectInfo = {0};
		memset((void*)&createSamplerStateObjectInfo, 0, sizeof(struct gpu_create_sampler_state_object_info));
		createSamplerStateObjectInfo.header.objectType = GPU_OBJECT_TYPE_SAMPLER_STATE;
		createSamplerStateObjectInfo.samplerState.wrap_s = GPU_TEXTURE_WRAP_MIRROR_REPEAT;
		createSamplerStateObjectInfo.samplerState.wrap_t = GPU_TEXTURE_WRAP_MIRROR_REPEAT;
		createSamplerStateObjectInfo.samplerState.wrap_r = GPU_TEXTURE_WRAP_MIRROR_REPEAT;
		createSamplerStateObjectInfo.samplerState.magImgFilter = GPU_TEXTURE_FILTER_LINEAR;
		createSamplerStateObjectInfo.samplerState.minImgFilter = GPU_TEXTURE_FILTER_LINEAR;
		createSamplerStateObjectInfo.samplerState.minMipFilter = GPU_TEXTURE_FILTER_LINEAR;
		createSamplerStateObjectInfo.samplerState.minDetailLevel = 16.0f;
		createSamplerStateObjectInfo.samplerState.maxDetailLevel = 31.0f;
		if (gpu_object_create(gpuId, subsystemContextId, (struct gpu_create_object_info*)&createSamplerStateObjectInfo, &samplerStateObjectId)!=0){
			printf("failed to create GPU host controller sampler state object via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			continue;
		}
		uint64_t vertexBufferResourceId = 0;
		struct gpu_vertex_buffer_triangle* pVertexBuffer = (struct gpu_vertex_buffer_triangle*)0x0;
		if (virtualAllocPage((uint64_t*)&pVertexBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
			printf("failed to allocate physical page for GPU host controller vertex buffer\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			continue;
		}
		*pVertexBuffer = vertexBuffer;
		memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
		createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
		createResourceInfo.resourceInfo.contextId = subsystemContextId;
		createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R32G32B32A32_FLOAT;
		createResourceInfo.resourceInfo.normal.format = 64;
		createResourceInfo.resourceInfo.normal.width = sizeof(struct gpu_vertex_buffer_triangle);
		createResourceInfo.resourceInfo.normal.height = 1;
		createResourceInfo.resourceInfo.normal.depth = 1;
		createResourceInfo.resourceInfo.normal.arraySize = 1;
		createResourceInfo.resourceInfo.normal.target = GPU_TARGET_BUFFER;
		createResourceInfo.resourceInfo.normal.bind = GPU_BIND_VERTEX_BUFFER;
		if (gpu_resource_create(gpuId, createResourceInfo, &vertexBufferResourceId)!=0){
			printf("failed to create GPU host controller vertex buffer object resource via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_context_attach_resource(gpuId, subsystemContextId, vertexBufferResourceId)!=0){
			printf("failed to attach GPU host controller vertex buffer resource to context via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_resource_attach_backing(gpuId, vertexBufferResourceId, (unsigned char*)pVertexBuffer, PAGE_SIZE)!=0){
			printf("failed to attach physical pages to GPU host controller resource via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		uint64_t indexBufferResourceId = 0;
		float indexBuffer[3] = {0};
		uint64_t indexBufferSize = sizeof(float)*3;
		memset((void*)indexBuffer, 0, indexBufferSize);
		memset((void*)&createResourceInfo, 0, sizeof(struct gpu_create_resource_info));
		createResourceInfo.resourceInfo.resourceType = GPU_RESOURCE_TYPE_3D;
		createResourceInfo.resourceInfo.contextId = subsystemContextId;
		createResourceInfo.resourceInfo.normal.format = GPU_FORMAT_R32_FLOAT;
		createResourceInfo.resourceInfo.normal.width = indexBufferSize;
		createResourceInfo.resourceInfo.normal.height = 1;
		createResourceInfo.resourceInfo.normal.depth = 1;
		createResourceInfo.resourceInfo.normal.arraySize = 1;
		createResourceInfo.resourceInfo.normal.target = GPU_TARGET_BUFFER;
		createResourceInfo.resourceInfo.normal.bind = GPU_BIND_INDEX_BUFFER;
		if (gpu_resource_create(gpuId, createResourceInfo, &indexBufferResourceId)!=0){
			printf("failed to create GPU host controller index buffer resource via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);	
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_context_attach_resource(gpuId, subsystemContextId, indexBufferResourceId)!=0){
			printf("failed to attach GPU host controller resource to GPU host controller context via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		gpu_cmd_context_reset(gpuId, cmdContextId);
		struct gpu_resource_inline_write_cmd_info resourceInlineWriteCmdInfo = {0};
		memset((void*)&resourceInlineWriteCmdInfo, 0, sizeof(struct gpu_resource_inline_write_cmd_info));
		resourceInlineWriteCmdInfo.header.commandType = GPU_CMD_TYPE_RESOURCE_INLINE_WRITE;
		resourceInlineWriteCmdInfo.resourceId = indexBufferResourceId;
		resourceInlineWriteCmdInfo.level = 0x00;
		resourceInlineWriteCmdInfo.stride = sizeof(float);
		resourceInlineWriteCmdInfo.layer_stride = 0x00;;
		resourceInlineWriteCmdInfo.boxRect.width = indexBufferSize;
		resourceInlineWriteCmdInfo.boxRect.height = 1;
		resourceInlineWriteCmdInfo.boxRect.depth = 1;
		resourceInlineWriteCmdInfo.bufferSize = indexBufferSize;
		resourceInlineWriteCmdInfo.pBuffer = (unsigned char*)indexBuffer;
		gpu_cmd_context_push_cmd(gpuId, subsystemContextId, (struct gpu_cmd_info_header*)&resourceInlineWriteCmdInfo);
/*		if (gpu_cmd_context_submit(gpuId, subsystemContextId, cmdContextId)!=0){
			printf("failed to submit GPU host controller command list to GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}*/
		struct gpu_transfer_to_device_info transferToDeviceInfo = {0};
		memset((void*)&transferToDeviceInfo, 0, sizeof(struct gpu_transfer_to_device_info));
		transferToDeviceInfo.boxRect.width = sizeof(struct gpu_vertex_buffer_triangle);
		transferToDeviceInfo.boxRect.height = 1;
		transferToDeviceInfo.boxRect.depth = 1;
		if (gpu_transfer_to_device(gpuId, vertexBufferResourceId, transferToDeviceInfo)!=0){
			printf("failed to transfer vertex buffer data to GPU host controller resource via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}	
		gpu_cmd_context_reset(gpuId, cmdContextId);
		struct gpu_set_vertex_buffer_list_cmd_info setVertexBufferListCmdInfo = {0};
		memset((void*)&setVertexBufferListCmdInfo, 0, sizeof(struct gpu_set_vertex_buffer_list_cmd_info));
		struct gpu_vertex_buffer vertexBufferList[1] = {0};
		memset((void*)vertexBufferList, 0, sizeof(struct gpu_vertex_buffer));
		vertexBufferList[0].stride = sizeof(struct gpu_vertex_triangle);
		vertexBufferList[0].resource_id = vertexBufferResourceId;
		setVertexBufferListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_VERTEX_BUFFER_LIST;
		setVertexBufferListCmdInfo.vertexBufferCount = 1;
		setVertexBufferListCmdInfo.pVertexBufferList = (struct gpu_vertex_buffer*)vertexBufferList;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setVertexBufferListCmdInfo);
		if (gpu_cmd_context_submit(gpuId, subsystemContextId, cmdContextId)!=0){
			printf("failed to submit GPU host controller set vertex buffer list command\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_object_bind(gpuId, rasterizerObjectId)!=0){
			printf("failed to bind GPU host controller rasterizer state object\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_object_bind(gpuId, dsaObjectId)!=0){
			printf("failed to bind GPU host controller DSA state object\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_object_bind(gpuId, blendStateListObjectId)!=0){
			printf("failed to bind GPU host controller blend state list object\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_object_bind(gpuId, vertexElementListObjectId)!=0){
			printf("faield to bind GPU host controller vertex element list object\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_object_bind(gpuId, vertexShaderObjectId)!=0){
			printf("failed to bind GPU host controller vertex shader object\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		if (gpu_object_bind(gpuId, fragmentShaderObjectId)!=0){
			printf("failed to bind GPU host controller fragment shader object\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		gpu_cmd_context_reset(gpuId, cmdContextId);
		struct gpu_bind_sampler_state_list_cmd_info bindSamplerStateListCmdInfo = {0};
		memset((void*)&bindSamplerStateListCmdInfo, 0, sizeof(struct gpu_bind_sampler_state_list_cmd_info));
		bindSamplerStateListCmdInfo.header.commandType = GPU_CMD_TYPE_BIND_SAMPLER_STATE_LIST;
		bindSamplerStateListCmdInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
		bindSamplerStateListCmdInfo.startSlot = 0x00;
		bindSamplerStateListCmdInfo.samplerStateList[0] = samplerStateObjectId;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&bindSamplerStateListCmdInfo);
		struct gpu_set_sampler_view_list_cmd_info setSamplerViewListCmdInfo = {0};
		memset((void*)&setSamplerViewListCmdInfo, 0, sizeof(struct gpu_set_sampler_view_list_cmd_info));
		setSamplerViewListCmdInfo.header.commandType = GPU_CMD_TYPE_SET_SAMPLER_VIEW_LIST;
		setSamplerViewListCmdInfo.shaderType = GPU_SHADER_TYPE_FRAGMENT;
		setSamplerViewListCmdInfo.startSlot = 0x00;
		setSamplerViewListCmdInfo.samplerViewList[0] = samplerViewObjectId;
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setSamplerViewListCmdInfo);
		if (gpu_cmd_context_submit(gpuId, subsystemContextId, cmdContextId)!=0){
			printf("failed to submit command list to GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		struct gpu_draw_vbo_cmd_info drawVboCmdInfo = {0};
		memset((void*)&drawVboCmdInfo, 0, sizeof(struct gpu_draw_vbo_cmd_info));
		struct gpu_draw_vbo_info drawVboInfo = {0};
		memset((void*)&drawVboInfo, 0, sizeof(struct gpu_draw_vbo_info));
		drawVboInfo.start = 0;
		drawVboInfo.count = 3;
		drawVboInfo.mode = GPU_PRIMITIVE_TRIANGLES;
		drawVboInfo.instance_count = 1;
		drawVboInfo.max_index = drawVboInfo.count-1;
		drawVboCmdInfo.header.commandType = GPU_CMD_TYPE_DRAW_VBO;
		drawVboCmdInfo.pDrawVboInfo = &drawVboInfo;
		pMonitorDesc->surfaceObjectId = surfaceObjectId;
		pMonitorDesc->contextId = subsystemContextId;
		pMonitorDesc->resourceId = subsystemResourceId;
		if (gpu_set_scanout(monitorId, subsystemResourceId)!=0){
			printf("failed to set virtual I/O GPU host controller scanout via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
			virtualFreePage((uint64_t)pVertexBuffer, 0);
			continue;
		}
		struct gpu_matrix_rotation_2d rotationMatrix = {0};
		memset((void*)&rotationMatrix, 0, sizeof(struct gpu_matrix_rotation_2d));
		double angle = 25.0;
		double rad = anglef_to_radf(-angle);
		rotationMatrix.matrix[0][0] = (float)cosf(rad);
		rotationMatrix.matrix[0][1] = -(float)sinf(rad);
		rotationMatrix.matrix[1][0] = (float)sinf(rad);
		rotationMatrix.matrix[1][1] = (float)cosf(rad);
		struct gpu_set_constant_buffer_cmd_info setConstantBufferCmdInfo = {0};
		memset((void*)&setConstantBufferCmdInfo, 0, sizeof(struct gpu_set_constant_buffer_cmd_info));
		setConstantBufferCmdInfo.header.commandType = GPU_CMD_TYPE_SET_CONSTANT_BUFFER;
		setConstantBufferCmdInfo.shaderType = GPU_SHADER_TYPE_VERTEX;
		setConstantBufferCmdInfo.index = 0x00;
		setConstantBufferCmdInfo.bufferSize = sizeof(struct gpu_matrix_rotation_2d);
		setConstantBufferCmdInfo.pBuffer = (unsigned char*)&rotationMatrix;
		gpu_cmd_context_reset(gpuId, cmdContextId);
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&clearCmdInfo);
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&setConstantBufferCmdInfo);
		gpu_cmd_context_push_cmd(gpuId, cmdContextId, (struct gpu_cmd_info_header*)&drawVboCmdInfo);
		time_us = get_time_us();
		if (gpu_cmd_context_submit(gpuId, subsystemContextId, cmdContextId)!=0){
			printf("failed to submit command list to GPU host controller via GPU subsystem\r\n");
			break;
		}
		uint64_t elapsed_us = get_time_us()-time_us;
		printf("vertex buffer object rasterized and fragmented in %fms\r\n", ((double)elapsed_us)/1000.0);
		struct gpu_rect flushRect = {0};
		flushRect.x = 0;
		flushRect.y = 0;
		flushRect.width = pScanoutInfo->resolution.width;
		flushRect.height = pScanoutInfo->resolution.height;
		if (gpu_resource_flush(gpuId, subsystemResourceId, flushRect)!=0){
			printf("failed to flush GPU host controller framebuffer physical pages via GPU subsystem\r\n");
			break;
		}
		gpu_resource_delete(gpuId, textureResourceId);
		gpu_resource_delete(gpuId, vertexBufferResourceId);
		virtualFree((uint64_t)pTextureBuffer, textureBufferSize);
		virtualFreePage((uint64_t)pVertexBuffer, 0);
		struct gpu_transfer_from_device_info transferFromDeviceInfo = {0};
		memset((void*)&transferFromDeviceInfo, 0, sizeof(struct gpu_transfer_from_device_info));
		transferFromDeviceInfo.boxRect.width = subsystemCreateResourceInfo.resourceInfo.normal.width;
		transferFromDeviceInfo.boxRect.height = subsystemCreateResourceInfo.resourceInfo.normal.height;
		transferFromDeviceInfo.boxRect.depth = 1;
/*		if (gpu_transfer_from_device(gpuId, subsystemResourceId, transferFromDeviceInfo)!=0){
			printf("failed to transfer resource data from virtual I/O GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
*/		memset((void*)&transferToDeviceInfo, 0, sizeof(struct gpu_transfer_to_device_info));
		transferToDeviceInfo.boxRect.width = subsystemCreateResourceInfo.resourceInfo.normal.width;
		transferToDeviceInfo.boxRect.height = subsystemCreateResourceInfo.resourceInfo.normal.height;
		transferToDeviceInfo.boxRect.depth = 1;
		transferToDeviceInfo.offset = 0;
/*		if (gpu_transfer_to_device(gpuId, subsystemResourceId, transferToDeviceInfo)!=0){
			printf("failed to transfer resource data to virtual I/O GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}*/
		flushRect.x = 0;
		flushRect.y = 0;
		flushRect.width = pScanoutInfo->resolution.width;
		flushRect.height = pScanoutInfo->resolution.height;
		if (gpu_resource_flush(gpuId, subsystemResourceId, flushRect)!=0){
			printf("failed to flush resource data to virtual I/O GPU host controller via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
		if (gpu_cmd_context_deinit(gpuId, cmdContextId)!=0){
			printf("failed to deinitialize command context via GPU subsystem\r\n");
			gpu_context_delete(gpuId, subsystemContextId);
			continue;
		}
	}
	virtualFreePage((uint64_t)pDisplayInfo, 0);
	return 0;
}
int virtio_gpu_get_info(struct virtio_gpu_info* pInfo){
	if (!pInfo)
		return -1;
	if (gpuInfo.pBaseRegisters){
		*pInfo = gpuInfo;
		return 0;
	}
	if (pcie_get_function_by_id(VIRTIO_VENDOR_ID, VIRTIO_GPU_DEVICE_ID, &gpuInfo.location)!=0){
		return -1;
	}
	struct pcie_cap_link* pCurrentCapLink = (struct pcie_cap_link*)0x0;
	if (pcie_get_first_cap_ptr(gpuInfo.location, &pCurrentCapLink)!=0){
		printf("failed to get first PCIe capability link\r\n");
		return -1;
	}	
	while (1){
		if (pCurrentCapLink->cap_id!=0x09){
			if (pcie_get_next_cap_ptr(gpuInfo.location, pCurrentCapLink, &pCurrentCapLink)!=0)
				break;
		}
		struct virtio_gpu_pcie_config_cap* pConfigCap = (struct virtio_gpu_pcie_config_cap*)pCurrentCapLink;
		uint64_t blockBase = 0;
		if (pcie_get_bar(gpuInfo.location, pConfigCap->bar, &blockBase)!=0){
			printf("failed to get BAR%d for virtual I/O GPU register block\r\n");
			return -1;
		}	
		blockBase+=pConfigCap->bar_offset;	
		switch (pConfigCap->config_type){
			case VIRTIO_GPU_BLOCK_BASE_REGISTERS:{
				gpuInfo.pBaseRegisters = (volatile struct virtio_gpu_base_registers*)blockBase;			 
				break;
			}
			case VIRTIO_GPU_BLOCK_NOTIFY_REGISTERS:{
				struct virtio_gpu_pcie_config_cap_notify* pNotifyCap = (struct virtio_gpu_pcie_config_cap_notify*)pConfigCap;
				gpuInfo.pNotifyRegisters = (volatile uint32_t*)blockBase;
				gpuInfo.notifyOffsetMultiplier = pNotifyCap->notify_offset_multiplier;
				break;				   
			}
			case VIRTIO_GPU_BLOCK_INTERRUPT_REGISTERS:{
				gpuInfo.pInterruptRegisters = (volatile struct virtio_gpu_interrupt_registers*)blockBase;
				break;				      
			}
			case VIRTIO_GPU_BLOCK_DEVICE_REGISTERS:{
				gpuInfo.pDeviceRegisters = (volatile struct virtio_gpu_device_registers*)blockBase;
				break;				
			}
			case VIRTIO_GPU_BLOCK_PCIE_REGISTERS:{
				gpuInfo.pPcieRegisters = (volatile struct virtio_gpu_pcie_registers*)blockBase;
				break;				 
			}
		}	
		if (pcie_get_next_cap_ptr(gpuInfo.location, pCurrentCapLink, &pCurrentCapLink)!=0)
			break;
	}
	if (!gpuInfo.pBaseRegisters||!gpuInfo.pNotifyRegisters||!gpuInfo.pInterruptRegisters||!gpuInfo.pDeviceRegisters||!gpuInfo.pPcieRegisters){
		return -1;
	}
	*pInfo = gpuInfo;
	return 0;
}
int virtio_gpu_disable_legacy_vga(void){
	struct pcie_info pcieInfo = {0};
	if (pcie_get_info(&pcieInfo)!=0)
		return -1;
	for (uint64_t bus = pcieInfo.startBus;bus<pcieInfo.endBus;bus++){
		for (uint64_t device = 0;device<32;device++){
			for (uint64_t function = 0;function<8;function++){
				struct pcie_location location = {0};
				location.bus = bus;
				location.dev = device;
				location.func = function;
				if (pcie_function_exists(location)!=0)
					continue;
				uint8_t classId = 0;
				uint8_t subClassId = 0;
				uint8_t progIf = 0;
				if (pcie_get_class(location, &classId)!=0)
					continue;
				if (classId!=0x03)
					continue;
				if (pcie_get_subclass(location, &subClassId)!=0)
					continue;
				if (subClassId!=0x00)
					continue;
				if (pcie_get_progif(location, &progIf)!=0)
					continue;
				if (progIf!=0x00)
					continue;
				return 0;
				outb(0x3C4, 0x01);
				outb(0x3C5, inb(0x3C5)|(1<<5));
				inb(0x3DA);
				outb(0x3C0, 0x00);
				uint32_t pcieCommandRegister = 0;
				pcie_read_dword(location, 0x04, &pcieCommandRegister);
				pcieCommandRegister&=~(1<<0);
				pcieCommandRegister&=~(1<<1);
				pcieCommandRegister&=~(1<<2);
				pcieCommandRegister&=~(1<<5);
				pcieCommandRegister&=~(1<<10);
				pcie_write_dword(location, 0x04, pcieCommandRegister);
				return 0;
			}
		}
	}
	return 0;
}
int virtio_gpu_init_isr_mapping_table(void){
	struct virtio_gpu_isr_mapping_table_entry* pIsrMappingTable = (struct virtio_gpu_isr_mapping_table_entry*)0x0;
	uint64_t isrMappingTableSize = IDT_MAX_ENTRIES*sizeof(struct virtio_gpu_isr_mapping_table_entry);	
	if (virtualAlloc((uint64_t*)&pIsrMappingTable, isrMappingTableSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate ISR mapping table\r\n");
		return -1;
	}	
	memset((void*)pIsrMappingTable, 0, sizeof(struct virtio_gpu_isr_mapping_table_entry));
	gpuInfo.pIsrMappingTable = pIsrMappingTable;
	gpuInfo.isrMappingTableSize = isrMappingTableSize;	
	return 0;
}
int virtio_gpu_deinit_isr_mapping_table(void){
	if (virtualFree((uint64_t)gpuInfo.pIsrMappingTable, gpuInfo.isrMappingTableSize)!=0)
		return -1;	
	return 0;
}
int virtio_gpu_init_cmd_context_subsystem(void){
	struct subsystem_desc* pCmdContextSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pCmdContextSubsystemDesc, VIRTIO_GPU_MAX_CMD_CONTEXT_COUNT)!=0){
		printf("failed to initialize virtual I/O GPU host controller VirGL command context subsystem\r\n");
		return -1;
	}
	gpuInfo.pCmdContextSubsystemDesc = pCmdContextSubsystemDesc;
	return 0;
}
int virtio_gpu_deinit_cmd_context_subsystem(void){
	if (subsystem_deinit(gpuInfo.pCmdContextSubsystemDesc)!=0){
		printf("failed to deinitialize virtual I/O GPU host contorller VirGL command context subsystem\r\n");
		return -1;
	}
	return 0;
}
int virtio_gpu_init_object_subsystem(void){
	struct subsystem_desc* pSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pSubsystemDesc, VIRTIO_GPU_MAX_OBJECT_COUNT)!=0){
		printf("failed to initialize object subsystem\r\n");
		return -1;
	}
	gpuInfo.pObjectSubsystemDesc = pSubsystemDesc;
	return 0;
}
int virtio_gpu_deinit_object_subsystem(void){
	if (subsystem_deinit(gpuInfo.pObjectSubsystemDesc)!=0){
		printf("failed to deinitialize object subsystem\r\n");
		return -1;
	}
	return 0;
}
int virtio_gpu_get_response_type_name(uint16_t responseType, const unsigned char** ppResponseTypeName){
	if (!ppResponseTypeName)
		return -1;
	static const unsigned char* pSuccessMappingTable[]={
		[0x00] = "No data success",
		[0x01] = "Display info success",
		[0x02] = "Capset info success",
		[0x03] = "Capset success",
		[0x04] = "Monitor identification success", 
		[0x05] = "Resource plane info success",
		[0x06] = "Map info success",
	};
	static const unsigned char* pErrorMappingTable[]={
		[0x00] = "Unknown error",
		[0x01] = "Out of VRAM",
		[0x02] = "Invalid scanout",
		[0x03] = "Invalid resource ID",
		[0x04] = "Invalid context ID",
		[0x05] = "Invalid parameter",
	};
	const unsigned char* pResponseTypeName = (const unsigned char*)0x0;
	if (responseType>=0x1100&&responseType<=0x1106){
		pResponseTypeName = pSuccessMappingTable[responseType-0x1100];	
	}	
	if (responseType>=0x1200&&responseType<=0x1206){
		pResponseTypeName = pErrorMappingTable[responseType-0x1200];	
	}
	if (!pResponseTypeName)
		return -1;
	*ppResponseTypeName = pResponseTypeName;
	return 0;
}
int virtio_gpu_reset(void){
	*(uint32_t*)&gpuInfo.pBaseRegisters->device_status = 0;
	return 0;
}
int virtio_gpu_acknowledge(void){
	struct virtio_gpu_device_status deviceStatus = {0};	
	memset((void*)&deviceStatus, 0, sizeof(struct virtio_gpu_device_status)); 
	deviceStatus.acknowledge = 1;
	gpuInfo.pBaseRegisters->device_status = deviceStatus;	
	memset((void*)&deviceStatus, 0, sizeof(struct virtio_gpu_device_status));	
	deviceStatus.driver = 1;
	gpuInfo.pBaseRegisters->device_status = deviceStatus;	
	return 0;
}
int virtio_gpu_validate_driver(void){
	struct virtio_gpu_device_status deviceStatus = {0};	
	memset((void*)&deviceStatus, 0, sizeof(struct virtio_gpu_device_status));	
	deviceStatus.driver_ok = 1;
	gpuInfo.pBaseRegisters->device_status = deviceStatus;
	return 0;
}
int virtio_gpu_sync(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box boxRect, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_rect rect = {0};
	rect.x = boxRect.x;
	rect.y = boxRect.y;
	rect.width = boxRect.width;
	rect.height = boxRect.height;
	if (virtio_gpu_commit(pResourceDesc, boxRect, pResponseHeader)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pResponseHeader->type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		mutex_unlock(&mutex);
		return 0;
	}
	if (virtio_gpu_push(pResourceDesc, rect, pResponseHeader)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pResponseHeader->type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		mutex_unlock(&mutex);
		return 0;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_commit(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box boxRect, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_response_header responseHeader = {0};
	if (pResourceDesc->resourceInfo.resourceType==VIRTIO_GPU_RESOURCE_TYPE_2D){
		struct virtio_gpu_rect rect = {0};
		rect.x = boxRect.x;
		rect.y = boxRect.y;
		rect.width = boxRect.width;
		rect.height = boxRect.height;
		if (virtio_gpu_transfer_h2d_2d(pResourceDesc, rect, ((rect.y*pResourceDesc->resourceInfo.normal.width)+rect.x)*sizeof(struct uvec4_8), &responseHeader)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
		*pResponseHeader = responseHeader;
		mutex_unlock(&mutex);
		return 0;
	}
	struct virtio_gpu_context_desc* pContextDesc = pResourceDesc->pContextDesc;
	if (!pContextDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_gl_transfer_3d_command transferCommand = {0};
	memset((void*)&transferCommand, 0, sizeof(struct virtio_gpu_gl_transfer_3d_command));
	transferCommand.resource_id = pResourceDesc->resourceId;
	transferCommand.transfer_type = VIRTIO_GPU_GL_TRANSFER_WRITE;
	transferCommand.box_rect = boxRect;
	transferCommand.box_rect.depth = 1;
	transferCommand.offset = ((boxRect.y*pResourceDesc->resourceInfo.normal.width)+boxRect.x)*sizeof(struct uvec4_8);
	transferCommand.direction = VIRTIO_GPU_GL_TRANSFER_DIRECTION_H2D;
	if (virtio_gpu_gl_transfer_3d(pContextDesc, transferCommand, &responseHeader)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_push(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_resource_flush(pResourceDesc, rect, &responseHeader)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_get_display_info(struct virtio_gpu_display_info_response* pDisplayInfo){
	if (!pDisplayInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_command_header* pCommandBuffer = (struct virtio_gpu_command_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	pCommandBuffer->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;	
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_command_header);	
	allocCmdInfo.pResponseBuffer = (unsigned char*)pDisplayInfo;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_display_info_response);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU command\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	virtio_gpu_queue_yield_until_response(&commandDesc);
	if (virtualFreePage((uint64_t)pCommandBuffer, 0)!=0){
		printf("failed to free command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_create_resource_2d(struct virtio_gpu_create_resource_info createResourceInfo, uint64_t resourceId, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_create_resource_2d_command* pCommandBuffer = (struct virtio_gpu_create_resource_2d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate command buffer physical page\r\n");	
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_create_resource_2d_command));
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate response buffer physical page\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pResponseBuffer, 0, sizeof(struct virtio_gpu_response_header));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;	
	pCommandBuffer->resource_id = resourceId;
	pCommandBuffer->format = createResourceInfo.resourceInfo.normal.format;
	pCommandBuffer->width = createResourceInfo.resourceInfo.normal.width;
	pCommandBuffer->height = createResourceInfo.resourceInfo.normal.height;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;	
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_create_resource_2d_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate create resource command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);	
		virtualFreePage((uint64_t)pResponseBuffer, 0);	
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_create_resource_3d(struct virtio_gpu_create_resource_info createResourceInfo, uint64_t resourceId, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_context_desc* pContextDesc = createResourceInfo.pContextDesc;
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_create_resource_3d_command* pCommandBuffer = (struct virtio_gpu_create_resource_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate command buffer physical page\r\n");	
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_create_resource_3d_command));
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate response buffer physical page\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pResponseBuffer, 0, sizeof(struct virtio_gpu_response_header));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_RESOURCE_CREATE_3D;
	if (pContextDesc)
		pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	pCommandBuffer->resource_id = resourceId;
	pCommandBuffer->format = createResourceInfo.resourceInfo.normal.format;
	pCommandBuffer->width = createResourceInfo.resourceInfo.normal.width;
	pCommandBuffer->height = createResourceInfo.resourceInfo.normal.height;
	pCommandBuffer->target = createResourceInfo.resourceInfo.normal.target;
	pCommandBuffer->bind = createResourceInfo.resourceInfo.normal.bind;
	pCommandBuffer->depth = createResourceInfo.resourceInfo.normal.depth;
	pCommandBuffer->flags = createResourceInfo.resourceInfo.normal.flags;
	pCommandBuffer->array_size = createResourceInfo.resourceInfo.normal.arraySize;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;	
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_create_resource_3d_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate create resource command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);	
		virtualFreePage((uint64_t)pResponseBuffer, 0);	
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_create_resource_blob(struct virtio_gpu_create_resource_info createResourceInfo, uint64_t blobResourceId, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t memoryEntryCount = align_up(createResourceInfo.resourceInfo.blob.blobBufferSize, PAGE_SIZE)/PAGE_SIZE;
	uint64_t memoryEntryListSize = memoryEntryCount*sizeof(struct virtio_gpu_memory_entry);
	struct virtio_gpu_create_resource_blob_command* pCommandBuffer = (struct virtio_gpu_create_resource_blob_command*)0x0;
	uint64_t commandBufferSize = sizeof(struct virtio_gpu_create_resource_blob_command)+memoryEntryListSize;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAlloc((uint64_t*)&pCommandBuffer, commandBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical pages for virtual I/O GPU host controller create resource blob command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller create resource blob response buffer\r\n");
		virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_create_resource_blob_command));
	struct virtio_gpu_memory_entry* pMemoryEntryList = (struct virtio_gpu_memory_entry*)pCommandBuffer->memory_entry_list;
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB;
	if (createResourceInfo.pContextDesc){
		pCommandBuffer->commandHeader.context_id = createResourceInfo.pContextDesc->contextId;
	}
	pCommandBuffer->blob_id = blobResourceId;
	pCommandBuffer->mem_flags = createResourceInfo.resourceInfo.blob.memFlags;
	pCommandBuffer->map_flags = createResourceInfo.resourceInfo.blob.mapFlags;
	pCommandBuffer->resource_id = createResourceInfo.pResourceDesc->resourceId;
	pCommandBuffer->size = createResourceInfo.resourceInfo.blob.blobBufferSize;
	pCommandBuffer->memory_entry_count = memoryEntryCount;
	for (uint64_t i = 0;i<memoryEntryCount;i++){
		struct virtio_gpu_memory_entry* pMemoryEntry = pMemoryEntryList+i;
		unsigned char* pPage = createResourceInfo.resourceInfo.blob.pBlobBuffer+(i*PAGE_SIZE);
		uint64_t pagePhysicalAddress = 0;
		uint64_t pageSize = (i==memoryEntryCount-1&&createResourceInfo.resourceInfo.blob.blobBufferSize%PAGE_SIZE) ? createResourceInfo.resourceInfo.blob.blobBufferSize%PAGE_SIZE : PAGE_SIZE;
		if (virtualToPhysical((uint64_t)pPage, &pagePhysicalAddress)!=0){
			printf("failed to get physical address of blob buffer physical page\r\n");
			virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
			virtualFreePage((uint64_t)pResponseBuffer, 0);		
			mutex_unlock(&mutex);
			return -1;
		}
		pMemoryEntry->physicalAddress = pagePhysicalAddress;
		pMemoryEntry->length = pageSize;
		pMemoryEntry->padding0 = 0;
	}
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = commandBufferSize;
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU host controller command\r\n");
		virtualFree((uint64_t)pCommandBuffer, commandBufferSize);	
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU host controller control queue\r\n");
		virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_create_resource(struct virtio_gpu_create_resource_info createResourceInfo, uint64_t resourceId, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_response_header responseHeader = {0};
	switch (createResourceInfo.resourceInfo.resourceType){
		case VIRTIO_GPU_RESOURCE_TYPE_2D:{
			if (virtio_gpu_create_resource_2d(createResourceInfo, resourceId, &responseHeader)!=0){
				printf("failed to create virtual I/O GPU host controller 2D resource\r\n");
				mutex_unlock(&mutex);
				return -1;
			}
			*pResponseHeader = responseHeader;
			mutex_unlock(&mutex);
			return 0;
		};	
		case VIRTIO_GPU_RESOURCE_TYPE_3D:{
			if (virtio_gpu_create_resource_3d(createResourceInfo, resourceId, &responseHeader)!=0){
				printf("failed to create virtual I/O GPU host controller 3D resource\r\n");
				mutex_unlock(&mutex);
				return -1;
			}				 
			*pResponseHeader = responseHeader;
			mutex_unlock(&mutex);
			return 0;				 
		};
		case VIRTIO_GPU_RESOURCE_TYPE_BLOB:{
			if (virtio_gpu_create_resource_blob(createResourceInfo, resourceId, &responseHeader)!=0){
				printf("failed to create virtual I/O GPU host controller blob resource\r\n");
				mutex_unlock(&mutex);
				return -1;
			}
			*pResponseHeader = responseHeader;
			mutex_unlock(&mutex);
			return 0;
		};
	}
	mutex_unlock(&mutex);
	return -1;
}
int virtio_gpu_delete_resource(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_delete_resource_command* pCommandBuffer = (struct virtio_gpu_delete_resource_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}  
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_delete_resource_command));
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_RESOURCE_FREE;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_delete_resource_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);	
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU delete resource command\r\n");	
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);	
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;	
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_attach_resource_backing(struct virtio_gpu_resource_desc* pResourceDesc, unsigned char* pBuffer, uint64_t bufferSize, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pBuffer||!bufferSize||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t physicalPageCount = align_up(bufferSize, PAGE_SIZE)/PAGE_SIZE;	
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	uint64_t commandBufferSize = sizeof(struct virtio_gpu_attach_backing_command)+(sizeof(struct virtio_gpu_memory_entry)*physicalPageCount);
	struct virtio_gpu_attach_backing_command* pCommandBuffer = (struct virtio_gpu_attach_backing_command*)0x0;
	if (virtualAlloc((uint64_t*)&pCommandBuffer, commandBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical pages for command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for response buffer\r\n");
		virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_attach_backing_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;	
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	pCommandBuffer->memory_entry_count = physicalPageCount;
	for (uint64_t i = 0;i<physicalPageCount;i++){
		struct virtio_gpu_memory_entry* pMemoryEntry = pCommandBuffer->memoryEntryList+i;
		uint64_t segmentSize = (i==physicalPageCount-1&&bufferSize%PAGE_SIZE) ? bufferSize%PAGE_SIZE : PAGE_SIZE;
		unsigned char* pSegment = pBuffer+(i*PAGE_SIZE);
		uint64_t pSegment_phys = 0;
		if (virtualToPhysical((uint64_t)pSegment, &pSegment_phys)!=0){
			printf("failed to get physical address of segment\r\n");	
			virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
			virtualFreePage((uint64_t)pResponseBuffer, 0);
			mutex_unlock(&mutex);
			return -1;
		}
		pMemoryEntry->physicalAddress = pSegment_phys;
		pMemoryEntry->length = segmentSize;	
		pMemoryEntry->padding0 = 0;
	}	
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = commandBufferSize;
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);	
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU attach backing command\r\n");
		virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFree((uint64_t)pCommandBuffer, commandBufferSize);	
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	pResourceDesc->resourceInfo.normal.pBuffer = pBuffer;
	pResourceDesc->resourceInfo.normal.bufferSize = bufferSize;
	virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_set_scanout(uint32_t scanoutId, struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_set_scanout_command* pCommandBuffer = (struct virtio_gpu_set_scanout_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for response header\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_set_scanout_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_SET_SCANOUT;	
	pCommandBuffer->rect.width = pResourceDesc->resourceInfo.normal.width;
	pCommandBuffer->rect.height = pResourceDesc->resourceInfo.normal.height;
	pCommandBuffer->scanout_id = scanoutId;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_set_scanout_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to push virtual I/O GPU set scanout command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseHeader, 0);
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);	
		virtualFreePage((uint64_t)pResponseHeader, 0);	
		mutex_unlock(&mutex);
		return -1;
	}	
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;	 
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_transfer_h2d_2d(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, uint64_t offset, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_transfer_to_host_2d_command* pCommandBuffer = (struct virtio_gpu_transfer_to_host_2d_command*)0x0;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;	
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		serial_print(0, "failed to allocate physical page for command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		serial_print(0, "failed to allocate physical page for response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_transfer_to_host_2d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_TRANSFER_H2D;
	pCommandBuffer->rect = rect;
	pCommandBuffer->offset = offset;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_transfer_to_host_2d_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		serial_print(0, "failed to allocate virtual I/O GPU transfer H2D 2D command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		serial_print(0, "failed to run virtual I/O GPU queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;	
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponseHeader = responseHeader;	
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_transfer_h2d_3d(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box box, uint64_t offset, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_transfer_to_host_3d_command* pCommandBuffer = (struct virtio_gpu_transfer_to_host_3d_command*)0x0;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;	
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		serial_print(0, "failed to allocate physical page for command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		serial_print(0, "failed to allocate physical page for response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_transfer_to_host_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_TRANSFER_H2D;
	pCommandBuffer->box = box;
	pCommandBuffer->offset = offset;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_transfer_to_host_3d_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		serial_print(0, "failed to allocate virtual I/O GPU transfer H2D 3D command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		serial_print(0, "failed to run virtual I/O GPU queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;	
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponseHeader = responseHeader;	
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_transfer_d2h_3d(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_box box, uint64_t offset, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_transfer_from_host_3d_command* pCommandBuffer = (struct virtio_gpu_transfer_from_host_3d_command*)0x0;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;	
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		serial_print(0, "failed to allocate physical page for command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		serial_print(0, "failed to allocate physical page for response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_transfer_from_host_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_TRANSFER_D2H;
	pCommandBuffer->box = box;
	pCommandBuffer->offset = offset;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_transfer_from_host_3d_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		serial_print(0, "failed to allocate virtual I/O GPU transfer D2H 3D command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		serial_print(0, "failed to run virtual I/O GPU queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;	
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponseHeader = responseHeader;	
	return 0;
}
int virtio_gpu_resource_flush(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_resource_flush_command* pCommandBuffer = (struct virtio_gpu_resource_flush_command*)0x0;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;	
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for resource flush command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for resource flush response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_resource_flush_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
	pCommandBuffer->rect = rect;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_resource_flush_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virutal I/O GPU resource flush command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");		
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_create_context(uint64_t contextId, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_create_context_command* pCommandBuffer = (struct virtio_gpu_create_context_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for create context command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for create context response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_create_context_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_CONTEXT_CREATE;
	pCommandBuffer->commandHeader.context_id = contextId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_create_context_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virutal I/O GPU create context command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_delete_context(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_delete_context_command* pCommandBuffer = (struct virtio_gpu_delete_context_command*)0x0;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU delete context command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU delete context response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_delete_context_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_CONTEXT_DESTROY;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_delete_context_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU host controller delete context command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU host controller control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_context_attach_resource(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_attach_context_resource_command* pCommandBuffer = (struct virtio_gpu_attach_context_resource_command*)0x0;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller attach context resource command\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller attach context resource response\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_attach_context_resource_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_CONTEXT_ATTACH_RESOURCE;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_attach_context_resource_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU host controller attach context resource command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU host controller control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	pResourceDesc->pContextDesc = pContextDesc;
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_context_detach_resource(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pResourceDesc->pContextDesc;
	if (!pContextDesc){
		printf("virtual I/O GPU host controller resource descriptor not attached to virtual I/O GPU host controller context descriptor\r\n");
		return -1;
	}
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_attach_context_resource_command* pCommandBuffer = (struct virtio_gpu_attach_context_resource_command*)0x0;
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller detach context resource command\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller detachh context resource response\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_detach_context_resource_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_CONTEXT_DETACH_RESOURCE;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	pCommandBuffer->resource_id = pResourceDesc->resourceId;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_detach_context_resource_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU host controller detach context resource command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU host controller control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	pResourceDesc->pContextDesc = (struct virtio_gpu_context_desc*)0x0;
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_submit(struct virtio_gpu_submit_3d_command* pCommandBuffer, struct virtio_gpu_response_header* pResponseHeader){
	if (!pCommandBuffer||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller submit command response\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t commandBufferSize = sizeof(struct virtio_gpu_submit_3d_command)+pCommandBuffer->size;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = commandBufferSize;
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU host controller submit command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_get_capset_info(uint32_t capsetId, struct virtio_gpu_get_capset_info_response* pResponse){
	if (!pResponse)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_get_capset_info_command* pCommandBuffer = (struct virtio_gpu_get_capset_info_command*)0x0;
	struct virtio_gpu_get_capset_info_response* pResponseBuffer = (struct virtio_gpu_get_capset_info_response*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller get capset info command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller get capset info response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_get_capset_info_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_GET_CAPSET_INFO;
	pCommandBuffer->capset_index = capsetId-1;
	struct virtio_gpu_command_desc commandDesc = {0};
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_get_capset_info_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_get_capset_info_response);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU host controller command descriptor\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU host controller control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);	
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_get_capset_info_response capsetInfoResponse = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*pResponse = capsetInfoResponse;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_get_capset(uint32_t capsetId, uint32_t capsetVersion, struct virtio_gpu_get_capset_response* pResponseBuffer, uint64_t responseBufferSize){
	if (!pResponseBuffer||!responseBufferSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_get_capset_command* pCommandBuffer = (struct virtio_gpu_get_capset_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller capset info\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_get_capset_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_GET_CAPSET;
	pCommandBuffer->capset_id = capsetId;
	pCommandBuffer->capset_version = capsetVersion;
	struct virtio_gpu_command_desc commandDesc = {0};
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_get_capset_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = responseBufferSize;
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virutal I/O GPU host controller get capset command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU host controller control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_create_surface_object(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResourceDesc||!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D; 
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_create_surface_object_command* pCommand = (struct virtio_gpu_gl_create_surface_object_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_create_surface(pCommand, pObjectDesc->objectId, pResourceDesc->resourceId, pResourceDesc->resourceInfo.normal.format, 0, 0);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
} 
int virtio_gpu_gl_create_vertex_element_list_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, uint32_t vertexElementCount, struct virtio_gpu_gl_vertex_element* pVertexElementList, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pSurfaceObjectDesc||!vertexElementCount||!pVertexElementList||!pObjectDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pSurfaceObjectDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pObjectDesc->objectType = VIRTIO_GPU_GL_OBJECT_TYPE_VERTEX_ELEMENT_LIST;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	unsigned char* pCommandList = (unsigned char*)pCommandBuffer->command_data;
	struct virtio_gpu_gl_create_vertex_element_list_object_command* pCommand = (struct virtio_gpu_gl_create_vertex_element_list_object_command*)pCommandList;
	virtio_gpu_gl_write_create_vertex_element_list(pCommand, pObjectDesc->objectId, vertexElementCount, pVertexElementList);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_create_sampler_view_object(struct virtio_gpu_resource_desc* pResourceDesc, uint32_t format, uint32_t firstLevel, uint32_t lastLevel, struct virtio_gpu_swizzle swizzle, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pObjectDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pResourceDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pObjectDesc->objectType = VIRTIO_GPU_GL_OBJECT_TYPE_SAMPLER_VIEW;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	unsigned char* pCommandList = (unsigned char*)pCommandBuffer->command_data;
	struct virtio_gpu_gl_create_sampler_view_object_command* pCommand = (struct virtio_gpu_gl_create_sampler_view_object_command*)pCommandList;
	virtio_gpu_gl_write_create_sampler_view(pCommand, pObjectDesc->objectId, pResourceDesc->resourceId, format, firstLevel, lastLevel, swizzle);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_create_sampler_state_object(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_gl_sampler_state samplerState, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pObjectDesc->objectType = VIRTIO_GPU_GL_OBJECT_TYPE_SAMPLER_STATE;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	unsigned char* pCommandList = (unsigned char*)pCommandBuffer->command_data;
	struct virtio_gpu_gl_create_sampler_state_object_command* pCommand = (struct virtio_gpu_gl_create_sampler_state_object_command*)pCommandList;
	virtio_gpu_gl_write_create_sampler_state(pCommand, pObjectDesc->objectId, samplerState);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_set_vertex_buffer_list(struct virtio_gpu_object_desc* pSurfaceObjectDesc, uint64_t vertexBufferCount, struct virtio_gpu_gl_vertex_buffer* pVertexBufferList, struct virtio_gpu_response_header* pResponseHeader){
	if (!pSurfaceObjectDesc||!vertexBufferCount||!pVertexBufferList)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pSurfaceObjectDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller submit 3D command\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_set_vertex_buffer_list_command* pCommand = (struct virtio_gpu_gl_set_vertex_buffer_list_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_set_vertex_buffer_list(pCommand, vertexBufferCount, pVertexBufferList);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_create_rasterizer_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, struct virtio_gpu_gl_rasterizer_state rasterizerState, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pSurfaceObjectDesc||!pObjectDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pSurfaceObjectDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pObjectDesc->objectType = VIRTIO_GPU_GL_OBJECT_TYPE_RASTERIZER;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	unsigned char* pCommandList = (unsigned char*)pCommandBuffer->command_data;
	struct virtio_gpu_gl_create_rasterizer_object_command* pCommand = (struct virtio_gpu_gl_create_rasterizer_object_command*)pCommandList;
	virtio_gpu_gl_write_create_rasterizer_object(pCommand, pObjectDesc->objectId, rasterizerState);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_create_dsa_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, struct virtio_gpu_gl_dsa_state dsaState, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pSurfaceObjectDesc||!pObjectDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pSurfaceObjectDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pObjectDesc->objectType = VIRTIO_GPU_GL_OBJECT_TYPE_DSA;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	unsigned char* pCommandList = (unsigned char*)pCommandBuffer->command_data;
	struct virtio_gpu_gl_create_dsa_object_command* pCommand = (struct virtio_gpu_gl_create_dsa_object_command*)pCommandList;
	virtio_gpu_gl_write_create_dsa_object(pCommand, pObjectDesc->objectId, dsaState);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_create_blend_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, uint32_t s0, uint32_t s1, uint32_t blendStateCount, struct virtio_gpu_gl_blend_state* pBlendStateList, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pSurfaceObjectDesc||!blendStateCount||!pBlendStateList||!pObjectDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pSurfaceObjectDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pObjectDesc->objectType = VIRTIO_GPU_GL_OBJECT_TYPE_BLEND;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	unsigned char* pCommandList = (unsigned char*)pCommandBuffer->command_data;
	struct virtio_gpu_gl_create_blend_object_command* pCommand = (struct virtio_gpu_gl_create_blend_object_command*)pCommandList;
	virtio_gpu_gl_write_create_blend_object(pCommand, pObjectDesc->objectId, s0, s1, blendStateCount, pBlendStateList);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_set_framebuffer_state(struct virtio_gpu_context_desc* pContextDesc, uint32_t colorBufferCount, uint32_t depthStencilHandle, uint32_t* pColorBufferHandleList, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!colorBufferCount||!pColorBufferHandleList||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_set_framebuffer_state_list_command* pCommand = (struct virtio_gpu_gl_set_framebuffer_state_list_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_set_framebuffer_state_list(pCommand, colorBufferCount, depthStencilHandle, pColorBufferHandleList);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_set_viewport_state_list(struct virtio_gpu_context_desc* pContextDesc, uint32_t startSlot, uint32_t viewportStateCount, struct virtio_gpu_gl_viewport_state* pViewportStateList, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!viewportStateCount||!pViewportStateList||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);;
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_set_viewport_state_list_command* pCommand = (struct virtio_gpu_gl_set_viewport_state_list_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_set_viewport_state_list(pCommand, startSlot, viewportStateCount, pViewportStateList);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_set_scissor_state_list(struct virtio_gpu_context_desc* pContextDesc, uint32_t startSlot, uint32_t scissorStateCount, struct virtio_gpu_gl_scissor_state* pScissorStateList, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!scissorStateCount||!pScissorStateList||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_set_scissor_state_list_command* pCommand = (struct virtio_gpu_gl_set_scissor_state_list_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_set_scissor_state_list(pCommand, startSlot, scissorStateCount, pScissorStateList);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_create_shader_object(struct virtio_gpu_object_desc* pSurfaceObjectDesc, struct virtio_gpu_create_shader_info createShaderInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pSurfaceObjectDesc||!pObjectDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pSurfaceObjectDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_shader_object_desc* pShaderObjectDesc = (struct virtio_gpu_shader_object_desc*)kmalloc(sizeof(struct virtio_gpu_shader_object_desc));
	if (!pShaderObjectDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pShaderObjectDesc, 0, sizeof(struct virtio_gpu_shader_object_desc));
	pShaderObjectDesc->pObjectDesc = pObjectDesc;
	pShaderObjectDesc->pSurfaceObjectDesc = pSurfaceObjectDesc;
	pShaderObjectDesc->shaderType = createShaderInfo.shaderType;
	pObjectDesc->objectType = VIRTIO_GPU_GL_OBJECT_TYPE_SHADER;
	pObjectDesc->pExtra = (unsigned char*)pShaderObjectDesc;
	uint64_t streamOutputSize = sizeof(struct virtio_gpu_gl_stream_output)*createShaderInfo.streamOutputCount;
	uint64_t commandBufferSize = sizeof(struct virtio_gpu_submit_3d_command);
	uint64_t glCommandBufferSize = sizeof(struct virtio_gpu_gl_create_shader_object_command)+streamOutputSize;
	uint64_t maxFirstSegmentSize = PAGE_SIZE-sizeof(struct virtio_gpu_gl_create_shader_object_command)-sizeof(struct virtio_gpu_submit_3d_command);
	uint64_t maxSegmentSize = PAGE_SIZE-sizeof(struct virtio_gpu_gl_shader_object_segment_command);
	uint64_t shaderCodeSizeAligned = align_up(createShaderInfo.shaderCodeSize, sizeof(uint32_t));
	uint64_t shaderCodeSegmentCount = 1;
	if (createShaderInfo.shaderCodeSize>maxFirstSegmentSize)
		shaderCodeSegmentCount+=((shaderCodeSizeAligned-maxFirstSegmentSize)/maxSegmentSize)+1;
	glCommandBufferSize+=shaderCodeSizeAligned+((shaderCodeSegmentCount-1)*sizeof(struct virtio_gpu_gl_shader_object_segment_command));
	commandBufferSize+=glCommandBufferSize;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAlloc((uint64_t*)&pCommandBuffer, commandBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical pages for virtual I/O GPU host controller submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_create_shader_object_command* pGlCommand = (struct virtio_gpu_gl_create_shader_object_command*)pCommandBuffer->command_data;
	struct virtio_gpu_gl_create_shader_object_so_command* pGlStreamOutputCommand = (struct virtio_gpu_gl_create_shader_object_so_command*)pGlCommand;
	memset((void*)pGlCommand, 0, sizeof(struct virtio_gpu_gl_create_shader_object_command));
	struct virtio_gpu_gl_stream_output* pStreamOutputList = (struct virtio_gpu_gl_stream_output*)(pGlStreamOutputCommand->stream_output_list);
	memcpy((void*)pStreamOutputList, (void*)createShaderInfo.pStreamOutputList, createShaderInfo.streamOutputCount*sizeof(struct virtio_gpu_gl_stream_output));
	unsigned char* pFirstShaderCodeSegment = (unsigned char*)(createShaderInfo.streamOutputCount ? (unsigned char*)pStreamOutputList+createShaderInfo.streamOutputCount : (unsigned char*)(pGlCommand+1));
	uint64_t firstSegmentSize = (PAGE_SIZE-sizeof(struct virtio_gpu_submit_3d_command)-sizeof(struct virtio_gpu_gl_create_shader_object_command));
	firstSegmentSize = (createShaderInfo.shaderCodeSize<firstSegmentSize) ? createShaderInfo.shaderCodeSize : firstSegmentSize;
	uint64_t firstSegmentSizeAligned = align_up(firstSegmentSize, sizeof(uint32_t));
	memcpy((void*)pFirstShaderCodeSegment, (void*)createShaderInfo.pShaderCode, firstSegmentSize);
	pGlCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pGlCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_SHADER;
	pGlCommand->header.length = 5+(firstSegmentSizeAligned/sizeof(uint32_t));
	pGlCommand->object_id = pObjectDesc->objectId;
	pGlCommand->shader_type = createShaderInfo.shaderType;
	pGlCommand->token_count = createShaderInfo.tokenCount;
	pGlCommand->offset_length = shaderCodeSizeAligned;
	for (uint64_t i = 0;i<shaderCodeSegmentCount-1;i++){
		struct virtio_gpu_gl_shader_object_segment_command* pSegmentCommand = (struct virtio_gpu_gl_shader_object_segment_command*)(((unsigned char*)pCommandBuffer)+((i+1)*PAGE_SIZE));
		uint64_t shaderCodeSegmentSize = (i==shaderCodeSegmentCount-2) ? createShaderInfo.shaderCodeSize%maxSegmentSize : maxSegmentSize;
		uint64_t shaderCodeSegmentSizeAligned = align_up(shaderCodeSegmentSize, sizeof(uint32_t));
		pSegmentCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
		pSegmentCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_SHADER;
		pSegmentCommand->header.length = 3+align_up(shaderCodeSegmentSize, sizeof(uint32_t))/sizeof(uint32_t);
		pSegmentCommand->offset_length = (maxFirstSegmentSize+(i*maxSegmentSize))|(1<<31);
		memcpy((void*)pSegmentCommand->segment_data, (void*)(createShaderInfo.pShaderCode+maxFirstSegmentSize+(i*maxSegmentSize)), shaderCodeSegmentSize);
	}
	pCommandBuffer->size = glCommandBufferSize;
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to push VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFree((uint64_t)pCommandBuffer, commandBufferSize);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_delete_object(struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pObjectDesc->pContextDesc->contextId;
	struct virtio_gpu_gl_delete_object_command* pCommand = (struct virtio_gpu_gl_delete_object_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_delete_object(pCommand, pObjectDesc->objectId);
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirtGL command list to virtual I/O GPU host controller\r\n");	
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);	
	return 0;
}
int virtio_gpu_gl_bind_object(struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pObjectDesc||!pResponseHeader)
		return -1;
	struct virtio_gpu_context_desc* pContextDesc = pObjectDesc->pContextDesc;
	if (!pContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_command_header* pCommand = (struct virtio_gpu_gl_command_header*)pCommandBuffer->command_data;
	switch (pObjectDesc->objectType){
		case VIRTIO_GPU_GL_OBJECT_TYPE_SHADER:{
			struct virtio_gpu_shader_object_desc* pShaderObjectDesc = (struct virtio_gpu_shader_object_desc*)pObjectDesc->pExtra;
			if (!pShaderObjectDesc){
				mutex_unlock(&mutex);
				return -1;
			}
			virtio_gpu_gl_write_bind_shader_object((struct virtio_gpu_gl_bind_shader_object_command*)pCommand, pObjectDesc->objectId, pShaderObjectDesc->shaderType);
			break;
		}
		default:{
			virtio_gpu_gl_write_bind_object((struct virtio_gpu_gl_bind_object_command*)pCommand, pObjectDesc->objectId, pObjectDesc->objectType);
			break;
		}
	}
	pCommandBuffer->size = (pCommand->length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");	
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);	
	return 0;
}
int virtio_gpu_gl_clear(struct virtio_gpu_context_desc* pContextDesc, uint32_t buffers, struct fvec4_32 color, double depth, uint32_t stencil, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for clear command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_clear_command* pClearCommand = (struct virtio_gpu_gl_clear_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_clear(pClearCommand, buffers, color, depth, stencil);
	pCommandBuffer->size = (pClearCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_draw_vbo(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_gl_draw_vbo_command_info commandInfo, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for virtual I/O GPU host controller submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_draw_vbo_command* pCommand = (struct virtio_gpu_gl_draw_vbo_command*)pCommandBuffer->command_data;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_DRAW_VBO;
	pCommand->header.length = 12;
	pCommand->header.object_type = 0;
	pCommand->commandInfo = commandInfo;
	pCommandBuffer->size = (pCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_transfer_3d(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_gl_transfer_3d_command command, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for submit 3D command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset_32((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	struct virtio_gpu_gl_transfer_3d_command* pGlCommand = (struct virtio_gpu_gl_transfer_3d_command*)pCommandBuffer->command_data;
	virtio_gpu_gl_write_transfer_3d(pGlCommand, command);
	pCommandBuffer->size = (pGlCommand->header.length+1)*sizeof(uint32_t);
	if (virtio_gpu_submit(pCommandBuffer, pResponseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gl_write_delete_object(struct virtio_gpu_gl_delete_object_command* pCommand, uint32_t objectId){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_DELETE_OBJECT;
	pCommand->header.object_type = 0;
	pCommand->header.length = 1;
	pCommand->object_id = objectId;
	return 0;
}
int virtio_gpu_gl_write_bind_object(struct virtio_gpu_gl_bind_object_command* pCommand, uint32_t objectId, uint32_t objectType){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_BIND_OBJECT;
	pCommand->header.object_type = objectType;
	pCommand->header.length = 1;
	pCommand->object_id = objectId;
	return 0;
}
int virtio_gpu_gl_write_bind_shader_object(struct virtio_gpu_gl_bind_shader_object_command* pCommand, uint32_t objectId, uint32_t shaderType){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_BIND_SHADER;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_SHADER;
	pCommand->header.length = 2;
	pCommand->object_id = objectId;
	pCommand->shader_type = shaderType;
	return 0;
}
int virtio_gpu_gl_write_create_surface(struct virtio_gpu_gl_create_surface_object_command* pCommand, uint32_t objectId, uint32_t resourceId, uint32_t format, uint32_t dword0, uint32_t dword1){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_SURFACE;
	pCommand->header.length = 5;
	pCommand->object_id = objectId;
	pCommand->resource_id = resourceId;
	pCommand->format = format;
	pCommand->texture.level = dword0;
	pCommand->texture.first_layer = dword1;
	return 0;
}
int virtio_gpu_gl_write_create_vertex_element_list(struct virtio_gpu_gl_create_vertex_element_list_object_command* pCommand, uint32_t objectId, uint32_t vertexElementCount, struct virtio_gpu_gl_vertex_element* pVertexElementList){
	if (!pCommand||!vertexElementCount||!pVertexElementList)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_VERTEX_ELEMENT_LIST;
	pCommand->header.length = 1+(vertexElementCount*4);
	pCommand->object_id = objectId;
	memcpy((void*)pCommand->vertex_element_list, (void*)pVertexElementList, vertexElementCount*sizeof(struct virtio_gpu_gl_vertex_element));
	return 0;
}
int virtio_gpu_gl_write_create_sampler_view(struct virtio_gpu_gl_create_sampler_view_object_command* pCommand, uint32_t objectId, uint32_t resourceId, uint32_t format, uint32_t firstLevel, uint32_t lastLevel, struct virtio_gpu_swizzle swizzle){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_SAMPLER_VIEW;
	pCommand->header.length = 6;
	pCommand->object_id = objectId;
	pCommand->resource_id = resourceId;
	pCommand->format = format;
	pCommand->first_level = firstLevel;
	pCommand->last_level = lastLevel;
	pCommand->swizzle = swizzle;
	return 0;
}
int virtio_gpu_gl_write_create_sampler_state(struct virtio_gpu_gl_create_sampler_state_object_command* pCommand, uint32_t objectId, struct virtio_gpu_gl_sampler_state samplerState){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_SAMPLER_STATE;
	pCommand->header.length = 9;
	pCommand->object_id = objectId;
	pCommand->sampler_state = samplerState;
	return 0;
}
int virtio_gpu_gl_write_set_vertex_buffer_list(struct virtio_gpu_gl_set_vertex_buffer_list_command* pCommand, uint64_t vertexBufferCount, struct virtio_gpu_gl_vertex_buffer* pVertexBufferList){
	if (!pCommand||!vertexBufferCount||!pVertexBufferList)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_VERTEX_BUFFER_LIST;
	pCommand->header.object_type = 0x0;
	pCommand->header.length = (vertexBufferCount*3);
	memcpy((void*)pCommand->vertex_buffer_list, (void*)pVertexBufferList, vertexBufferCount*sizeof(struct virtio_gpu_gl_vertex_buffer));
	return 0;
}
int virtio_gpu_gl_write_create_rasterizer_object(struct virtio_gpu_gl_create_rasterizer_object_command* pCommand, uint32_t objectId, struct virtio_gpu_gl_rasterizer_state rasterizerState){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_RASTERIZER;
	pCommand->header.length = 9;
	pCommand->object_id = objectId;
	pCommand->rasterizer_state = rasterizerState;
	return 0;
}
int virtio_gpu_gl_write_create_dsa_object(struct virtio_gpu_gl_create_dsa_object_command* pCommand, uint32_t objectId, struct virtio_gpu_gl_dsa_state dsaState){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_DSA;
	pCommand->header.length = 5;
	pCommand->object_id = objectId;
	pCommand->dsa_state = dsaState;
	return 0;
}
int virtio_gpu_gl_write_create_blend_object(struct virtio_gpu_gl_create_blend_object_command* pCommand, uint32_t objectId, uint32_t s0, uint32_t s1, uint32_t blendStateCount, struct virtio_gpu_gl_blend_state* pBlendStateList){
	if (!pCommand||!pBlendStateList)
		return -1;
	memset((void*)pCommand, 0, sizeof(struct virtio_gpu_gl_create_blend_object_command));
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CREATE_OBJECT;
	pCommand->header.object_type = VIRTIO_GPU_GL_OBJECT_TYPE_BLEND;
	pCommand->header.length = 11;
	pCommand->object_id = objectId;
	pCommand->s0 = s0;
	pCommand->s1 = s1;
	memcpy((void*)pCommand->blend_state_list, (void*)pBlendStateList, blendStateCount*sizeof(struct virtio_gpu_gl_blend_state));
	return 0;
}
int virtio_gpu_gl_write_set_framebuffer_state_list(struct virtio_gpu_gl_set_framebuffer_state_list_command* pCommand, uint32_t colorBufferCount, uint32_t depthStencilHandle, uint32_t* pColorBufferHandleList){
	if (!pCommand||!colorBufferCount||!pColorBufferHandleList)
		return -1;
	if (colorBufferCount>8)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_FRAMEBUFFER_STATE_LIST;
	pCommand->header.object_type = 0;
	pCommand->header.length = 2+colorBufferCount;
	pCommand->color_buffer_count = colorBufferCount;
	pCommand->depth_stencil_handle = depthStencilHandle;
	memcpy((void*)pCommand->color_buffer_handle_list, (void*)pColorBufferHandleList, sizeof(uint32_t)*colorBufferCount);
	return 0;
}
int virtio_gpu_gl_write_set_viewport_state_list(struct virtio_gpu_gl_set_viewport_state_list_command* pCommand, uint32_t startSlot, uint64_t viewportStateCount, struct virtio_gpu_gl_viewport_state* pViewportStateList){
	if (!pCommand||!pViewportStateList)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_VIEWPORT_STATE_LIST;
	pCommand->header.object_type = 0;
	pCommand->header.length = 1+(6*viewportStateCount);
	pCommand->start_slot = startSlot;	
	memcpy((void*)pCommand->viewport_state_list, (void*)pViewportStateList, sizeof(struct virtio_gpu_gl_viewport_state)*viewportStateCount);
	return 0;
}
int virtio_gpu_gl_write_set_scissor_state_list(struct virtio_gpu_gl_set_scissor_state_list_command* pCommand, uint32_t startSlot, uint32_t scissorStateCount, struct virtio_gpu_gl_scissor_state* pScissorStateList){
	if (!pCommand||!pScissorStateList)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_SCISSOR_STATE_LIST;
	pCommand->header.object_type = 0x0;
	pCommand->header.length = 1+(scissorStateCount*2);
	pCommand->start_slot = startSlot;
	memcpy((void*)pCommand->scissor_state_list, (void*)pScissorStateList, sizeof(struct virtio_gpu_gl_scissor_state)*scissorStateCount);
	return 0;
}
int virtio_gpu_gl_write_clear(struct virtio_gpu_gl_clear_command* pCommand, uint32_t buffers, struct fvec4_32 color, double depth, uint32_t stencil){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_CLEAR;
	pCommand->header.object_type = 0;
	pCommand->header.length = 8;
	pCommand->buffers = buffers;
	pCommand->color = color;
	pCommand->depth = depth;
	pCommand->stencil = stencil;
	return 0;
}
int virtio_gpu_gl_write_resource_inline_write(struct virtio_gpu_gl_resource_inline_write_command* pCommand, uint32_t resourceId, uint32_t level, uint32_t stride, uint32_t layer_stride, struct virtio_gpu_box boxRect, unsigned char* pBuffer, uint64_t bufferSize){
	if (!pCommand||!pBuffer||!bufferSize)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_RESOURCE_INLINE_WRITE;
	pCommand->header.object_type = 0x00;
	pCommand->header.length = 10+(bufferSize/sizeof(uint32_t));
	pCommand->resource_id = resourceId;
	pCommand->level = level;
	pCommand->stride = stride;
	pCommand->layer_stride = layer_stride;
	pCommand->boxRect = boxRect;
	memcpy((void*)pCommand->data, (void*)pBuffer, bufferSize);
	return 0;
}
int virtio_gpu_gl_write_set_sampler_view_list(struct virtio_gpu_gl_set_sampler_view_list_command* pCommand, uint32_t shaderType, uint32_t startSlot, uint32_t samplerViewCount, uint32_t* pSamplerViewList){
	if (!pCommand||!pSamplerViewList)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_SAMPLER_VIEW_LIST;
	pCommand->header.object_type = 0x00;
	pCommand->header.length = 2+samplerViewCount;
	pCommand->shader_type = shaderType;
	pCommand->start_slot = startSlot;
	memcpy((void*)pCommand->sampler_view_list, (void*)pSamplerViewList, samplerViewCount*sizeof(uint32_t));
	return 0;
}
int virtio_gpu_gl_write_set_index_buffer(struct virtio_gpu_gl_set_index_buffer_command* pCommand, uint32_t resourceId, uint32_t length, uint32_t offset){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_INDEX_BUFFER;
	pCommand->header.object_type = 0x00;
	pCommand->header.length = 3;
	pCommand->resource_id = resourceId;
	pCommand->length = length;
	pCommand->offset = offset;
	return 0;
}
int virtio_gpu_gl_write_set_constant_buffer(struct virtio_gpu_gl_set_constant_buffer_command* pCommand, uint32_t shaderType, uint32_t index, uint32_t bufferSize, unsigned char* pBuffer){
	if (!pCommand||!bufferSize||!pBuffer)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_CONSTANT_BUFFER;
	pCommand->header.object_type = 0x00;
	pCommand->header.length = 2+(bufferSize/sizeof(uint32_t));
	pCommand->shader_type = shaderType;
	pCommand->index = index;
	memcpy((void*)pCommand->data, (void*)pBuffer, bufferSize);
	return 0;
}
int virtio_gpu_gl_write_bind_sampler_state_list(struct virtio_gpu_gl_bind_sampler_state_list_command* pCommand, uint32_t shaderType, uint32_t startSlot, uint32_t samplerStateCount, uint32_t* pSamplerStateList){
	if (!pCommand||!pSamplerStateList)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_BIND_SAMPLER_STATE_LIST;
	pCommand->header.object_type = 0x00;
	pCommand->header.length = 2+samplerStateCount;
	pCommand->shader_type = shaderType;
	pCommand->start_slot = startSlot;
	memcpy((void*)pCommand->sampler_state_list, (void*)pSamplerStateList, samplerStateCount*sizeof(uint32_t));
	return 0;
}
int virtio_gpu_gl_write_set_uniform_buffer(struct virtio_gpu_gl_set_uniform_buffer_command* pCommand, uint32_t shaderType, uint32_t index, uint32_t offset, uint32_t length, uint32_t resourceId){
	if (!pCommand)
		return -1;
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_SET_UNIFORM_BUFFER;
	pCommand->header.object_type = 0x00;
	pCommand->header.length = 5;
	pCommand->shader_type = shaderType;
	pCommand->index = index;
	pCommand->offset = offset;
	pCommand->length = length;
	pCommand->resource_id = resourceId;
	return 0;
}
int virtio_gpu_gl_write_transfer_3d(struct virtio_gpu_gl_transfer_3d_command* pCommand, struct virtio_gpu_gl_transfer_3d_command command){
	if (!pCommand)
		return -1;
	memcpy((void*)pCommand, (void*)&command, sizeof(struct virtio_gpu_gl_transfer_3d_command));
	pCommand->header.opcode = VIRTIO_GPU_GL_CMD_TRANSFER3D;
	pCommand->header.object_type = 0x0;
	pCommand->header.length = 13;
	return 0;
}
int virtio_gpu_cmd_context_init(uint64_t commandListSize, struct virtio_gpu_cmd_context_desc** ppCmdContextDesc){
	if (!commandListSize||!ppCmdContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_cmd_context_desc* pCmdContextDesc = (struct virtio_gpu_cmd_context_desc*)kmalloc(sizeof(struct virtio_gpu_cmd_context_desc));
	if (!pCmdContextDesc){
		printf("failed to allocate virtual I/O GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCmdContextDesc, 0, sizeof(struct virtio_gpu_cmd_context_desc));
	uint64_t cmdContextId = 0;
	if (subsystem_alloc_entry(gpuInfo.pCmdContextSubsystemDesc, (unsigned char*)pCmdContextDesc, &cmdContextId)!=0){
		printf("failed to allocate virtual I/O GPU host controller command context descriptor subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pCmdContextDesc->cmdContextId = cmdContextId;
	pCmdContextDesc->commandListSize = commandListSize;
	uint64_t commandBufferSize = sizeof(struct virtio_gpu_submit_3d_command)+commandListSize;
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
	if (virtualAlloc((uint64_t*)&pCommandBuffer, commandBufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate virtual I/O GPU host controller command buffer for VirGL command list\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCmdContextDesc->pCommandBuffer = pCommandBuffer;
	pCmdContextDesc->commandBufferSize = commandBufferSize;
	*ppCmdContextDesc = pCmdContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_cmd_context_deinit(struct virtio_gpu_cmd_context_desc* pCmdContextDesc){
	if (!pCmdContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (virtualFree((uint64_t)pCmdContextDesc->pCommandBuffer, pCmdContextDesc->commandBufferSize)!=0){
		printf("failed to free virtual I/O GPU host controller command context command buffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(gpuInfo.pCmdContextSubsystemDesc, pCmdContextDesc->cmdContextId)!=0){
		printf("failed to free virtual I/O GPU host controller command context descriptor subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	kfree((void*)pCmdContextDesc);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_cmd_context_reset(struct virtio_gpu_cmd_context_desc* pCmdContextDesc){
	if (!pCmdContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pCmdContextDesc->currentCommandOffset = 0;
	pCmdContextDesc->commandCount = 0;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_cmd_context_get_desc(uint64_t cmdContextId, struct virtio_gpu_cmd_context_desc** ppCmdContextDesc){
	if (!ppCmdContextDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_cmd_context_desc* pCmdContextDesc = (struct virtio_gpu_cmd_context_desc*)0x0;
	if (subsystem_get_entry(gpuInfo.pCmdContextSubsystemDesc, cmdContextId, (uint64_t*)&pCmdContextDesc)!=0){
		printf("failed to get virtual I/O GPU host controller VirGL command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pCmdContextDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	*ppCmdContextDesc = pCmdContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_cmd_context_get_current_cmd(struct virtio_gpu_cmd_context_desc* pCmdContextDesc, unsigned char** ppCurrentCommand){
	if (!pCmdContextDesc||!ppCurrentCommand)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	unsigned char* pCurrentCommand = ((unsigned char*)pCmdContextDesc->pCommandBuffer->command_data)+pCmdContextDesc->currentCommandOffset;
	*ppCurrentCommand = pCurrentCommand;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_cmd_context_push_cmd(struct virtio_gpu_cmd_context_desc* pCmdContextDesc, uint64_t commandSize){
	if (!pCmdContextDesc||!commandSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	pCmdContextDesc->currentCommandOffset+=commandSize;
	pCmdContextDesc->commandCount++;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_queue_init(struct virtio_gpu_queue_info* pQueueInfo){
	if (!pQueueInfo)
		return -1;
	uint64_t lapic_id = 0;
	if (lapic_get_id(&lapic_id)!=0){
		printf("failed to get LAPIC ID\r\n");
		return -1;
	}
	uint64_t maxMemoryDescCount = VIRTIO_GPU_MAX_MEMORY_DESC_COUNT;
	uint64_t maxCommandCount = VIRTIO_GPU_MAX_COMMAND_COUNT;
	uint64_t maxResponseCount = VIRTIO_GPU_MAX_RESPONSE_COUNT;	
	struct virtio_gpu_queue_info queueInfo = {0};
	memset((void*)&queueInfo, 0, sizeof(struct virtio_gpu_queue_info));
	queueInfo.maxMemoryDescCount = maxMemoryDescCount;
	queueInfo.maxCommandCount = maxCommandCount;
	queueInfo.maxResponseCount = maxResponseCount;
	*pQueueInfo = queueInfo;
	struct virtio_gpu_command_desc** ppCommandDescList = (struct virtio_gpu_command_desc**)0x0;
	uint64_t pCommandDescListSize = maxCommandCount*sizeof(struct virtio_gpu_command_desc*);	
	if (virtualAlloc((uint64_t*)&ppCommandDescList, pCommandDescListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate command descriptor list\r\n");
		return -1;
	}	
	memset((void*)ppCommandDescList, 0, pCommandDescListSize);
	pQueueInfo->ppCommandDescList = ppCommandDescList;
	pQueueInfo->pCommandDescListSize = pCommandDescListSize;	
	struct virtio_gpu_response_desc** ppResponseDescList = (struct virtio_gpu_response_desc**)0x0;
	uint64_t pResponseDescListSize = maxResponseCount*sizeof(struct virtio_gpu_response_desc*);
	if (virtualAlloc((uint64_t*)&ppResponseDescList, pResponseDescListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate response descriptor list\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}	
	memset((void*)ppResponseDescList, 0, pResponseDescListSize);	
	pQueueInfo->ppResponseDescList = ppResponseDescList;
	pQueueInfo->pResponseDescListSize = pResponseDescListSize;	
	struct virtio_gpu_memory_desc* pMemoryDescList = (struct virtio_gpu_memory_desc*)0x0;
	uint64_t pMemoryDescList_phys = 0;
	if (virtualAllocPage((uint64_t*)&pMemoryDescList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for memory descriptor list\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}
	pQueueInfo->pMemoryDescList = pMemoryDescList;
	if (virtualToPhysical((uint64_t)pMemoryDescList, &pMemoryDescList_phys)!=0){
		printf("failed to get physical address of memory descriptor list\r\n");	
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}	
	pQueueInfo->pMemoryDescList_phys = pMemoryDescList_phys;	
	memset((void*)pMemoryDescList, 0, PAGE_SIZE);	
	struct virtio_gpu_command_ring* pCommandRing = (struct virtio_gpu_command_ring*)0x0;	
	uint64_t pCommandRing_phys = 0;
	if (virtualAllocPage((uint64_t*)&pCommandRing, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for command list\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}	
	pQueueInfo->pCommandRing = pCommandRing;
	if (virtualToPhysical((uint64_t)pCommandRing, &pCommandRing_phys)!=0){
		printf("failed to get physical address of command list\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);
		return -1;
	}
	pQueueInfo->pCommandRing_phys = pCommandRing_phys;
	memset((void*)pCommandRing, 0, PAGE_SIZE);
	struct virtio_gpu_response_ring* pResponseRing = (struct virtio_gpu_response_ring*)0x0;	
	uint64_t pResponseRing_phys = 0;
	if (virtualAllocPage((uint64_t*)&pResponseRing, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for response list\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}	
	pQueueInfo->pResponseRing = pResponseRing;
	if (virtualToPhysical((uint64_t)pResponseRing, &pResponseRing_phys)!=0){
		printf("failed to get physical address of response list\r\n");	
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}	
	pQueueInfo->pResponseRing_phys = pResponseRing_phys;	
	memset((void*)pResponseRing, 0, PAGE_SIZE);
	uint8_t msixVector = (uint8_t)(gpuInfo.queueCount+1);	
	uint8_t isrVector = 0;
	if (idt_get_free_vector(&isrVector)!=0){
		printf("failed to get free ISR vector\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}	
	printf("ISR vector: %d\r\n", isrVector);
	if (idt_add_entry(isrVector, (uint64_t)virtio_gpu_response_queue_isr, 0x8E, 0x0, 0x0)!=0){
		printf("failed to set ISR entry for virtual I/O GPU response queue ISR\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}	
	pQueueInfo->isrVector = isrVector;
	if (pcie_set_msix_entry(gpuInfo.location, gpuInfo.pMsgControl, msixVector, lapic_id, isrVector)!=0){
		printf("failed to set MSI-X entry for virtual I/O GPU response queue ISR\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);
		return -1;
	}	
	pQueueInfo->msixVector = msixVector;
	if (pcie_msix_enable_entry(gpuInfo.location, gpuInfo.pMsgControl, msixVector)!=0){
		printf("failed to enable MSI-X entry for virtual I/O GPU response queue ISR\r\n");
		virtio_gpu_queue_deinit(pQueueInfo);	
		return -1;
	}
	struct virtio_gpu_isr_mapping_table_entry* pIsrMappingTableEntry = gpuInfo.pIsrMappingTable+isrVector;
	struct virtio_gpu_isr_mapping_table_entry isrMappingTableEntry = {0};
	memset((void*)&isrMappingTableEntry, 0, sizeof(struct virtio_gpu_isr_mapping_table_entry));
	isrMappingTableEntry.pQueueInfo = pQueueInfo;
	*pIsrMappingTableEntry = isrMappingTableEntry;
	return 0;
}
int virtio_gpu_queue_deinit(struct virtio_gpu_queue_info* pQueueInfo){
	if (!pQueueInfo)
		return -1;
	uint64_t memoryDescListSize = pQueueInfo->maxMemoryDescCount*sizeof(struct virtio_gpu_memory_desc);
	uint64_t commandRingSize = pQueueInfo->maxCommandCount*sizeof(struct virtio_gpu_command_list_entry);
	uint64_t responseRingSize = pQueueInfo->maxResponseCount*sizeof(struct virtio_gpu_response_list_entry);
	if (pQueueInfo->ppCommandDescList){
		if (virtualFree((uint64_t)pQueueInfo->ppCommandDescList, pQueueInfo->pCommandDescListSize)!=0){
			if (pQueueInfo->ppResponseDescList)
				virtualFree((uint64_t)pQueueInfo->ppResponseDescList, pQueueInfo->pResponseDescListSize);
			if (pQueueInfo->pMemoryDescList)
				virtualFree((uint64_t)pQueueInfo->pMemoryDescList, memoryDescListSize);
			if (pQueueInfo->pResponseRing)
				virtualFree((uint64_t)pQueueInfo->pResponseRing, responseRingSize);
			if (pQueueInfo->pCommandRing)
				virtualFree((uint64_t)pQueueInfo->pCommandRing, commandRingSize);
			return -1;
		}
	}
	if (pQueueInfo->ppResponseDescList){	
		if (virtualFree((uint64_t)pQueueInfo->ppResponseDescList, pQueueInfo->pResponseDescListSize)!=0){
			if (pQueueInfo->pMemoryDescList)
				virtualFree((uint64_t)pQueueInfo->pMemoryDescList, memoryDescListSize);
			if (pQueueInfo->pResponseRing)
				virtualFree((uint64_t)pQueueInfo->pResponseRing, responseRingSize);
			if (pQueueInfo->pCommandRing)
				virtualFree((uint64_t)pQueueInfo->pCommandRing, commandRingSize);
			return -1;
		}
	}
	if (pQueueInfo->pMemoryDescList){	
		if (virtualFree((uint64_t)pQueueInfo->pMemoryDescList, memoryDescListSize)!=0){
			if (pQueueInfo->pResponseRing)	
				virtualFree((uint64_t)pQueueInfo->pResponseRing, responseRingSize);
			if (pQueueInfo->pCommandRing)
				virtualFree((uint64_t)pQueueInfo->pCommandRing, commandRingSize);	
			return -1;
		}	
	}
	if (pQueueInfo->pCommandRing){
		if (virtualFree((uint64_t)pQueueInfo->pCommandRing, commandRingSize)!=0){
			if (pQueueInfo->pResponseRing)
				virtualFree((uint64_t)pQueueInfo->pResponseRing, responseRingSize);
			return -1;
		}
	}
	if (pQueueInfo->pResponseRing){
		if (virtualFree((uint64_t)pQueueInfo->pResponseRing, responseRingSize)!=0){
			return -1;
		}
	}
	return 0;
}
int virtio_gpu_notify(uint16_t notifyId){
	uint64_t notifyOffset = ((uint64_t)notifyId)*gpuInfo.notifyOffsetMultiplier;
	__asm__ volatile("sfence" ::: "memory");
	*(volatile uint16_t*)(((uint64_t)gpuInfo.pNotifyRegisters)+notifyOffset) = notifyId;
	__asm__ volatile("sfence" ::: "memory");
	return 0;
}
int virtio_gpu_run_queue(struct virtio_gpu_queue_info* pQueueInfo){
	if (!pQueueInfo)
		return -1;
	if (virtio_gpu_notify(pQueueInfo->notifyId)!=0){
		printf("failed to notify queue\r\n");
		return -1;
	}	
	return 0;
}
int virtio_gpu_response_queue_interrupt(uint8_t interruptVector){
	struct virtio_gpu_isr_mapping_table_entry* pIsrMappingTableEntry = gpuInfo.pIsrMappingTable+interruptVector;
	struct virtio_gpu_queue_info* pQueueInfo = pIsrMappingTableEntry->pQueueInfo;
	if (!pQueueInfo){
		return -1;
	}	
	uint64_t responseCount = pQueueInfo->pResponseRing->index-pQueueInfo->oldResponseRingIndex;
	if (!responseCount)
		return -1;
	for (uint64_t i = 0;i<responseCount;i++){	
		uint64_t currentResponse = pQueueInfo->currentResponse;	
		volatile struct virtio_gpu_response_list_entry* pResponseListEntry = &pQueueInfo->pResponseRing->responseList[currentResponse];
		struct virtio_gpu_response_desc* pResponseDesc = pQueueInfo->ppResponseDescList[pQueueInfo->currentResponse];	
		pResponseDesc->responseComplete = 1;	
		gpuInfo.controlQueueInfo.currentResponse++;
		if (gpuInfo.controlQueueInfo.currentResponse>gpuInfo.controlQueueInfo.maxResponseCount){
			gpuInfo.controlQueueInfo.currentResponse = 0;	
		}
	}
	pQueueInfo->oldResponseRingIndex = pQueueInfo->pResponseRing->index;
	return 0;
}
int virtio_gpu_alloc_memory_desc(struct virtio_gpu_queue_info* pQueueInfo, uint64_t physicalAddress, uint32_t size, uint16_t flags, struct virtio_gpu_memory_desc_info* pMemoryDescInfo){
	if (!pQueueInfo||!physicalAddress||!size||!pMemoryDescInfo){
		return -1;
	}
	uint64_t memoryDescIndex = pQueueInfo->memoryDescCount;
	if (memoryDescIndex>pQueueInfo->maxMemoryDescCount-1){
		pQueueInfo->memoryDescCount = 0;	
		memoryDescIndex = 0;
	}
	volatile struct virtio_gpu_memory_desc* pMemoryDesc = &pQueueInfo->pMemoryDescList[memoryDescIndex];	
	memset((void*)pMemoryDesc, 0, sizeof(struct virtio_gpu_memory_desc));
	pMemoryDesc->physical_address = physicalAddress;
	pMemoryDesc->length = size;	
	pMemoryDesc->flags = flags;
	struct virtio_gpu_memory_desc_info memoryDescInfo = {0};
	memset((void*)&memoryDescInfo, 0, sizeof(struct virtio_gpu_memory_desc_info));
	memoryDescInfo.pQueueInfo = pQueueInfo;
	memoryDescInfo.memoryDescIndex = memoryDescIndex;	
	memoryDescInfo.pMemoryDesc = pMemoryDesc;	
	pQueueInfo->memoryDescCount++;
	*pMemoryDescInfo = memoryDescInfo;	
	return 0;
}
int virtio_gpu_queue_alloc_cmd(struct virtio_gpu_queue_info* pQueueInfo, struct virtio_gpu_alloc_cmd_info allocCmdInfo, struct virtio_gpu_command_desc* pCommandDesc){
	if (!pQueueInfo||!pCommandDesc)
		return -1;
	uint64_t commandIndex = pQueueInfo->currentCommand;
	if (commandIndex>=pQueueInfo->maxCommandCount){
		pQueueInfo->currentCommand = 0;
		commandIndex = 0;
	}
	uint64_t pCommandBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)allocCmdInfo.pCommandBuffer, &pCommandBuffer_phys)!=0){
		printf("failed to get command buffer physical address\r\n");
		return -1;
	}	
	uint64_t pResponseBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)allocCmdInfo.pResponseBuffer, &pResponseBuffer_phys)!=0){
		printf("failed to get response buffer physical address\r\n");
		return -1;
	}	
	uint64_t commandBufferPageCount = align_up(allocCmdInfo.commandBufferSize, PAGE_SIZE)/PAGE_SIZE;
	uint64_t bufferPageCount = align_up(allocCmdInfo.bufferSize, PAGE_SIZE)/PAGE_SIZE;
	struct virtio_gpu_memory_desc_info commandMemoryDescInfo = {0};
	struct virtio_gpu_memory_desc_info responseMemoryDescInfo = {0};
	struct virtio_gpu_memory_desc_info lastMemoryDescInfo = {0};	
	for (uint64_t i = 0;i<commandBufferPageCount;i++){	
		unsigned char* pSegment = allocCmdInfo.pCommandBuffer+(i*PAGE_SIZE);
		uint64_t segmentSize = (i==commandBufferPageCount-1&&allocCmdInfo.commandBufferSize%PAGE_SIZE) ? allocCmdInfo.commandBufferSize%PAGE_SIZE : PAGE_SIZE;
		uint64_t pSegment_phys = 0;
		if (virtualToPhysical((uint64_t)pSegment, &pSegment_phys)!=0){
			printf("failed to get physical address of command buffer segment\r\n");
			return -1;
		}
		struct virtio_gpu_memory_desc_info memoryDescInfo = {0};
		if (!allocCmdInfo.pCommandBuffer)
			serial_print(0, "invalid virtual I/O GPU host controller command buffer\r\n");
		if (!pSegment)
			serial_print(0, "invalid virtual I/O GPU host controller segment\r\n");
		if (!pQueueInfo)
			serial_print(0, "invalid virtual I/O GPU host controller queue info\r\n");
		if (!pSegment_phys)
			serial_print(0, "invalid segment physical address\r\n");
		if (!segmentSize)
			serial_print(0, "invalid segment size\r\n");
//		printf("segment physical address: %p\r\n", pSegment_phys);
//		printf("segment size: %d\r\n", segmentSize);
		if (virtio_gpu_alloc_memory_desc(pQueueInfo, pSegment_phys, segmentSize, VIRTIO_GPU_MEMORY_DESC_FLAG_NEXT, &memoryDescInfo)!=0){
			serial_print(0, "failed to push virtual I/O GPU memory descriptor for command buffer physical page\r\n");
			return -1;
		} 
		if (!i){
			commandMemoryDescInfo = memoryDescInfo;
			lastMemoryDescInfo = memoryDescInfo;
			continue;
		}
		lastMemoryDescInfo.pMemoryDesc->next = memoryDescInfo.memoryDescIndex;
		lastMemoryDescInfo = memoryDescInfo;	
	}
	for (uint64_t i = 0;i<bufferPageCount;i++){
		unsigned char* pSegment = allocCmdInfo.pBuffer+(i*PAGE_SIZE);	
		uint64_t segmentSize = (i==bufferPageCount-1&&allocCmdInfo.bufferSize%PAGE_SIZE) ? allocCmdInfo.bufferSize%PAGE_SIZE : PAGE_SIZE;
		uint64_t pSegment_phys = 0;
		if (virtualToPhysical((uint64_t)pSegment, &pSegment_phys)!=0){
			printf("failed to get physical address of buffer segment\r\n");
			return -1;
		}
		struct virtio_gpu_memory_desc_info segmentMemoryDescInfo = {0};	
		if (virtio_gpu_alloc_memory_desc(pQueueInfo, pSegment_phys, segmentSize, VIRTIO_GPU_MEMORY_DESC_FLAG_NEXT, &segmentMemoryDescInfo)!=0){
			printf("failed to allocate memory desc for segment\r\n");
			return -1;
		}	
		lastMemoryDescInfo.pMemoryDesc->next = segmentMemoryDescInfo.memoryDescIndex;	
		lastMemoryDescInfo = segmentMemoryDescInfo;	
	}	
	if (virtio_gpu_alloc_memory_desc(pQueueInfo, pResponseBuffer_phys, allocCmdInfo.responseBufferSize, VIRTIO_GPU_MEMORY_DESC_FLAG_WRITE, &responseMemoryDescInfo)!=0){
		printf("failed to allocate virtuall I/O memory descriptor for response header\r\n");
		return -1;
	}
	lastMemoryDescInfo.pMemoryDesc->next = responseMemoryDescInfo.memoryDescIndex;	
	volatile struct virtio_gpu_command_list_entry* pCommandEntry = &pQueueInfo->pCommandRing->commandList[commandIndex];	
	pCommandEntry->index = commandMemoryDescInfo.memoryDescIndex;	
	struct virtio_gpu_command_desc** ppCommandDesc = pQueueInfo->ppCommandDescList+commandIndex;
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	commandDesc.pQueueInfo = pQueueInfo;	
	commandDesc.commandIndex = commandIndex;
	commandDesc.pCommandBuffer = allocCmdInfo.pCommandBuffer;
	commandDesc.pCommandBuffer_phys = pCommandBuffer_phys;
	commandDesc.commandBufferSize = allocCmdInfo.commandBufferSize;	
	commandDesc.pResponseBuffer = allocCmdInfo.pResponseBuffer;
	commandDesc.pResponseBuffer_phys = pResponseBuffer_phys;	
	commandDesc.responseBufferSize = allocCmdInfo.responseBufferSize;	
	*pCommandDesc = commandDesc;	
	pQueueInfo->ppResponseDescList[pQueueInfo->currentResponse] = &pCommandDesc->responseDesc;	
	pQueueInfo->currentCommand++;	
	pQueueInfo->pCommandRing->index = (pQueueInfo->pCommandRing->index>=65534) ? 0 : pQueueInfo->pCommandRing->index+1;
	*ppCommandDesc = pCommandDesc;	
	return 0;
}
int virtio_gpu_queue_polling_enable(struct virtio_gpu_queue_info* pQueueInfo){
	if (!pQueueInfo)
		return -1;
	if (pcie_msix_disable(gpuInfo.pMsgControl)!=0)
		return -1;
	if (pcie_msix_disable_entry(gpuInfo.location, gpuInfo.pMsgControl, pQueueInfo->msixVector)!=0)
		return -1;
	pQueueInfo->pollingEnabled = 1;
	gpuInfo.pBaseRegisters->queue_select = 0x0;
	gpuInfo.pBaseRegisters->queue_msix_vector = 0xFFFF;
	gpuInfo.pBaseRegisters->queue_enable = 0x01;
	pQueueInfo->pResponseRing->flags = VIRTIO_GPU_RESPONSE_RING_FLAG_NO_NOTIFY;
	return 0;
}
int virtio_gpu_queue_polling_disable(struct virtio_gpu_queue_info* pQueueInfo){
	if (!pQueueInfo)
		return -1;
	if (pcie_msix_enable(gpuInfo.pMsgControl)!=0)
		return -1;
	if (pcie_msix_enable_entry(gpuInfo.location, gpuInfo.pMsgControl, pQueueInfo->msixVector)!=0)
		return -1;
	pQueueInfo->pollingEnabled = 0;
	gpuInfo.pBaseRegisters->queue_select = 0x0;
	gpuInfo.pBaseRegisters->queue_msix_vector = pQueueInfo->msixVector;
	gpuInfo.pBaseRegisters->queue_enable = 0x01;
	pQueueInfo->pResponseRing->flags = 0x0;
	return 0;
}
int virtio_gpu_queue_yield_until_response(struct virtio_gpu_command_desc* pCommandDesc){
	if (!pCommandDesc)
		return -1;
	struct virtio_gpu_queue_info* pQueueInfo = pCommandDesc->pQueueInfo;
	if (!pQueueInfo)
		return -1;
	static struct mutex_t mutex = {0};
	if (!pQueueInfo->pollingEnabled){
		mutex_lock_isr_safe(&mutex);
	}
	while (!pCommandDesc->responseDesc.responseComplete){
		if (!pQueueInfo->pollingEnabled)
			continue;
		virtio_gpu_response_queue_interrupt(pQueueInfo->isrVector);
	}
	if (!pQueueInfo->pollingEnabled)
		mutex_unlock_isr_safe(&mutex);
	return 0;
}
int virtio_gpu_subsystem_read_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pColor){
	if (!pColor)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t gpuId = pSubsystemMonitorDesc->gpuId;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, pMonitorDesc->resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t pixelOffset = (position.y*pMonitorDesc->scanoutInfo.resolution.width)+position.x;
	volatile struct uvec4_8* pFramebuffer = (struct uvec4_8*)pResourceDesc->resourceInfo.normal.pBuffer;
	volatile struct uvec4_8* pPixel = pFramebuffer+pixelOffset;
	*pColor = *pPixel;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_write_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8 color){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t gpuId = pSubsystemMonitorDesc->gpuId;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, pMonitorDesc->resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsytem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t pixelOffset = (position.y*pMonitorDesc->scanoutInfo.resolution.width)+position.x;
	volatile struct uvec4_8* pFramebuffer = (struct uvec4_8*)pResourceDesc->resourceInfo.normal.pBuffer;
	if (!pFramebuffer){
		serial_print(0, "invalid framebuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	volatile struct uvec4_8* pPixel = pFramebuffer+pixelOffset;
	*pPixel = color;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_sync(uint64_t monitorId, struct uvec4 rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t gpuId = pSubsystemMonitorDesc->gpuId;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, pMonitorDesc->resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);	
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_box boxRect = {0};
	boxRect.x = rect.x;
	boxRect.y = rect.y;
	boxRect.width = rect.z;
	boxRect.height = rect.w;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_sync(pResourceDesc, boxRect, &responseHeader)!=0){
		serial_print(0, "failed to sync virtual I/O GPU host controller resource data virtual I/O GPU host controller driver fault\r\n");
		printf("failed to sync virtual I/O GPU framebuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type name";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		serial_print(0, "failed to sync virtual I/O GPU host controller resource data\r\n");
		printf("failed to sync virtual I/O GPU framebuffer (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_commit(uint64_t monitorId, struct uvec4 rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t gpuId = pSubsystemMonitorDesc->gpuId;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, pMonitorDesc->resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_box boxRect = {0};
	boxRect.x = rect.x;
	boxRect.y = rect.y;
	boxRect.width = rect.z;
	boxRect.height = rect.w;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_commit(pResourceDesc, boxRect, &responseHeader)!=0){
		printf("failed to commit to virtual I/O GPU frameubuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to commit to virtual I/O GPU framebuffer (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push(uint64_t monitorId, struct uvec4 rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t gpuId = pSubsystemMonitorDesc->gpuId;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, pMonitorDesc->resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_rect virtioGpuRect = {0};
	virtioGpuRect.x = rect.x;
	virtioGpuRect.y = rect.y;
	virtioGpuRect.width = rect.z;
	virtioGpuRect.height = rect.w;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_push(pResourceDesc, virtioGpuRect, &responseHeader)!=0){
		printf("failed to push to virtual I/O GPU framebuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";	
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to push to virtual I/O GPU framebuffer (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_set_scanout(uint64_t monitorId, uint64_t resourceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
		printf("failed to get GPU host controller monitor descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t gpuId = pSubsystemMonitorDesc->gpuId;
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controlller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc){
		printf("GPU subsystem monitor descriptor not linked with virtual I/O GPU host controller monitor descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_set_scanout(pMonitorDesc->scanoutId, pResourceDesc, &responseHeader)!=0){
		printf("failed to set virtual I/O GPU host controller monitor %d scanout to resource %d\r\n", monitorId, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to set virtual I/O GPU host controller monitor %d scanout to resource %d (%s)\r\n", monitorId, resourceId, responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_cmd_context_init(uint64_t gpuId, uint64_t cmdContextId, uint64_t commandListSize){
	if (!commandListSize)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");	
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pSubsystemCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pSubsystemCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_cmd_context_desc* pCmdContextDesc = (struct virtio_gpu_cmd_context_desc*)0x0;
	if (virtio_gpu_cmd_context_init(commandListSize, &pCmdContextDesc)!=0){
		printf("failed to initialize virtual I/O GPU host controller command context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pSubsystemCmdContextDesc->extra = (uint64_t)pCmdContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_cmd_context_deinit(uint64_t gpuId, uint64_t cmdContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_cmd_context_desc* pSubsystemCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pSubsystemCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_cmd_context_desc* pCmdContextDesc = (struct virtio_gpu_cmd_context_desc*)pSubsystemCmdContextDesc->extra;
	if (!pCmdContextDesc){
		printf("GPU subsystem command context descriptor not linked with virtual I/O GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_cmd_context_deinit(pCmdContextDesc)!=0){
		printf("failed to deinitialize virtual I/O GPU host controller command context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_cmd_context_reset(uint64_t gpuId, uint64_t cmdContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pSubsystemCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pSubsystemCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_cmd_context_desc* pCmdContextDesc = (struct virtio_gpu_cmd_context_desc*)pSubsystemCmdContextDesc->extra;
	if (!pCmdContextDesc){
		printf("GPU subsystem command context descriptor not linked with virtual I/O GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_cmd_context_reset(pCmdContextDesc)!=0){
		printf("failed to reset GPU host controller command context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_set_framebuffer_state_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_framebuffer_state_list_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_framebuffer_state_list_command* pGlCommand = (struct virtio_gpu_gl_set_framebuffer_state_list_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_framebuffer_state_list(pGlCommand, pCommandInfo->colorBufferCount, pCommandInfo->depthStencilHandle, (uint32_t*)pCommandInfo->pColorBufferHandleList);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_set_viewport_state_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_viewport_state_list_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_viewport_state_list_command* pGlCommand = (struct virtio_gpu_gl_set_viewport_state_list_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_viewport_state_list(pGlCommand, pCommandInfo->startSlot, pCommandInfo->viewportStateCount, (struct virtio_gpu_gl_viewport_state*)pCommandInfo->pViewportStateList);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_set_scissor_state_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_scissor_state_list_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_scissor_state_list_command* pGlCommand = (struct virtio_gpu_gl_set_scissor_state_list_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_scissor_state_list(pGlCommand, pCommandInfo->startSlot, pCommandInfo->scissorStateCount, (struct virtio_gpu_gl_scissor_state*)pCommandInfo->pScissorStateList);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_set_vertex_buffer_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_vertex_buffer_list_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return-1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_vertex_buffer_list_command* pGlCommand = (struct virtio_gpu_gl_set_vertex_buffer_list_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_vertex_buffer_list(pGlCommand, pCommandInfo->vertexBufferCount, (struct virtio_gpu_gl_vertex_buffer*)pCommandInfo->pVertexBufferList);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_draw_vbo_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_draw_vbo_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_draw_vbo_command* pGlCommand = (struct virtio_gpu_gl_draw_vbo_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	pGlCommand->header.opcode = VIRTIO_GPU_GL_CMD_DRAW_VBO;
	pGlCommand->header.length = 12;
	pGlCommand->header.object_type = 0;
	pGlCommand->commandInfo = *(struct virtio_gpu_gl_draw_vbo_command_info*)pCommandInfo->pDrawVboInfo;
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_clear_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_clear_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_clear_command* pGlCommand = (struct virtio_gpu_gl_clear_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_clear(pGlCommand, pCommandInfo->buffers, pCommandInfo->color, pCommandInfo->depth, pCommandInfo->stencil);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_resource_inline_write_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_resource_inline_write_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_resource_inline_write_command* pGlCommand = (struct virtio_gpu_gl_resource_inline_write_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_resource_inline_write(pGlCommand, pCommandInfo->resourceId, pCommandInfo->level, pCommandInfo->stride, pCommandInfo->layer_stride, *(struct virtio_gpu_box*)&pCommandInfo->boxRect, pCommandInfo->pBuffer, pCommandInfo->bufferSize);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_set_sampler_view_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_sampler_view_list_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_sampler_view_list_command* pGlCommand = (struct virtio_gpu_gl_set_sampler_view_list_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_sampler_view_list(pGlCommand, pCommandInfo->shaderType, pCommandInfo->startSlot, pCommandInfo->samplerViewCount, (uint32_t*)pCommandInfo->samplerViewList);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;	
}
int virtio_gpu_subsystem_push_set_index_buffer_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_index_buffer_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_index_buffer_command* pGlCommand = (struct virtio_gpu_gl_set_index_buffer_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_index_buffer(pGlCommand, pCommandInfo->resourceId, pCommandInfo->length, pCommandInfo->offset);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_set_constant_buffer_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_constant_buffer_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_constant_buffer_command* pGlCommand = (struct virtio_gpu_gl_set_constant_buffer_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_constant_buffer(pGlCommand, pCommandInfo->shaderType, pCommandInfo->index, pCommandInfo->bufferSize, pCommandInfo->pBuffer);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_bind_sampler_state_list_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_bind_sampler_state_list_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_bind_sampler_state_list_command* pGlCommand = (struct virtio_gpu_gl_bind_sampler_state_list_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_bind_sampler_state_list(pGlCommand, pCommandInfo->shaderType, pCommandInfo->startSlot, pCommandInfo->samplerStateCount, (uint32_t*)pCommandInfo->samplerStateList);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_set_uniform_buffer_cmd(struct gpu_desc* pGpuDesc, struct virtio_gpu_cmd_context_desc* pCmdContextDesc, struct gpu_set_uniform_buffer_cmd_info* pCommandInfo){
	if (!pGpuDesc||!pCmdContextDesc||!pCommandInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_gl_set_uniform_buffer_command* pGlCommand = (struct virtio_gpu_gl_set_uniform_buffer_command*)0x0;
	virtio_gpu_cmd_context_get_current_cmd(pCmdContextDesc, (unsigned char**)&pGlCommand);
	virtio_gpu_gl_write_set_uniform_buffer(pGlCommand, pCommandInfo->shaderType, pCommandInfo->index, pCommandInfo->offset, pCommandInfo->length, pCommandInfo->resourceId);
	virtio_gpu_cmd_context_push_cmd(pCmdContextDesc, (pGlCommand->header.length+1)*sizeof(uint32_t));
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_push_cmd(uint64_t gpuId, uint64_t cmdContextId, struct gpu_cmd_info_header* pCommandInfo){
	if (!pCommandInfo)
		return -1;
	static const virtioGpuPushCmdFunc pushCmdVtable[]={
		[GPU_CMD_TYPE_SET_FRAMEBUFFER_STATE_LIST]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_framebuffer_state_list_cmd,
		[GPU_CMD_TYPE_SET_VIEWPORT_STATE_LIST]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_viewport_state_list_cmd,
		[GPU_CMD_TYPE_SET_SCISSOR_STATE_LIST]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_scissor_state_list_cmd,
		[GPU_CMD_TYPE_SET_VERTEX_BUFFER_LIST]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_vertex_buffer_list_cmd,
		[GPU_CMD_TYPE_DRAW_VBO]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_draw_vbo_cmd,
		[GPU_CMD_TYPE_CLEAR]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_clear_cmd,
		[GPU_CMD_TYPE_RESOURCE_INLINE_WRITE]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_resource_inline_write_cmd,
		[GPU_CMD_TYPE_SET_SAMPLER_VIEW_LIST]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_sampler_view_list_cmd,
		[GPU_CMD_TYPE_SET_INDEX_BUFFER]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_index_buffer_cmd,
		[GPU_CMD_TYPE_SET_CONSTANT_BUFFER]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_constant_buffer_cmd,
		[GPU_CMD_TYPE_BIND_SAMPLER_STATE_LIST]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_bind_sampler_state_list_cmd,
		[GPU_CMD_TYPE_SET_UNIFORM_BUFFER]=(virtioGpuPushCmdFunc)virtio_gpu_subsystem_push_set_uniform_buffer_cmd,
	};
	uint64_t commandType = pCommandInfo->commandType;
	if (commandType>sizeof(pushCmdVtable)/sizeof(virtioGpuPushCmdFunc))
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pSubsystemCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pSubsystemCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_cmd_context_desc* pCmdContextDesc = (struct virtio_gpu_cmd_context_desc*)pSubsystemCmdContextDesc->extra;
	if (!pCmdContextDesc){
		printf("GPU subsystem command context descriptor not linked with virtual I/O GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	virtioGpuPushCmdFunc pushCmdFunc = pushCmdVtable[commandType];
	if (!pushCmdFunc){
		printf("virtual I/O GPU host controller driver lacks support of command with type 0x%x\r\n", commandType);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pushCmdFunc(pGpuDesc, pCmdContextDesc, pCommandInfo)!=0){
		printf("failed to push virtual I/O GPU host controller command with type: 0x%x\r\n", commandType);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_cmd_context_submit(uint64_t gpuId, uint64_t contextId, uint64_t cmdContextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pSubsystemContextDesc = (struct gpu_context_desc*)0x0;
	if (gpu_context_get_desc(gpuId, contextId, &pSubsystemContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)pSubsystemContextDesc->extra;
	if (!pContextDesc){
		printf("GPU subsystem context descriptor not linked with virtual I/O GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_cmd_context_desc* pSubsystemCmdContextDesc = (struct gpu_cmd_context_desc*)0x0;
	if (gpu_cmd_context_get_desc(gpuId, cmdContextId, &pSubsystemCmdContextDesc)!=0){
		printf("failed to get GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_cmd_context_desc* pCmdContextDesc = (struct virtio_gpu_cmd_context_desc*)pSubsystemCmdContextDesc->extra;
	if (!pCmdContextDesc){
		printf("GPU subsystem command context descriptor not linked with virtual I/O GPU host controller command context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;	
	}
	struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)pCmdContextDesc->pCommandBuffer;
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId;
	pCommandBuffer->size = pCmdContextDesc->currentCommandOffset;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_submit(pCmdContextDesc->pCommandBuffer, &responseHeader)!=0){
		printf("failed to submit VirGL command list to virtual I/O GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to submit VirGL command list to virtual I/O GPU host controller (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_rasterizer_state_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_rasterizer_state_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_object_desc* pSubsystemSurfaceObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuInfo.gpuId, pCreateObjectInfo->surfaceObjectId, &pSubsystemSurfaceObjectDesc)!=0){
		printf("failed to get GPU host controller surface object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pSurfaceObjectDesc = (struct virtio_gpu_object_desc*)pSubsystemSurfaceObjectDesc->extra;
	if (!pSurfaceObjectDesc){
		printf("GPU subsystem surface object descriptor not linked with virtual I/O GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_gl_create_rasterizer_object(pSurfaceObjectDesc, *(struct virtio_gpu_gl_rasterizer_state*)pCreateObjectInfo->pRasterizerState, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller rasterizer state object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_dsa_state_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_dsa_state_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_object_desc* pSubsystemSurfaceObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuInfo.gpuId, pCreateObjectInfo->surfaceObjectId, &pSubsystemSurfaceObjectDesc)!=0){
		printf("failed to get GPU host controller surface object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pSurfaceObjectDesc = (struct virtio_gpu_object_desc*)pSubsystemSurfaceObjectDesc->extra;
	if (!pSurfaceObjectDesc){
		printf("GPU subsystem surface object descriptor not linked with virtual I/O GPU host controller surface object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_gl_create_dsa_object(pSurfaceObjectDesc, *(struct virtio_gpu_gl_dsa_state*)pCreateObjectInfo->pDsaState, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller DSA state object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_blend_state_list_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_blend_state_list_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header*pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_object_desc* pSubsystemSurfaceObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuInfo.gpuId, pCreateObjectInfo->surfaceObjectId, &pSubsystemSurfaceObjectDesc)!=0){
		printf("failed to get GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pSurfaceObjectDesc = (struct virtio_gpu_object_desc*)pSubsystemSurfaceObjectDesc->extra;
	if (!pSurfaceObjectDesc){
		printf("GPU subsystem surface object descriptor not linked with virtual I/O GPU host controller surface object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_gl_create_blend_object(pSurfaceObjectDesc, 0x0, 0x0, pCreateObjectInfo->blendStateCount, (struct virtio_gpu_gl_blend_state*)pCreateObjectInfo->pBlendStateList, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller blend state list object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_shader_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_shader_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_object_desc* pSubsystemSurfaceObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuInfo.gpuId, pCreateObjectInfo->surfaceObjectId, &pSubsystemSurfaceObjectDesc)!=0){
		printf("failed to get GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pSurfaceObjectDesc = (struct virtio_gpu_object_desc*)pSubsystemSurfaceObjectDesc->extra;
	if (!pSurfaceObjectDesc){
		printf("GPU subsystem surface object descriptor not linked with virtual I/O GPU host controller surface object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_create_shader_info createShaderInfo = {0};
	memset((void*)&createShaderInfo, 0, sizeof(struct virtio_gpu_create_shader_info));
	createShaderInfo.pShaderCode = pCreateObjectInfo->pShaderCode;
	createShaderInfo.shaderCodeSize = pCreateObjectInfo->shaderCodeSize;
	createShaderInfo.tokenCount = pCreateObjectInfo->shaderCodeSize;
	createShaderInfo.shaderType = pCreateObjectInfo->shaderType;
	if (virtio_gpu_gl_create_shader_object(pSurfaceObjectDesc, createShaderInfo, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller shader object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_vertex_element_list_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_vertex_element_list_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_object_desc* pSubsystemSurfaceObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuInfo.gpuId, pCreateObjectInfo->surfaceObjectId, &pSubsystemSurfaceObjectDesc)!=0){
		printf("failed to get GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pSurfaceObjectDesc = (struct virtio_gpu_object_desc*)pSubsystemSurfaceObjectDesc->extra;
	if (!pSurfaceObjectDesc){
		printf("GPU subsystem surface object descriptor not linked with virtual I/O GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_gl_create_vertex_element_list_object(pSurfaceObjectDesc, pCreateObjectInfo->vertexElementCount, (struct virtio_gpu_gl_vertex_element*)pCreateObjectInfo->pVertexElementList, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller vertex element list object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_sampler_view_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_sampler_view_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader){
		return -1;
	}
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuInfo.gpuId, pCreateObjectInfo->resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_swizzle swizzle = *(struct virtio_gpu_swizzle*)&pCreateObjectInfo->swizzle;
	if (virtio_gpu_gl_create_sampler_view_object(pResourceDesc, pCreateObjectInfo->format, pCreateObjectInfo->firstLevel, pCreateObjectInfo->lastLevel, swizzle, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller sampler view object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_sampler_state_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_sampler_state_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (virtio_gpu_gl_create_sampler_state_object(pContextDesc, *(struct virtio_gpu_gl_sampler_state*)&pCreateObjectInfo->samplerState, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virutal I/O GPU host controller sampler state object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_surface_object_create(struct virtio_gpu_context_desc* pContextDesc, struct gpu_create_surface_object_info* pCreateObjectInfo, struct virtio_gpu_object_desc* pObjectDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pCreateObjectInfo||!pObjectDesc||!pResponseHeader){
		return -1;
	}
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuInfo.gpuId, pCreateObjectInfo->resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem GPU host controller resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_gl_create_surface_object(pContextDesc, pResourceDesc, pObjectDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller surface object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_object_create(uint64_t gpuId, uint64_t contextId, uint64_t objectId, struct gpu_create_object_info* pCreateObjectInfo){
	if (!pCreateObjectInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pSubsystemContextDesc = (struct gpu_context_desc*)0x0;
	if (gpu_context_get_desc(gpuId, contextId, &pSubsystemContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_object_desc* pSubsystemObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuId, objectId, &pSubsystemObjectDesc)!=0){
		printf("failed to get GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)pSubsystemContextDesc->extra;
	if (!pContextDesc){
		printf("GPU subsystem context descriptor not linked with virtual I/O GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t objectType = pCreateObjectInfo->header.objectType;
	static virtioGpuSubsystemCreateObjectFunc createObjectFuncList[]={
		[GPU_OBJECT_TYPE_RASTERIZER_STATE]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_rasterizer_state_object_create,
		[GPU_OBJECT_TYPE_DSA_STATE]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_dsa_state_object_create,
		[GPU_OBJECT_TYPE_SHADER]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_shader_object_create,
		[GPU_OBJECT_TYPE_VERTEX_ELEMENT_LIST]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_vertex_element_list_object_create,
		[GPU_OBJECT_TYPE_SAMPLER_VIEW]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_sampler_view_object_create,
		[GPU_OBJECT_TYPE_SAMPLER_STATE]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_sampler_state_object_create,
		[GPU_OBJECT_TYPE_SURFACE]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_surface_object_create,
		[GPU_OBJECT_TYPE_BLEND_STATE_LIST]=(virtioGpuSubsystemCreateObjectFunc)virtio_gpu_subsystem_blend_state_list_object_create,
	};
	if (!objectType||objectType>sizeof(createObjectFuncList)/sizeof(virtioGpuSubsystemCreateObjectFunc)){
		printf("invalid or unsupported object type: 0x%x\r\n", objectType);
		mutex_unlock(&mutex);
		return -1;
	}
	virtioGpuSubsystemCreateObjectFunc createObjectFunc = createObjectFuncList[objectType];
	if (!createObjectFunc){
		printf("invalid or unsupported object type: 0x%x\r\n", objectType);
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pObjectDesc = (struct virtio_gpu_object_desc*)kmalloc(sizeof(struct virtio_gpu_object_desc));
	if (!pObjectDesc){
		printf("failed to allocate virtual I/O GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pObjectDesc, 0, sizeof(struct virtio_gpu_object_desc));
	pObjectDesc->objectId = objectId;
	pObjectDesc->objectType = objectType;
	pObjectDesc->pContextDesc = pContextDesc;
	struct virtio_gpu_response_header responseHeader = {0};
	if (createObjectFunc(pContextDesc, pCreateObjectInfo, pObjectDesc, &responseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller object with type: 0x%x\r\n", objectType);
		kfree((void*)pObjectDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to create virtual I/O GPU host controller object with type: 0x%x (%s)\r\n", objectType, responseTypeName);
		kfree((void*)pObjectDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pSubsystemObjectDesc->extra = (uint64_t)pObjectDesc;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_object_delete(uint64_t gpuId, uint64_t objectId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_object_desc* pSubsystemObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuId, objectId, &pSubsystemObjectDesc)!=0){
		printf("failed to get GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pObjectDesc = (struct virtio_gpu_object_desc*)pSubsystemObjectDesc->extra;
	if (!pObjectDesc){
		printf("GPU subsystem object descriptor not linked with virtual I/O GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_gl_delete_object(pObjectDesc, &responseHeader)!=0){
		printf("failed to delete virtual I/O GPU host controller object with type: 0x%x\r\n", pSubsystemObjectDesc->objectType);
		kfree((void*)pObjectDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to delete virtaul I/O GPU host controller object with type: 0x%x (%s)\r\n", responseTypeName);
		kfree((void*)pObjectDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	kfree((void*)pObjectDesc);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_object_bind(uint64_t gpuId, uint64_t objectId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_object_desc* pSubsystemObjectDesc = (struct gpu_object_desc*)0x0;
	if (gpu_object_get_desc(gpuId, objectId, &pSubsystemObjectDesc)!=0){
		printf("failed to get GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_object_desc* pObjectDesc = (struct virtio_gpu_object_desc*)pSubsystemObjectDesc->extra;
	if (!pObjectDesc){
		printf("GPU subsystem object descriptor not linked with virtual I/O GPU host controller object descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_gl_bind_object(pObjectDesc, &responseHeader)!=0){
		printf("failed to bind virtual I/O GPU host controller object\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to bind virtual I/O GPU host controller object (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex); 
	return 0;
}
int virtio_gpu_subsystem_context_create(uint64_t gpuId, uint64_t contextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pSubsystemContextDesc = (struct gpu_context_desc*)0x0;
	if (gpu_context_get_desc(gpuId, contextId, &pSubsystemContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)kmalloc(sizeof(struct virtio_gpu_context_desc));
	if (!pContextDesc){
		printf("failed to allocate virtual I/O GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pContextDesc, 0, sizeof(struct virtio_gpu_context_desc));
	pContextDesc->contextId = contextId;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_create_context(contextId, &responseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller context\r\n");
		kfree((void*)pContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to create virtual I/O GPU host controller context (%s)\r\n", responseTypeName);
		kfree((void*)pContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	pSubsystemContextDesc->extra = (uint64_t)pContextDesc;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_context_delete(uint64_t gpuId, uint64_t contextId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pSubsystemContextDesc = (struct gpu_context_desc*)0x0;
	if (gpu_context_get_desc(gpuId, contextId, &pSubsystemContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)pSubsystemContextDesc->extra;
	if (!pContextDesc){
		printf("GPU subsystem context descriptor not linked with virtual I/O GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_delete_context(pContextDesc, &responseHeader)!=0){
		printf("failed to delete virtual I/O GPU host controller context\r\n");
		kfree((void*)pContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to delete virtual I/O GPU host controller context (%s)\r\n", responseTypeName);
		kfree((void*)pContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	kfree((void*)pContextDesc);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_context_attach_resource(uint64_t gpuId, uint64_t contextId, uint64_t resourceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_context_desc* pSubsystemContextDesc = (struct gpu_context_desc*)0x0;
	if (gpu_context_get_desc(gpuId, contextId, &pSubsystemContextDesc)!=0){
		printf("failed to get GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)pSubsystemContextDesc->extra;
	if (!pContextDesc){
		printf("GPU subsystem context descriptor not linked with virtual I/O GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controlller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_context_attach_resource(pContextDesc, pResourceDesc, &responseHeader)!=0){
		printf("failed to attach virtual I/O GPU host controller resource to context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to attach virtual I/O GPU host controller resource to context (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_resource_create(uint64_t gpuId, uint64_t resourceId, struct gpu_create_resource_info subsystemCreateResourceInfo){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	serial_print(0, "getting virtual I/O GPU host controller resource descriptor\r\n");
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller subsystem resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	serial_print(0, "done getting virtual I/O GPU host controller resource descriptor\r\n");
	struct gpu_context_desc* pSubsystemContextDesc = (struct gpu_context_desc*)0x0;
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)0x0;
	if (subsystemCreateResourceInfo.resourceInfo.contextId){
		if (gpu_context_get_desc(gpuId, subsystemCreateResourceInfo.resourceInfo.contextId, &pSubsystemContextDesc)!=0){
			printf("failed to get virtual I/O GPU host controller context descriptor\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		serial_print(0, "getting virtual I/O GPU host controller context descriptor\r\n");
		if (gpu_context_get_desc(gpuId, subsystemCreateResourceInfo.resourceInfo.contextId, &pSubsystemContextDesc)!=0){
			printf("failed to get GPU host controller subsystem context descriptor\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		pContextDesc = (struct virtio_gpu_context_desc*)pSubsystemContextDesc->extra;
		if (!pContextDesc){
			printf("GPU subsystem context descriptor not linked with virtual I/O GPU host controller context descriptor\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		serial_print(0, "done getting virtual I/O GPU host controller context descriptor\r\n");
	}
	struct virtio_gpu_create_resource_info createResourceInfo = {0};
	memset((void*)&createResourceInfo, 0, sizeof(struct virtio_gpu_create_resource_info));
	createResourceInfo.resourceInfo.resourceType = subsystemCreateResourceInfo.resourceInfo.resourceType;
	switch (subsystemCreateResourceInfo.resourceInfo.resourceType){
		case GPU_RESOURCE_TYPE_2D:
		case GPU_RESOURCE_TYPE_3D:{
			createResourceInfo.resourceInfo.normal.format = subsystemCreateResourceInfo.resourceInfo.normal.format;
			createResourceInfo.resourceInfo.normal.width = subsystemCreateResourceInfo.resourceInfo.normal.width;
			createResourceInfo.resourceInfo.normal.height = subsystemCreateResourceInfo.resourceInfo.normal.height;
			createResourceInfo.resourceInfo.normal.depth = subsystemCreateResourceInfo.resourceInfo.normal.depth;
			createResourceInfo.resourceInfo.normal.arraySize = subsystemCreateResourceInfo.resourceInfo.normal.arraySize;
			createResourceInfo.resourceInfo.normal.target = subsystemCreateResourceInfo.resourceInfo.normal.target;
			createResourceInfo.resourceInfo.normal.bind = subsystemCreateResourceInfo.resourceInfo.normal.bind;
			createResourceInfo.resourceInfo.normal.flags = subsystemCreateResourceInfo.resourceInfo.normal.flags;
			break;	
		}		  
		case GPU_RESOURCE_TYPE_BLOB:{
			createResourceInfo.resourceInfo.blob.resourceId = subsystemCreateResourceInfo.resourceInfo.blob.resourceId;
		       	createResourceInfo.resourceInfo.blob.memFlags = subsystemCreateResourceInfo.resourceInfo.blob.memFlags;
			createResourceInfo.resourceInfo.blob.mapFlags = subsystemCreateResourceInfo.resourceInfo.blob.mapFlags;
			createResourceInfo.resourceInfo.blob.pBlobBuffer = subsystemCreateResourceInfo.resourceInfo.blob.pBlobBuffer;
			createResourceInfo.resourceInfo.blob.blobBufferSize = subsystemCreateResourceInfo.resourceInfo.blob.blobBufferSize;
			break;		    
		};
		default:{
			printf("unsupported GPU host controller resource type: 0x%x\r\n", subsystemCreateResourceInfo.resourceInfo.resourceType);
			mutex_unlock(&mutex);
			return -1;
		};	
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)kmalloc(sizeof(struct virtio_gpu_resource_desc));
	if (!pResourceDesc){
		printf("failed to allocate virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pResourceDesc, 0, sizeof(struct virtio_gpu_resource_desc));
	pResourceDesc->resourceId = resourceId;
	pResourceDesc->resourceInfo.resourceType = createResourceInfo.resourceInfo.resourceType;
	pResourceDesc->resourceInfo = createResourceInfo.resourceInfo;
	createResourceInfo.pContextDesc = pContextDesc;
	createResourceInfo.pResourceDesc = pResourceDesc;
	struct virtio_gpu_response_header responseHeader = {0};
	serial_print(0, "creating virtual I/O GPU host controller resource\r\n");
	if (virtio_gpu_create_resource(createResourceInfo, resourceId, &responseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller resource\r\n");
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		while (1){};
		printf("failed to create virtual I/O GPU host controller resource (%s)\r\n", responseTypeName);
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	serial_print(0, "done creating virtual I/O GPU host controller resource\r\n");
	pSubsystemResourceDesc->extra = (uint64_t)pResourceDesc;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_resource_delete(uint64_t gpuId, uint64_t resourceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_delete_resource(pResourceDesc, &responseHeader)!=0){
		printf("failed to delete virtual I/O GPU host controller resource\r\n");
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to delete virtual I/O GPU host controller resource\r\n");
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	kfree((void*)pResourceDesc);
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_resource_attach_backing(uint64_t gpuId, uint64_t resourceId, unsigned char* pBuffer, uint64_t bufferSize){
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
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_attach_resource_backing(pResourceDesc, pBuffer, bufferSize, &responseHeader)!=0){
		printf("failed to attach physical pages to virtaul I/O GPU host controller resource\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to attach physical pages to virtual I/O GPU host controller resource (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_resource_flush(uint64_t gpuId, uint64_t resourceId, struct gpu_rect rect){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	struct virtio_gpu_rect virtioGpuRect = {0};
	virtioGpuRect.x = rect.x;
	virtioGpuRect.y = rect.y;
	virtioGpuRect.width = rect.width;
	virtioGpuRect.height = rect.height;
	if (virtio_gpu_resource_flush(pResourceDesc, virtioGpuRect, &responseHeader)!=0){
		printf("failed to flush virtual I/O GPU host controller resource data to virtual I/O GPU host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to flush virtual I/O GPU host controller resource data to virtual I/O GPU host controller (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_transfer_to_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_to_device_info transferToDeviceInfo){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_context_desc* pContextDesc = pResourceDesc->pContextDesc;
	if (!pContextDesc){
		printf("virtual I/O GPU host controller resource descriptor not linked with virtual I/O GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header responseHeader = {0};
	switch (pResourceDesc->resourceInfo.resourceType){
		case VIRTIO_GPU_RESOURCE_TYPE_2D:{
			struct virtio_gpu_rect virtioGpuRect = {0};
			virtioGpuRect.x = transferToDeviceInfo.rect.x;
			virtioGpuRect.y = transferToDeviceInfo.rect.y;
			virtioGpuRect.width = transferToDeviceInfo.rect.width;
			virtioGpuRect.height = transferToDeviceInfo.rect.height;
			if (virtio_gpu_transfer_h2d_2d(pResourceDesc, virtioGpuRect, transferToDeviceInfo.offset, &responseHeader)!=0){
				printf("failed to transfer resource data to virtual I/O GPU host controller\r\n");	
				mutex_unlock(&mutex);	
				return -1;
			}
			break;
		}
		case VIRTIO_GPU_RESOURCE_TYPE_3D:{
			struct virtio_gpu_box virtioGpuBox = {0};
			virtioGpuBox.x = transferToDeviceInfo.boxRect.x;
			virtioGpuBox.y = transferToDeviceInfo.boxRect.y;
			virtioGpuBox.z = transferToDeviceInfo.boxRect.z;
			virtioGpuBox.width = transferToDeviceInfo.boxRect.width;
			virtioGpuBox.height = transferToDeviceInfo.boxRect.height;
			virtioGpuBox.depth = transferToDeviceInfo.boxRect.depth;
			struct virtio_gpu_gl_transfer_3d_command transferCommand = {0};
			memset((void*)&transferCommand, 0, sizeof(struct virtio_gpu_gl_transfer_3d_command));
			transferCommand.resource_id = pResourceDesc->resourceId;
			transferCommand.transfer_type = VIRTIO_GPU_GL_TRANSFER_WRITE;
			transferCommand.direction = VIRTIO_GPU_GL_TRANSFER_DIRECTION_H2D;
			transferCommand.box_rect = virtioGpuBox;
			if (virtio_gpu_gl_transfer_3d(pContextDesc, transferCommand, &responseHeader)!=0){
				printf("failed to transfer 3D resource data to VirGL rendering backned\r\n");
				mutex_unlock(&mutex);
				return -1;
			}
			if (virtio_gpu_transfer_h2d_3d(pResourceDesc, virtioGpuBox, transferToDeviceInfo.offset, &responseHeader)!=0){
				printf("failed to transfer resource data to virtual I/O GPU host controller\r\n");
				mutex_unlock(&mutex);
				return -1;
			}				 
			break;
		}
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to transfer reosurce data to virtual I/O GPU host controller\r\n");	
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_transfer_from_device(uint64_t gpuId, uint64_t resourceId, struct gpu_transfer_from_device_info transferInfo){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_resource_desc* pSubsystemResourceDesc = (struct gpu_resource_desc*)0x0;
	if (gpu_resource_get_desc(gpuId, resourceId, &pSubsystemResourceDesc)!=0){
		printf("failed to get GPU host controller resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)pSubsystemResourceDesc->extra;
	if (!pResourceDesc){
		printf("GPU subsystem resource descriptor not linked with virtual I/O GPU host controller reosurce descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pResourceDesc->resourceInfo.resourceType!=VIRTIO_GPU_RESOURCE_TYPE_3D){
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_context_desc* pContextDesc = pResourceDesc->pContextDesc;
	if (!pContextDesc){
		printf("virtual I/O GPU host controller resource descriptor not attached to virtual I/O GPU host controller context descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_gl_transfer_3d_command transferCommand = {0};
	memset((void*)&transferCommand, 0, sizeof(struct virtio_gpu_gl_transfer_3d_command));
	struct virtio_gpu_box boxRect = {0};
	memset((void*)&boxRect, 0, sizeof(struct virtio_gpu_box));
	boxRect.x = transferInfo.boxRect.x;
	boxRect.y = transferInfo.boxRect.y;
	boxRect.z = transferInfo.boxRect.z;
	boxRect.width = transferInfo.boxRect.width;
	boxRect.height = transferInfo.boxRect.height;
	boxRect.depth = transferInfo.boxRect.depth;
	transferCommand.resource_id = pResourceDesc->resourceId;
	transferCommand.transfer_type = VIRTIO_GPU_GL_TRANSFER_READ;
	transferCommand.direction = VIRTIO_GPU_GL_TRANSFER_DIRECTION_D2H;
	transferCommand.box_rect = boxRect;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_gl_transfer_3d(pContextDesc, transferCommand, &responseHeader)!=0){
		printf("failed to transfer virtual I/O GPU host controller resource data to host physical pages\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to transfer virtual I/O GPU host controller resource data to host physical pages (%s)\r\n", responseTypeName);
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_subsystem_panic_entry(uint64_t driverId){
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(driverId, &pDriverDesc)!=0)
		return -1;
	if (virtio_gpu_queue_polling_enable(&gpuInfo.controlQueueInfo)!=0)
		return -1;
	gpuInfo.panicMode = 1;
	serial_print(0, "virtual I/O GPU host controller panic mode\r\n");
	return 0;
}
