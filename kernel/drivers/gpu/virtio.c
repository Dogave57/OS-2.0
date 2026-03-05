#include "stdlib/stdlib.h"
#include "subsystem/subsystem.h"
#include "mem/vmm.h"
#include "cpu/idt.h"
#include "cpu/interrupt.h"
#include "align.h"
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
	printf("VIRTIO GPU driver bus: %d, dev: %d, func: %d\r\n", gpuInfo.location.bus, gpuInfo.location.dev, gpuInfo.location.func);
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
	legacyFeatures.virgl_support = 0;
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
	struct virtio_gpu_display_info* pDisplayInfo = (struct virtio_gpu_display_info*)0x0;
	if (virtualAllocPage((uint64_t*)&pDisplayInfo, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate physical page for display info\r\n");
		virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
		return -1;
	}	
	struct virtio_gpu_command_desc commandDesc = {0};
	memset((void*)&commandDesc, 0, sizeof(struct virtio_gpu_command_desc));
	struct virtio_gpu_alloc_cmd_info allocCmdInfo = {0};
	memset((void*)&allocCmdInfo, 0, sizeof(struct virtio_gpu_alloc_cmd_info));
	allocCmdInfo.pResponseBuffer = (unsigned char*)pDisplayInfo;
	allocCmdInfo.responseBufferSize = sizeof(struct virtio_gpu_display_info);
	memset((void*)pDisplayInfo, 0, sizeof(struct virtio_gpu_display_info));	
	struct virtio_gpu_command_header* pCommandHeader = &allocCmdInfo.commandHeader;	
	pCommandHeader->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;	
	if (virtio_gpu_queue_alloc_cmd(&gpuInfo.controlQueueInfo, allocCmdInfo, &commandDesc)!=0){
		printf("failed to allocate virtual I/O GPU command\r\n");
		virtualFreePage((uint64_t)pDisplayInfo, 0);
		virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
		return -1;
	}	
	if (virtio_gpu_run_queue(&gpuInfo.controlQueueInfo)!=0){
		printf("failed to run virtual I/O GPU command ring\r\n");
		virtualFreePage((uint64_t)pDisplayInfo, 0);
		virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);	
		return -1;
	}
	printf("running display info command\r\n");
	sleep(250);
	printf("response ring index: %d\r\n", gpuInfo.controlQueueInfo.pResponseRing->index);	
	printf("response ring flags: %d\r\n", gpuInfo.controlQueueInfo.pResponseRing->flags);
	while (!commandDesc.responseDesc.responseComplete){};	
	printf("finished display info command\r\n");
	for (uint64_t i = 0;i<VIRTIO_GPU_MAX_SCANOUT_COUNT;i++){
		struct virtio_gpu_scanout_info* pScanoutInfo = pDisplayInfo->scanoutList+i;
		if (!pScanoutInfo->enabled)
			continue;	
		printf("scanout %d has resolution %d:%d\r\n", i, pScanoutInfo->resolution.width, pScanoutInfo->resolution.height);
	}	
	if (virtio_gpu_queue_free_cmd(&commandDesc)!=0){
		printf("failed to free virtual I/O GPU command\r\n");
		virtualFreePage((uint64_t)pDisplayInfo, 0);
		virtio_gpu_queue_deinit(&gpuInfo.controlQueueInfo);
		return -1;
	}	
	virtualFreePage((uint64_t)pDisplayInfo, 0);
//	while (1){};
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
	printf("virtaul I/O GPU queue ISR\r\n");
	if (!pQueueInfo){
		return -1;
	}	
	uint64_t responseCount = pQueueInfo->pResponseRing->index-pQueueInfo->oldResponseRingIndex;
	for (uint64_t i = 0;i<responseCount;i++){	
		uint64_t currentResponse = pQueueInfo->currentResponse;	
		volatile struct virtio_gpu_response_list_entry* pResponseListEntry = &pQueueInfo->pResponseRing->responseList[currentResponse];
		printf("response list entry index: %d\r\n", pResponseListEntry->index);	
		struct virtio_gpu_response_desc* pResponseDesc = pQueueInfo->ppResponseDescList[pQueueInfo->currentResponse];	
		pResponseDesc->responseComplete = 1;	
		gpuInfo.controlQueueInfo.currentResponse++;
		if (gpuInfo.controlQueueInfo.currentResponse>gpuInfo.controlQueueInfo.maxResponseCount)
			gpuInfo.controlQueueInfo.currentResponse = 0;	
		printf("ring index: %d\r\n", pQueueInfo->pResponseRing->index);
		printf("MSI-X vector: %d\r\n", pQueueInfo->msixVector);	
	}
	pQueueInfo->oldResponseRingIndex = pQueueInfo->pResponseRing->index;
	if (pcie_msix_enable(gpuInfo.pMsgControl)!=0){
		printf("failed to enable MSI-X\r\n");	
		return -1;
	}
	if (pcie_msix_enable_entry(gpuInfo.location, gpuInfo.pMsgControl, pQueueInfo->msixVector)!=0){
		printf("failed to enable MSI-X entry\r\n");
		return -1;
	}	
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
	if (commandIndex>pQueueInfo->maxCommandCount){
		pQueueInfo->currentCommand = 0;
		commandIndex = 0;
	}
	volatile struct virtio_gpu_command_header* pCommandHeader = (struct virtio_gpu_command_header*)0x0;	
	uint64_t pCommandHeader_phys = 0;
	if (virtualAllocPage((uint64_t*)&pCommandHeader, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate command header physical page\r\n");
		return -1;
	}
	if (virtualToPhysical((uint64_t)pCommandHeader, &pCommandHeader_phys)!=0){
		printf("failed to get physical address of command header\r\n");
		virtualFreePage((uint64_t)pCommandHeader, 0);	
		return -1;
	}
	uint64_t pResponseBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)allocCmdInfo.pResponseBuffer, &pResponseBuffer_phys)!=0){
		printf("failed to get response buffer physical address\r\n");
		virtualFreePage((uint64_t)pCommandHeader, 0);
		return -1;
	}	
	*pCommandHeader = allocCmdInfo.commandHeader;	
	uint64_t bufferPageCount = align_up(allocCmdInfo.bufferSize, PAGE_SIZE)/PAGE_SIZE;
	struct virtio_gpu_memory_desc_info commandMemoryDescInfo = {0};
	struct virtio_gpu_memory_desc_info responseMemoryDescInfo = {0};	
	if (virtio_gpu_alloc_memory_desc(pQueueInfo, pCommandHeader_phys, sizeof(struct virtio_gpu_command_header), VIRTIO_GPU_MEMORY_DESC_FLAG_NEXT, &commandMemoryDescInfo)!=0){
		printf("failed to allocate virtual I/O memory descriptor for command header\r\n");
		virtualFreePage((uint64_t)pCommandHeader, 0);	
		return -1;
	}
	struct virtio_gpu_memory_desc_info lastMemoryDescInfo = commandMemoryDescInfo;	
	for (uint64_t i = 0;i<bufferPageCount;i++){
		unsigned char* pSegment = allocCmdInfo.pBuffer+(i*PAGE_SIZE);	
		uint64_t segmentSize = (i==bufferPageCount-1&&allocCmdInfo.bufferSize%PAGE_SIZE) ? allocCmdInfo.bufferSize%PAGE_SIZE : PAGE_SIZE;
		uint64_t pSegment_phys = 0;
		if (virtualToPhysical((uint64_t)pSegment, &pSegment_phys)!=0){
			printf("failed to get physical address of buffer segment\r\n");
			virtualFreePage((uint64_t)pCommandHeader, 0);
			return -1;
		}
		struct virtio_gpu_memory_desc_info segmentMemoryDescInfo = {0};	
		if (virtio_gpu_alloc_memory_desc(pQueueInfo, pSegment_phys, segmentSize, VIRTIO_GPU_MEMORY_DESC_FLAG_NEXT, &segmentMemoryDescInfo)!=0){
			printf("failed to allocate memory desc for segment\r\n");
			virtualFreePage((uint64_t)pCommandHeader, 0);
			return -1;
		}	
		lastMemoryDescInfo.pMemoryDesc->next = segmentMemoryDescInfo.memoryDescIndex;	
		lastMemoryDescInfo = segmentMemoryDescInfo;	
	}	
	if (virtio_gpu_alloc_memory_desc(pQueueInfo, pResponseBuffer_phys, allocCmdInfo.responseBufferSize, VIRTIO_GPU_MEMORY_DESC_FLAG_WRITE, &responseMemoryDescInfo)!=0){
		printf("failed to allocate virtuall I/O memory descriptor for response header\r\n");
		virtualFreePage((uint64_t)pCommandHeader, 0);
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
	commandDesc.pCommandHeader = pCommandHeader;
	commandDesc.pResponseBuffer = allocCmdInfo.pResponseBuffer;
	commandDesc.pResponseBuffer_phys = pResponseBuffer_phys;	
	commandDesc.responseBufferSize = allocCmdInfo.responseBufferSize;	
	commandDesc.pCommandHeader_phys = pCommandHeader_phys;
	*pCommandDesc = commandDesc;	
	pQueueInfo->ppResponseDescList[pQueueInfo->currentResponse] = &pCommandDesc->responseDesc;	
	pQueueInfo->currentCommand++;	
	pQueueInfo->pCommandRing->index = (pQueueInfo->pCommandRing->index>=65534) ? 0 : pQueueInfo->pCommandRing->index+1;
	*ppCommandDesc = pCommandDesc;	
	return 0;
}
int virtio_gpu_queue_free_cmd(struct virtio_gpu_command_desc* pCommandDesc){
	if (!pCommandDesc)
		return -1;
	if (virtualFreePage((uint64_t)pCommandDesc->pCommandHeader, 0)!=0){
		return -1;
	}	
	return 0;
}
