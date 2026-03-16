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
#include "subsystem/subsystem.h"
#include "subsystem/gpu.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include "drivers/apic.h"
#include "drivers/pcie.h"
#include "drivers/virtio.h"
#include "drivers/gpu/virtio.h"
struct virtio_gpu_info gpuInfo = {0};
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
	if (virtio_gpu_init_resource_subsystem()!=0){
		printf("failed to initialize resource subsystem\r\n");
		return -1;
	}
	if (virtio_gpu_init_context_subsystem()!=0){
		printf("failed to initialize context subsystem\r\n");
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
	struct gpu_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct gpu_driver_vtable));
	vtable.readPixel = virtio_gpu_subsystem_read_pixel;
	vtable.writePixel = virtio_gpu_subsystem_write_pixel;
	vtable.sync = virtio_gpu_subsystem_sync;
	vtable.commit = virtio_gpu_subsystem_commit;
	vtable.push = virtio_gpu_subsystem_push;
	vtable.panic = virtio_gpu_subsystem_panic_entry;
	uint64_t driverId = 0;
	if (gpu_driver_register(vtable, &driverId)!=0){
		printf("failed to register virtual I/O GPU host controller driver\r\n");
		return -1;
	}
	gpuInfo.driverId = driverId;
	gpuInfo.driverVtable = vtable;
	uint64_t gpuId = 0;
	struct gpu_info subsystemGpuInfo = {0};
	memset((void*)&subsystemGpuInfo, 0, sizeof(struct gpu_info));
	subsystemGpuInfo.maxMonitorCount = VIRTIO_GPU_MAX_SCANOUT_COUNT;
	if (gpu_register(driverId, subsystemGpuInfo, &gpuId)!=0){
		printf("failed to register virtual I/O GPU\r\n");
		gpu_driver_unregister(driverId);
		return -1;
	}
	gpuInfo.pBaseRegisters->device_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY;
	struct virtio_gpu_features_legacy legacyFeatures = {0};	
	memset((void*)&legacyFeatures, 0, sizeof(struct virtio_gpu_features_legacy));
	legacyFeatures.virgl_support = 1;
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
	gpuInfo.pBaseRegisters->device_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_MODERN;
	struct virtio_gpu_features_modern modernFeatures = {0};
	memset((void*)&modernFeatures, 0, sizeof(struct virtio_gpu_features_modern));	
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
		unsigned char* pFramebuffer = (unsigned char*)0x0;
		uint64_t framebufferSize = pScanoutInfo->resolution.width*pScanoutInfo->resolution.height*sizeof(struct uvec4_8);
		if (virtualAlloc((uint64_t*)&pFramebuffer, framebufferSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
			printf("failed to allocate framebuffer\r\n");
			continue;	
		}	
		for (uint64_t i = 0;i<framebufferSize/sizeof(struct uvec4_8);i++){
			struct uvec4_8* pPixel = ((struct uvec4_8*)pFramebuffer)+i;
			*(uint32_t*)pPixel = 0;
		}
		struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)0x0;
		struct virtio_gpu_response_header responseHeader = {0};
		memset((void*)&responseHeader, 0, sizeof(struct virtio_gpu_response_header));
		struct virtio_gpu_create_resource_info createResourceInfo = {0};
		memset((void*)&createResourceInfo, 0, sizeof(struct virtio_gpu_create_resource_info));
		createResourceInfo.format = 0x02;
		createResourceInfo.width = pScanoutInfo->resolution.width;
		createResourceInfo.height = pScanoutInfo->resolution.height;
		createResourceInfo.resourceType = VIRTIO_GPU_RESOURCE_TYPE_2D;
		if (virtio_gpu_create_resource(createResourceInfo, &pResourceDesc, &responseHeader)!=0){
			printf("failed to create framebuffer resource\r\n");
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to create framebuffer resource (%s)\r\n", responseTypeName);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}	
		if (virtio_gpu_attach_resource_backing(pResourceDesc, pFramebuffer, framebufferSize, &responseHeader)!=0){
			printf("failled to attach resource backing\r\n");
			virtio_gpu_delete_resource(pResourceDesc, &responseHeader);	
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}	
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to attach resource backing (%s)\r\n", responseTypeName);	
			virtio_gpu_delete_resource(pResourceDesc, &responseHeader);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		if (virtio_gpu_set_scanout(i, pResourceDesc, &responseHeader)!=0){
			printf("failed to set virtual I/O GPU scanout\r\n");
			virtio_gpu_delete_resource(pResourceDesc, &responseHeader);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}	
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";	
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to set virtual I/O GPU scanout (%s)\r\n", responseTypeName);	
			virtio_gpu_delete_resource(pResourceDesc, &responseHeader);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;	
		}
		struct gpu_monitor_info subsystemMonitorInfo = {0};
		uint64_t monitorId = 0;
		memset((void*)&subsystemMonitorInfo, 0, sizeof(struct gpu_monitor_info));
		subsystemMonitorInfo.resolution.width = pScanoutInfo->resolution.width;
		subsystemMonitorInfo.resolution.height = pScanoutInfo->resolution.height;
		subsystemMonitorInfo.pFramebuffer = (struct uvec4_8*)pFramebuffer;
		if (gpu_monitor_register(gpuInfo.gpuId, subsystemMonitorInfo, &monitorId)!=0){
			printf("failed to register virtual I/O GPU monitor into monitor subsystem\r\n");
			virtio_gpu_delete_resource(pResourceDesc, &responseHeader);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
		if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0){
			printf("failed to get monitor subsystem monitor descriptor\r\n");
			gpu_monitor_unregister(monitorId);
			virtio_gpu_delete_resource(pResourceDesc, &responseHeader);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)kmalloc(sizeof(struct virtio_gpu_monitor_desc));
		if (!pMonitorDesc){
			printf("failed to allocate virutal I/O GPU monitor descriptor\r\n");
			gpu_monitor_unregister(monitorId);
			virtio_gpu_delete_resource(pResourceDesc, &responseHeader);
			virtualFree((uint64_t)pFramebuffer, framebufferSize);
			continue;
		}
		memset((void*)pMonitorDesc, 0, sizeof(struct virtio_gpu_monitor_desc));
		pMonitorDesc->scanoutId = i;
		pMonitorDesc->scanoutInfo = *pScanoutInfo;
		pMonitorDesc->pFramebufferResourceDesc = pResourceDesc;
		pSubsystemMonitorDesc->extra = (uint64_t)pMonitorDesc;
		clear();
		printf("virtual I/O GPU scanout initialized\r\n");
		printf("scanout resolution %d:%d\r\n", pScanoutInfo->resolution.width, pScanoutInfo->resolution.height);
		break;
		struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)0x0;
		if (virtio_gpu_create_context(&pContextDesc, &responseHeader)!=0){
			printf("failed to create virtual I/O GPU host controller context\r\n");
			continue;
		}
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to create virtual I/O GPU host controller context (%s)\r\n", responseTypeName);
			continue;
		}
		struct virtio_gpu_resource_desc* pFramebufferResourceDesc = (struct virtio_gpu_resource_desc*)0x0;
		memset((void*)&createResourceInfo, 0, sizeof(struct virtio_gpu_create_resource_info));
		createResourceInfo.pContextDesc = pContextDesc;
		createResourceInfo.resourceType = VIRTIO_GPU_RESOURCE_TYPE_3D;
		createResourceInfo.width = pScanoutInfo->resolution.width;
		createResourceInfo.height = pScanoutInfo->resolution.height;
		createResourceInfo.target = VIRTIO_GPU_TARGET_TEXTURE_2D;
		createResourceInfo.format = VIRTIO_GPU_PIXEL_FORMAT_B8G8R8X8_UNORM;
		createResourceInfo.bind = VIRTIO_GPU_BIND_TYPE_RENDER_TARGET|VIRTIO_GPU_BIND_TYPE_SCANOUT|VIRTIO_GPU_BIND_TYPE_SAMPLER_VIEW;
		createResourceInfo.flags = VIRTIO_GPU_RESOURCE_FLAG_Y_0_TOP;
		createResourceInfo.depth = 1;
		createResourceInfo.arraySize = 1;
		if (virtio_gpu_create_resource(createResourceInfo, &pFramebufferResourceDesc, &responseHeader)!=0){
			printf("failed to create virtual I/O GPU host controller framebuffer resource\r\n");
			while (1){};
		}
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to create virtual I/O GPU host controller vertex resource (%s)\r\n", responseTypeName);
			while (1){};
		}
		if (virtio_gpu_attach_resource_backing(pFramebufferResourceDesc, pFramebuffer, framebufferSize, &responseHeader)!=0){
			printf("failed to attach virtual I/O GPU host controller framebuffer backing\r\n");
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			while (1){};
		}
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to attach virtual I/O GPU host controller framebuffer backing (%s)\r\n", responseTypeName);
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			while (1){};
		}
		if (virtio_gpu_context_attach_resource(pContextDesc, pFramebufferResourceDesc, &responseHeader)!=0){
			printf("failed to link virtual I/O GPU host controller context with resource\r\n");
			virtio_gpu_delete_context(pContextDesc, &responseHeader);	
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			continue;
		}
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to link virtual I/O GPU host controller context with resource (%s)\r\n", responseTypeName);
			virtio_gpu_delete_context(pContextDesc, &responseHeader);
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			continue;
		}
		struct virtio_gpu_submit_3d_command* pCommandBuffer = (struct virtio_gpu_submit_3d_command*)0x0;
		if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
			printf("failed to allocate physical page for submit 3D command buffer\r\n");
			virtio_gpu_delete_context(pContextDesc, &responseHeader);
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			while (1){};
		}
		memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_submit_3d_command));
		pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_SUBMIT_3D;
		pCommandBuffer->commandHeader.context_id = pContextDesc->contextId+1;
		unsigned char* pCommandList = (unsigned char*)pCommandBuffer->commandData;
		uint64_t commandListSize = 0;
		struct virtio_gpu_gl_create_surface_object_command* pSurfaceObjectCommand = (struct virtio_gpu_gl_create_surface_object_command*)pCommandList;
		memset((void*)pSurfaceObjectCommand, 0, sizeof(struct virtio_gpu_gl_create_surface_object_command));
		pSurfaceObjectCommand->header.opcode = VIRTIO_GPU_CMD_CREATE_OBJECT;
		pSurfaceObjectCommand->header.object_type = VIRTIO_GPU_OBJECT_TYPE_SURFACE;
		pSurfaceObjectCommand->header.length = 5;
		pSurfaceObjectCommand->object_id = pFramebufferResourceDesc->resourceId+1;
		pSurfaceObjectCommand->resource_id = pFramebufferResourceDesc->resourceId+1;
		pSurfaceObjectCommand->format = pFramebufferResourceDesc->format;
		commandListSize+=(pSurfaceObjectCommand->header.length+1)*sizeof(uint32_t);
		struct virtio_gpu_gl_set_framebuffer_state_command* pFramebufferStateCommand = (struct virtio_gpu_gl_set_framebuffer_state_command*)(pCommandList+commandListSize);
		memset((void*)pFramebufferStateCommand, 0, sizeof(struct virtio_gpu_gl_set_framebuffer_state_command));
		pFramebufferStateCommand->header.opcode = VIRTIO_GPU_CMD_SET_FRAMEBUFFER_STATE;
		pFramebufferStateCommand->header.length = 3;
		pFramebufferStateCommand->color_buffer_count = 1;
		pFramebufferStateCommand->color_buffer_handle_list[0] = pFramebufferResourceDesc->resourceId+1;
		commandListSize+=(pFramebufferStateCommand->header.length+1)*sizeof(uint32_t);
		struct virtio_gpu_gl_set_viewport_state_command* pViewportStateCommand = (struct virtio_gpu_gl_set_viewport_state_command*)(pCommandList+commandListSize);
		memset((void*)pViewportStateCommand, 0, sizeof(struct virtio_gpu_gl_set_viewport_state_command));
		pViewportStateCommand->header.opcode = VIRTIO_GPU_CMD_SET_VIEWPORT_STATE;
		pViewportStateCommand->header.length = 7;
		pViewportStateCommand->scale.x = ((float)pScanoutInfo->resolution.width)/2.0f;
		pViewportStateCommand->scale.y = ((float)pScanoutInfo->resolution.height)/2.0f;
		pViewportStateCommand->scale.z = 1.0f;
		pViewportStateCommand->translate.x = ((float)pScanoutInfo->resolution.width)/2.0f;
		pViewportStateCommand->translate.y = ((float)pScanoutInfo->resolution.height)/2.0f;
		pViewportStateCommand->translate.z = 1.0f;
		commandListSize+=(pViewportStateCommand->header.length+1)*sizeof(uint32_t);
		struct virtio_gpu_gl_clear_command* pClearCommand = (struct virtio_gpu_gl_clear_command*)(pCommandList+commandListSize);
		memset((void*)pClearCommand, 0, sizeof(struct virtio_gpu_gl_clear_command));
		pClearCommand->header.opcode = VIRTIO_GPU_CMD_CLEAR;
		pClearCommand->header.length = 8;
		pClearCommand->buffers|=(1<<0)|(1<<2);
		pClearCommand->color.x = 1.0f;
		pClearCommand->color.y = 0.5f;
		pClearCommand->color.z = 0.75f;
		pClearCommand->depth = 1.0;
		commandListSize+=(pClearCommand->header.length+1)*sizeof(uint32_t);
		pCommandBuffer->size = commandListSize;
		if (virtio_gpu_submit(pCommandBuffer, &responseHeader)!=0){
			printf("failed to submit virtual I/O host controller command list\r\n");
			virtio_gpu_delete_context(pContextDesc, &responseHeader);
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			return -1;
		}
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to submit virtual I/O GPU host controller command list (%s)\r\n", responseTypeName);
			virtio_gpu_delete_context(pContextDesc, &responseHeader);
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			return -1;
		}
		if (virtio_gpu_set_scanout(i, pFramebufferResourceDesc, &responseHeader)!=0){
			printf("failed to set virtual I/O GPU host controller scanout\r\n");
			virtio_gpu_delete_context(pContextDesc, &responseHeader);
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			return -1;
		}
		if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
			const unsigned char* responseTypeName = "Unknown response type";
			virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
			printf("failed to set virtual I/O GPU host controller scanout (%s)\r\n", responseTypeName);
			virtio_gpu_delete_context(pContextDesc, &responseHeader);
			virtio_gpu_delete_resource(pFramebufferResourceDesc, &responseHeader);
			return -1;
		}
		virtio_gpu_resource_flush(pFramebufferResourceDesc, pScanoutInfo->resolution, &responseHeader);
		while (1){};
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
			case VIRTIO_BLOCK_BASE_REGISTERS:{
				gpuInfo.pBaseRegisters = (volatile struct virtio_gpu_base_registers*)blockBase;			 
				break;
			}
			case VIRTIO_BLOCK_NOTIFY_REGISTERS:{
				struct virtio_gpu_pcie_config_cap_notify* pNotifyCap = (struct virtio_gpu_pcie_config_cap_notify*)pConfigCap;
				gpuInfo.pNotifyRegisters = (volatile uint32_t*)blockBase;
				gpuInfo.notifyOffsetMultiplier = pNotifyCap->notify_offset_multiplier;
				break;				   
			}
			case VIRTIO_BLOCK_INTERRUPT_REGISTERS:{
				gpuInfo.pInterruptRegisters = (volatile struct virtio_gpu_interrupt_registers*)blockBase;
				break;				      
			}
			case VIRTIO_BLOCK_DEVICE_REGISTERS:{
				gpuInfo.pDeviceRegisters = (volatile struct virtio_gpu_device_registers*)blockBase;
				break;				
			}
			case VIRTIO_BLOCK_PCIE_REGISTERS:{
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
int virtio_gpu_init_resource_subsystem(void){
	struct subsystem_desc* pResourceSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pResourceSubsystemDesc, VIRTIO_GPU_MAX_RESOURCE_COUNT)!=0){
		printf("failed to initialize resource subsystem\r\n");
		return -1;
	}
	gpuInfo.pResourceSubsystemDesc = pResourceSubsystemDesc;	
	return 0;
}
int virtio_gpu_deinit_resource_subsystem(void){
	if (subsystem_deinit(gpuInfo.pResourceSubsystemDesc)!=0){
		printf("failed to deinitialize resource subsystem\r\n");
		return -1;
	}	
	return 0;
}
int virtio_gpu_init_context_subsystem(void){
	struct subsystem_desc* pSubsystemDesc = (struct subsystem_desc*)0x0;
	if (subsystem_init(&pSubsystemDesc, VIRTIO_GPU_MAX_CONTEXT_COUNT)!=0){
		printf("failed to initialize context subsystem\r\n");
		return -1;
	}
	gpuInfo.pContextSubsystemDesc = pSubsystemDesc;
	return 0;
}
int virtio_gpu_deinit_context_subsystem(void){
	if (subsystem_deinit(gpuInfo.pContextSubsystemDesc)!=0){
		printf("failed to deinitialize context subsystem\r\n");
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
int virtio_gpu_sync(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (virtio_gpu_commit(pResourceDesc, rect, pResponseHeader)!=0){
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
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_commit(struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_rect rect, struct virtio_gpu_response_header* pResponseHeader){
	if (!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_transfer_h2d_2d(pResourceDesc, rect, ((rect.y*pResourceDesc->width)+rect.x)*sizeof(struct uvec4_8), &responseHeader)!=0){
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
int virtio_gpu_create_context_with_resource(struct virtio_gpu_context_desc** ppContextDesc, struct virtio_gpu_resource_desc* pResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!ppContextDesc||!pResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)0x0;
	if (virtio_gpu_create_context(&pContextDesc, pResponseHeader)!=0){
		printf("failed to create virtual I/O GPU host controller context\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pResponseHeader->type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		mutex_unlock(&mutex);
		return 0;
	}
	if (virtio_gpu_context_attach_resource(pContextDesc, pResourceDesc, pResponseHeader)!=0){
		printf("failed to attach virtual I/O GPU host controller context to resource\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pResponseHeader->type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		mutex_unlock(&mutex);
		return 0;
	}
	*ppContextDesc = pContextDesc;
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
int virtio_gpu_create_resource_2d(struct virtio_gpu_create_resource_info createResourceInfo, struct virtio_gpu_resource_desc** ppResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!ppResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t resourceId = 0;
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)kmalloc(sizeof(struct virtio_gpu_resource_desc));
	if (!pResourceDesc){
		printf("failed to allocate resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pResourceDesc, 0, sizeof(struct virtio_gpu_resource_desc));
	if (subsystem_alloc_entry(gpuInfo.pResourceSubsystemDesc, (unsigned char*)pResourceDesc, &resourceId)!=0){
		printf("failed to allocate resource subsystem entry\r\n");	
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}	
	pResourceDesc->resourceId = resourceId;
	pResourceDesc->width = createResourceInfo.width;
	pResourceDesc->height = createResourceInfo.height;
	pResourceDesc->format = createResourceInfo.format;
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_create_resource_2d_command* pCommandBuffer = (struct virtio_gpu_create_resource_2d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate command buffer physical page\r\n");	
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_create_resource_2d_command));
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate response buffer physical page\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pResponseBuffer, 0, sizeof(struct virtio_gpu_response_header));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;	
	pCommandBuffer->resource_id = resourceId+1;	
	pCommandBuffer->format = createResourceInfo.format;
	pCommandBuffer->width = createResourceInfo.width;
	pCommandBuffer->height = createResourceInfo.height;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;	
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_create_resource_2d_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate create resource command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);	
		virtualFreePage((uint64_t)pResponseBuffer, 0);	
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);	
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}	
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	pResourceDesc->resourceType = VIRTIO_GPU_RESOURCE_TYPE_2D;
	*ppResourceDesc = pResourceDesc;
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_create_resource_3d(struct virtio_gpu_create_resource_info createResourceInfo, struct virtio_gpu_resource_desc** ppResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!createResourceInfo.pContextDesc||!ppResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_context_desc* pContextDesc = createResourceInfo.pContextDesc;
	uint64_t resourceId = 0;
	struct virtio_gpu_resource_desc* pResourceDesc = (struct virtio_gpu_resource_desc*)kmalloc(sizeof(struct virtio_gpu_resource_desc));
	if (!pResourceDesc){
		printf("failed to allocate resource descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pResourceDesc, 0, sizeof(struct virtio_gpu_resource_desc));
	if (subsystem_alloc_entry(gpuInfo.pResourceSubsystemDesc, (unsigned char*)pResourceDesc, &resourceId)!=0){
		printf("failed to allocate resource subsystem entry\r\n");	
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}	
	pResourceDesc->resourceId = resourceId;
	pResourceDesc->pContextDesc = createResourceInfo.pContextDesc;
	pResourceDesc->width = createResourceInfo.width;
	pResourceDesc->height = createResourceInfo.height;
	pResourceDesc->depth = createResourceInfo.depth;
	pResourceDesc->bind = createResourceInfo.bind;
	pResourceDesc->format = createResourceInfo.format;
	pResourceDesc->flags = createResourceInfo.flags;
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_create_resource_3d_command* pCommandBuffer = (struct virtio_gpu_create_resource_3d_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate command buffer physical page\r\n");	
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_create_resource_3d_command));
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate response buffer physical page\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)pResponseBuffer, 0, sizeof(struct virtio_gpu_response_header));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_RESOURCE_CREATE_3D;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId+1;
	pCommandBuffer->resource_id = resourceId+1;	
	pCommandBuffer->format = createResourceInfo.format;
	pCommandBuffer->width = createResourceInfo.width;
	pCommandBuffer->height = createResourceInfo.height;
	pCommandBuffer->target = createResourceInfo.target;
	pCommandBuffer->bind = createResourceInfo.bind;
	pCommandBuffer->depth = createResourceInfo.depth;
	pCommandBuffer->flags = createResourceInfo.flags;
	pCommandBuffer->array_size = createResourceInfo.arraySize;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;	
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_create_resource_3d_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate create resource command\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);	
		virtualFreePage((uint64_t)pResponseBuffer, 0);	
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);	
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		kfree((void*)pResourceDesc);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, resourceId);
		mutex_unlock(&mutex);
		return -1;
	}	
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	pResourceDesc->resourceType = VIRTIO_GPU_RESOURCE_TYPE_3D;
	*ppResourceDesc = pResourceDesc;
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_create_resource(struct virtio_gpu_create_resource_info createResourceInfo, struct virtio_gpu_resource_desc** ppResourceDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!ppResourceDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (createResourceInfo.resourceType==VIRTIO_GPU_RESOURCE_TYPE_2D){
		if (virtio_gpu_create_resource_2d(createResourceInfo, ppResourceDesc, pResponseHeader)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
		mutex_unlock(&mutex);
		return 0;
	}
	if (createResourceInfo.resourceType==VIRTIO_GPU_RESOURCE_TYPE_3D){
		if (virtio_gpu_create_resource_3d(createResourceInfo, ppResourceDesc, pResponseHeader)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
		mutex_unlock(&mutex);
		return 0;
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
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, pResourceDesc->resourceId);
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_delete_resource_command));
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for response buffer\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, pResourceDesc->resourceId);
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}	
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD_RESOURCE_FREE;
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_delete_resource_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);	
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU delete resource command\r\n");	
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);	
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, pResourceDesc->resourceId);
		kfree((void*)pResourceDesc);	
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, pResourceDesc->resourceId);
		kfree((void*)pResourceDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;	
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	if (subsystem_free_entry(gpuInfo.pResourceSubsystemDesc, pResourceDesc->resourceId)!=0){
		printf("failed to free resource subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	kfree((void*)pResourceDesc);
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
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
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
	pResourceDesc->pBuffer = pBuffer;
	pResourceDesc->bufferSize = bufferSize;
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
	pCommandBuffer->rect.width = pResourceDesc->width;
	pCommandBuffer->rect.height = pResourceDesc->height;
	pCommandBuffer->scanout_id = scanoutId;
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
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
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
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
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
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
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
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
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
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
int virtio_gpu_create_context(struct virtio_gpu_context_desc** ppContextDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!ppContextDesc||!pResponseHeader)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct virtio_gpu_context_desc* pContextDesc = (struct virtio_gpu_context_desc*)kmalloc(sizeof(struct virtio_gpu_context_desc));
	if (!pContextDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t contextId = 0;
	if (subsystem_alloc_entry(gpuInfo.pContextSubsystemDesc, (unsigned char*)pContextDesc, &contextId)!=0){
		printf("failed to allocate subsystem entry for virtual I/O GPU host controller context\r\n");
		kfree((void*)pContextDesc);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pContextDesc, 0, sizeof(struct virtio_gpu_context_desc));
	pContextDesc->contextId = contextId;
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	struct virtio_gpu_create_context_command* pCommandBuffer = (struct virtio_gpu_create_context_command*)0x0;
	if (virtualAllocPage((uint64_t*)&pCommandBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for create context command buffer\r\n");
		kfree((void*)pContextDesc);
		subsystem_free_entry(gpuInfo.pContextSubsystemDesc, contextId);
		mutex_unlock(&mutex);
		return -1;
	}
	struct virtio_gpu_response_header* pResponseBuffer = (struct virtio_gpu_response_header*)0x0;
	if (virtualAllocPage((uint64_t*)&pResponseBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate physical page for create context response buffer\r\n");
		kfree((void*)pContextDesc);
		subsystem_free_entry(gpuInfo.pContextSubsystemDesc, contextId);
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCommandBuffer, 0, sizeof(struct virtio_gpu_create_context_command));
	pCommandBuffer->commandHeader.type = VIRTIO_GPU_CMD3D_CONTEXT_CREATE;
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId+1;
	allocCmdInfo.pCommandBuffer = (unsigned char*)pCommandBuffer;
	allocCmdInfo.commandBufferSize = sizeof(struct virtio_gpu_create_context_command);
	allocCmdInfo.pResponseBuffer = (unsigned char*)pResponseBuffer;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_response_header);
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virutal I/O GPU create context command\r\n");
		kfree((void*)pContextDesc);
		subsystem_free_entry(gpuInfo.pContextSubsystemDesc, contextId);
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU control queue\r\n");
		kfree((void*)pContextDesc);
		subsystem_free_entry(gpuInfo.pContextSubsystemDesc, contextId);
		virtualFreePage((uint64_t)pCommandBuffer, 0);
		virtualFreePage((uint64_t)pResponseBuffer, 0);
		mutex_unlock(&mutex);
		return -1;
	}
	virtio_gpu_queue_yield_until_response(&commandDesc);
	struct virtio_gpu_response_header responseHeader = *pResponseBuffer;
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);
	*ppContextDesc = pContextDesc;
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
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId+1;
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
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId+1;
	pCommandBuffer->resource_id = pResourceDesc->resourceId+1;
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
	pContextDesc->resourceId = pResourceDesc->resourceId;
	pContextDesc->pResourceDesc = pResourceDesc;
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_context_detach_resource(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResponseHeader)
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
	pCommandBuffer->commandHeader.context_id = pContextDesc->contextId+1;
	pCommandBuffer->resource_id = pContextDesc->resourceId+1;
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
	virtualFreePage((uint64_t)pCommandBuffer, 0);
	virtualFreePage((uint64_t)pResponseBuffer, 0);	
	*pResponseHeader = responseHeader;
	mutex_unlock(&mutex);
	return 0;
}
int virtio_gpu_gallium_bind_object(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc)
		return -1;
	mutex_lock(&pContextDesc->mutex);
	
	mutex_unlock(&pContextDesc->mutex);
	return 0;
}
int virtio_gpu_gallium_delete_object(struct virtio_gpu_context_desc* pContextDesc, struct virtio_gpu_response_header* pResponseHeader){
	if (!pContextDesc||!pResponseHeader)
		return -1;
	mutex_lock(&pContextDesc->mutex);
	mutex_unlock(&pContextDesc->mutex);
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
	__asm__ volatile("mfence" ::: "memory");
	*(volatile uint16_t*)(((uint64_t)gpuInfo.pNotifyRegisters)+notifyOffset) = notifyId;
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
	if (!pQueueInfo||!physicalAddress||!size||!pMemoryDescInfo)
		return -1;
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
		if (virtio_gpu_alloc_memory_desc(pQueueInfo, pSegment_phys, segmentSize, VIRTIO_GPU_MEMORY_DESC_FLAG_NEXT, &memoryDescInfo)!=0){
			printf("failed to allocate virtual I/O memory descriptor for command header\r\n");
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
	if (!pQueueInfo->pollingEnabled)
		mutex_lock_isr_safe(&mutex);
	while (!pCommandDesc->responseDesc.responseComplete){
		if (!pQueueInfo->pollingEnabled)
			continue;
		virtio_gpu_response_queue_interrupt(pQueueInfo->isrVector);
		sleep(5);
	}
	if (!pQueueInfo->pollingEnabled)
		mutex_unlock_isr_safe(&mutex);
	return 0;
}
int virtio_gpu_subsystem_read_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8* pColor){
	if (!pColor)
		return -1;
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0)
		return -1;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc)
		return -1;
	uint64_t pixelOffset = (position.y*pMonitorDesc->scanoutInfo.resolution.width)+position.x;
	volatile struct uvec4_8* pFramebuffer = (struct uvec4_8*)pMonitorDesc->pFramebufferResourceDesc->pBuffer;
	volatile struct uvec4_8* pPixel = pFramebuffer+pixelOffset;
	struct uvec4_8 pixel = *pPixel;
	struct uvec4_8 newPixel = {pixel.z, pixel.y, pixel.x, pixel.w};
	*pColor = newPixel;
	return 0;
}
int virtio_gpu_subsystem_write_pixel(uint64_t monitorId, struct uvec2 position, struct uvec4_8 color){
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0)
		return -1;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc)
		return -1;
	uint64_t pixelOffset = (position.y*pMonitorDesc->scanoutInfo.resolution.width)+position.x;
	volatile struct uvec4_8* pFramebuffer = (struct uvec4_8*)pMonitorDesc->pFramebufferResourceDesc->pBuffer;
	if (!pFramebuffer){
		serial_print(0, "invalid framebuffer\r\n");
		return -1;
	}
	volatile struct uvec4_8* pPixel = pFramebuffer+pixelOffset;
	struct uvec4_8 newColor = {color.z, color.y, color.x, color.w};
	*pPixel = newColor;
	return 0;
}
int virtio_gpu_subsystem_sync(uint64_t monitorId, struct uvec4 rect){
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0)
		return -1;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc)
		return -1;
	struct virtio_gpu_rect virtioGpuRect = {0};
	virtioGpuRect.x = rect.x;
	virtioGpuRect.y = rect.y;
	virtioGpuRect.width = rect.z;
	virtioGpuRect.height = rect.w;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_sync(pMonitorDesc->pFramebufferResourceDesc, virtioGpuRect, &responseHeader)!=0){
		printf("failed to sync virtual I/O GPU framebuffer\r\n");
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type name";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to sync virtual I/O GPU framebuffer (%s)\r\n", responseTypeName);
		return -1;
	}
	return 0;
}
int virtio_gpu_subsystem_commit(uint64_t monitorId, struct uvec4 rect){
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0)
		return -1;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc)
		return -1;
	struct virtio_gpu_rect virtioGpuRect = {0};
	virtioGpuRect.x = rect.x;
	virtioGpuRect.y = rect.y;
	virtioGpuRect.width = rect.z;
	virtioGpuRect.height = rect.w;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_commit(pMonitorDesc->pFramebufferResourceDesc, virtioGpuRect, &responseHeader)!=0){
		printf("failed to commit to virtual I/O GPU frameubuffer\r\n");
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to commit to virtual I/O GPU framebuffer (%s)\r\n", responseTypeName);
		return -1;
	}
	return 0;
}
int virtio_gpu_subsystem_push(uint64_t monitorId, struct uvec4 rect){
	struct gpu_monitor_desc* pSubsystemMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(monitorId, &pSubsystemMonitorDesc)!=0)
		return -1;
	struct virtio_gpu_monitor_desc* pMonitorDesc = (struct virtio_gpu_monitor_desc*)pSubsystemMonitorDesc->extra;
	if (!pMonitorDesc)
		return -1;
	struct virtio_gpu_rect virtioGpuRect = {0};
	virtioGpuRect.x = rect.x;
	virtioGpuRect.y = rect.y;
	virtioGpuRect.width = rect.z;
	virtioGpuRect.height = rect.w;
	struct virtio_gpu_response_header responseHeader = {0};
	if (virtio_gpu_push(pMonitorDesc->pFramebufferResourceDesc, virtioGpuRect, &responseHeader)!=0){
		printf("failed to push to virtual I/O GPU framebuffer\r\n");
		return -1;
	}
	if (responseHeader.type!=VIRTIO_GPU_RESPONSE_OK_NODATA){
		const unsigned char* responseTypeName = "Unknown response type";	
		virtio_gpu_get_response_type_name(responseHeader.type, &responseTypeName);
		printf("failed to push to virtual I/O GPU framebuffer (%s)\r\n", responseTypeName);
		return -1;
	}
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
