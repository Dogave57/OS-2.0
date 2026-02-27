#include "stdlib/stdlib.h"
#include "subsystem/subsystem.h"
#include "mem/vmm.h"
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
	printf("VIRTIO GPU driver bus: %d, dev: %d, func: %d\r\n", gpuInfo.location.bus, gpuInfo.location.dev, gpuInfo.location.func);
	if (virtio_gpu_reset()!=0){
		printf("failed to reset virtual I/O GPU\r\n");
		return -1;
	}	
	if (virtio_gpu_acknowledge()!=0){
		printf("failed to acknowledge virtual I/O GPU\r\n");
		return -1;
	}
	printf("configuring virtual I/O GPU HC\r\n");
	gpuInfo.pBaseRegisters->device_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY;
	struct virtio_gpu_features_legacy legacyFeatures = *(volatile struct virtio_gpu_features_legacy*)&gpuInfo.pBaseRegisters->device_feature;
	legacyFeatures.virgl_support = 0;
	gpuInfo.pBaseRegisters->driver_feature = *(uint32_t*)&legacyFeatures;
	gpuInfo.pBaseRegisters->driver_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY;
	*(volatile struct virtio_gpu_features_legacy*)&gpuInfo.pBaseRegisters->driver_feature = legacyFeatures;
	struct virtio_gpu_device_status deviceStatus = gpuInfo.pBaseRegisters->device_status;
	deviceStatus.features_valid = 1;
	gpuInfo.pBaseRegisters->device_status = deviceStatus;
	deviceStatus = gpuInfo.pBaseRegisters->device_status;
	if (!deviceStatus.features_valid){
		printf("virtual I/O GPU controller does not support legacy features set\r\n");
		return -1;
	}
	gpuInfo.pBaseRegisters->device_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_MODERN;
	struct virtio_gpu_features_modern modernFeatures = *(volatile struct virtio_gpu_features_modern*)&gpuInfo.pBaseRegisters->device_feature;
	gpuInfo.pBaseRegisters->driver_feature = *(uint32_t*)&modernFeatures;
	gpuInfo.pBaseRegisters->driver_feature_select = VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_MODERN;
	*(volatile struct virtio_gpu_features_modern*)&gpuInfo.pBaseRegisters->driver_feature = modernFeatures;	
	deviceStatus = gpuInfo.pBaseRegisters->device_status;
	gpuInfo.pBaseRegisters->device_status = deviceStatus;
	deviceStatus = gpuInfo.pBaseRegisters->device_status;
	if (!deviceStatus.features_valid){
		printf("virtual I/O GPU controller does not support modern features set\r\n");
		return -1;
	}
	printf("finished configuring virtual I/O GPU HC\r\n");
	if (virtio_gpu_queue_init(&gpuInfo.virtQueueInfo)!=0){
		printf("failed to initialize virtual I/O GPU virtqueue\r\n");
		return -1;
	}	
	gpuInfo.pBaseRegisters->queue_select = 0x0;
	gpuInfo.pBaseRegisters->queue_enable = 0x0;
	gpuInfo.pBaseRegisters->queue_size = PAGE_SIZE;
	gpuInfo.pBaseRegisters->queue_memory_desc_list_base = gpuInfo.virtQueueInfo.pMemoryDescList_phys;	
	gpuInfo.pBaseRegisters->queue_command_list_base = gpuInfo.virtQueueInfo.pCommandList_phys;
	gpuInfo.pBaseRegisters->queue_response_list_base = gpuInfo.virtQueueInfo.pResponseList_phys;	
	gpuInfo.pBaseRegisters->queue_enable = 0x01;
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
int virtio_gpu_reset(void){
	*(uint32_t*)&gpuInfo.pBaseRegisters->device_status = 0;
	return 0;
}
int virtio_gpu_acknowledge(void){
	struct virtio_gpu_device_status deviceStatus = gpuInfo.pBaseRegisters->device_status;
	deviceStatus.acknowledge = 1;
	deviceStatus.driver = 1;	
	gpuInfo.pBaseRegisters->device_status = deviceStatus;	
	return 0;
}
int virtio_gpu_queue_init(struct virtio_gpu_queue_info* pQueueInfo){
	if (!pQueueInfo)
		return -1;
	struct virtio_gpu_memory_desc* pMemoryDescList = (struct virtio_gpu_memory_desc*)0x0;
	uint64_t pMemoryDescList_phys = 0;
	if (virtualAllocPage((uint64_t*)&pMemoryDescList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for memory descriptor list\r\n");
		return -1;
	}
	if (virtualToPhysical((uint64_t)pMemoryDescList, &pMemoryDescList_phys)!=0){
		printf("failed to get physical address of memory descriptor list\r\n");	
		virtualFreePage((uint64_t)pMemoryDescList, 0);
		return -1;
	}	
	struct virtio_gpu_command_header* pCommandList = (struct virtio_gpu_command_header*)0x0;
	uint64_t pCommandList_phys = 0;
	if (virtualAllocPage((uint64_t*)&pCommandList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for command list\r\n");
		virtualFreePage((uint64_t)pMemoryDescList, 0);
		return -1;
	}	
	if (virtualToPhysical((uint64_t)pCommandList, &pCommandList_phys)!=0){
		printf("failed to get physical address of command list\r\n");
		virtualFreePage((uint64_t)pMemoryDescList, 0);
		virtualFreePage((uint64_t)pCommandList, 0);
		return -1;
	}
	struct virtio_gpu_response_header* pResponseList = (struct virtio_gpu_response_header*)0x0;
	uint64_t pResponseList_phys = 0;
	if (virtualAllocPage((uint64_t*)&pResponseList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for response list\r\n");
		virtualFreePage((uint64_t)pMemoryDescList, 0);
		virtualFreePage((uint64_t)pCommandList, 0);
		return -1;
	}	
	if (virtualToPhysical((uint64_t)pResponseList, &pResponseList_phys)!=0){
		printf("failed to get physical address of response list\r\n");	
		virtualFreePage((uint64_t)pMemoryDescList, 0);
		virtualFreePage((uint64_t)pCommandList, 0);	
		virtualFreePage((uint64_t)pResponseList, 0);	
		return -1;
	}	
	uint64_t maxMemoryDescCount = VIRTIO_GPU_MAX_MEMORY_DESC_COUNT;
	uint64_t maxCommandCount = VIRTIO_GPU_MAX_COMMAND_COUNT;
	uint64_t maxResponseCount = VIRTIO_GPU_MAX_RESPONSE_COUNT;	
	uint64_t memoryDescListSize = sizeof(struct virtio_gpu_memory_desc)*maxMemoryDescCount;
	uint64_t commandListSize = sizeof(struct virtio_gpu_command_header)*maxCommandCount;
	uint64_t responseListSize = sizeof(struct virtio_gpu_response_header)*maxResponseCount;	
	struct virtio_gpu_queue_info queueInfo = {0};
	memset((void*)&queueInfo, 0, sizeof(struct virtio_gpu_queue_info));	
	queueInfo.pMemoryDescList = pMemoryDescList;
	queueInfo.pCommandList = pCommandList;
	queueInfo.pResponseList = pResponseList;	
	queueInfo.pMemoryDescList_phys = pMemoryDescList_phys;	
	queueInfo.pCommandList_phys = pCommandList_phys;
	queueInfo.pResponseList_phys = pResponseList_phys;	
	queueInfo.maxMemoryDescCount = maxMemoryDescCount;
	queueInfo.maxCommandCount = maxCommandCount;
	queueInfo.maxResponseCount = maxResponseCount;
	*pQueueInfo = queueInfo;	
	return 0;
}
int virtio_gpu_queue_deinit(struct virtio_gpu_queue_info* pQueueInfo){
	if (!pQueueInfo)
		return -1;
	uint64_t memoryDescListSize = pQueueInfo->maxMemoryDescCount*sizeof(struct virtio_gpu_memory_desc);
	uint64_t commandListSize = pQueueInfo->maxCommandCount*sizeof(struct virtio_gpu_command_header);
	uint64_t responseListSize = pQueueInfo->maxResponseCount*sizeof(struct virtio_gpu_response_header);
	if (virtualFree((uint64_t)pQueueInfo->pMemoryDescList, memoryDescListSize)!=0){
		virtualFree((uint64_t)pQueueInfo->pResponseList, responseListSize);
		virtualFree((uint64_t)pQueueInfo->pCommandList, commandListSize);	
		return -1;
	}	
	if (virtualFree((uint64_t)pQueueInfo->pCommandList, commandListSize)!=0){
		virtualFree((uint64_t)pQueueInfo->pResponseList, responseListSize);
		return -1;
	}
	if (virtualFree((uint64_t)pQueueInfo->pResponseList, responseListSize)!=0){
		return -1;
	}
	return 0;
}
int virtio_gpu_queue_run_cmd(struct virtio_gpu_command_desc* pCommandDesc){
	if (!pCommandDesc)
		return -1;

	return 0;
}
int virtio_gpu_alloc_memory_desc(struct virtio_gpu_queue_info* pQueueInfo, uint64_t physicalAddress, uint32_t size, uint16_t flags, uint64_t* pMemoryDescIndex){
	if (!pQueueInfo||!physicalAddress||!size||!pMemoryDescIndex)
		return -1;
	uint64_t memoryDescIndex = pQueueInfo->memoryDescCount;
	if (memoryDescIndex>pQueueInfo->maxMemoryDescCount-1){
		pQueueInfo->memoryDescCount = 0;	
		memoryDescIndex = 0;
	}
	struct virtio_gpu_memory_desc* pMemoryDesc = &pQueueInfo->pMemoryDescList[memoryDescIndex];	
	memset((void*)pMemoryDesc, 0, sizeof(struct virtio_gpu_memory_desc));
	pMemoryDesc->physical_address = physicalAddress;
	pMemoryDesc->length = size;	
	pMemoryDesc->flags = flags;
	*pMemoryDescIndex = memoryDescIndex;	
	return 0;
}
int virtio_gpu_queue_alloc_cmd(struct virtio_gpu_queue_info* pQueueInfo, struct virtio_gpu_command_header commandHeader, struct virtio_gpu_command_desc* pCommandDesc){
	if (!pQueueInfo||!pCommandDesc)
		return -1;

	return 0;
}
