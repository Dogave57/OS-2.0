#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "cpu/mutex.h"
#include "cpu/idt.h"
#include "drivers/apic.h"
#include "drivers/pcie.h"
#include "drivers/timer.h"
#include "subsystem/pcie.h"
#include "drivers/nvme.h"
static struct nvme_driver_info driverInfo = {0};
int nvme_driver_init(void){
	if (nvme_driver_init_drive_desc_mapping_table()!=0){
		printf("failed to initialize drive descriptor mapping table\r\n");
		return -1;
	}	
	struct pcie_driver_vtable pcieDriverVtable = {0};
	memset((void*)&pcieDriverVtable, 0, sizeof(struct pcie_driver_vtable));
	pcieDriverVtable.registerFunction = nvme_subsystem_function_register;
	pcieDriverVtable.unregisterFunction = nvme_subsystem_function_unregister;
	if (pcie_subsystem_driver_register(pcieDriverVtable, NVME_DRIVE_PCIE_CLASS, NVME_DRIVE_PCIE_SUBCLASS, &driverInfo.driverId)!=0){
		printf("failed to register PCIe NVMe protocol driver\r\n");
		return -1;
	}
	return 0;
}
int nvme_driver_init_drive_desc_mapping_table(void){
	struct nvme_drive_desc** pDriveDescList = (struct nvme_drive_desc**)0x0;
	uint64_t driveDescListSize = sizeof(struct nvme_drive_desc*)*IDT_MAX_ENTRIES;	
	if (virtualAlloc((uint64_t*)&pDriveDescList, driveDescListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate drive descriptor list\r\n");
		return -1;
	}	
	memset((void*)pDriveDescList, 0, driveDescListSize);
	driverInfo.pDriveDescMappingTable = pDriveDescList;
	driverInfo.driveDescMappingTableSize = driveDescListSize;	
	return 0;
}
int nvme_enable(struct nvme_drive_desc* pDriveDesc){
	if (!pDriveDesc)
		return -1;
	struct nvme_controller_status controllerStatus = pDriveDesc->pBaseRegisters->controller_status;
	struct nvme_controller_config controllerConfig = pDriveDesc->pBaseRegisters->controller_config;
	if (controllerStatus.ready||controllerConfig.enable){
		return 0;
	}
	controllerConfig.enable = 1;
	pDriveDesc->pBaseRegisters->controller_config = controllerConfig;
	while (!controllerStatus.ready){
		controllerStatus = pDriveDesc->pBaseRegisters->controller_status;
	}
	return 0;
}
int nvme_disable(struct nvme_drive_desc* pDriveDesc){
	if (!pDriveDesc)
		return -1;
	struct nvme_controller_status controllerStatus = pDriveDesc->pBaseRegisters->controller_status;
	if (!controllerStatus.ready)
		return 0;
	struct nvme_controller_config controllerConfig = pDriveDesc->pBaseRegisters->controller_config;
	controllerConfig.enable = 0;
	pDriveDesc->pBaseRegisters->controller_config = controllerConfig;
	while (controllerStatus.ready){
		controllerStatus = pDriveDesc->pBaseRegisters->controller_status;
	}
	return 0;
}
int nvme_enabled(struct nvme_drive_desc* pDriveDesc){
	if (!pDriveDesc)
		return -1;
	struct nvme_controller_config controllerConfig = pDriveDesc->pBaseRegisters->controller_config;
	return controllerConfig.enable ? 0 : -1;
}
int nvme_ring_doorbell(struct nvme_drive_desc* pDriveDesc, uint64_t queueId, uint16_t tail){
	if (!pDriveDesc)
		return -1;
	struct nvme_capabilities_register capabilities = pDriveDesc->pBaseRegisters->capabilities;	
	uint16_t* pDoorbell = (uint16_t*)(((uint64_t)pDriveDesc->pDoorbellBase)+(2*queueId*capabilities.doorbell_stride));
	*pDoorbell = tail;	
	return 0;
}
int nvme_run_submission_queue(struct nvme_submission_queue_desc* pSubmissionQueueDesc){
	if (!pSubmissionQueueDesc)
		return -1;
	if (!pSubmissionQueueDesc->submissionEntry)
		return -1;
	if (nvme_enable(pSubmissionQueueDesc->pDriveDesc)!=0)
		return -1;	
	if (nvme_ring_doorbell(pSubmissionQueueDesc->pDriveDesc, pSubmissionQueueDesc->queueId, pSubmissionQueueDesc->submissionEntry)!=0)
		return -1;
	return 0;
}
int nvme_alloc_completion_queue(struct nvme_drive_desc* pDriveDesc, uint64_t maxEntryCount, struct nvme_completion_queue_desc* pCompletionQueueDesc){
	if (!pDriveDesc||!pCompletionQueueDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (maxEntryCount>NVME_MAX_CQE_COUNT)
		maxEntryCount = NVME_DEFAULT_CQE_COUNT;
	uint64_t completionQueueSize = maxEntryCount*sizeof(struct nvme_completion_qe);
	struct nvme_completion_qe* pCompletionQueue = (struct nvme_completion_qe*)0x0;
	uint64_t pCompletionQueue_phys = 0;
	if (virtualAlloc((uint64_t*)&pCompletionQueue, completionQueueSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		mutex_unlock(&mutex);
		return -1;
	}	
	if (virtualToPhysical((uint64_t)pCompletionQueue, &pCompletionQueue_phys)!=0){
		virtualFree((uint64_t)pCompletionQueue, completionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pCompletionQueue, 0, completionQueueSize);
	struct nvme_completion_queue_desc completionQueueDesc = {0};
	memset((void*)&completionQueueDesc, 0, sizeof(struct nvme_completion_queue_desc));
	completionQueueDesc.pCompletionQueue = pCompletionQueue;
	completionQueueDesc.pCompletionQueue_phys = pCompletionQueue_phys;
	completionQueueDesc.maxCompletionEntryCount = maxEntryCount;
	*pCompletionQueueDesc = completionQueueDesc;
	mutex_unlock(&mutex);	
	return 0;
}
int nvme_free_completion_queue(struct nvme_completion_queue_desc* pCompletionQueueDesc){
	if (!pCompletionQueueDesc)
		return -1;
	if (virtualFree((uint64_t)pCompletionQueueDesc->pCompletionQueue, pCompletionQueueDesc->maxCompletionEntryCount*sizeof(struct nvme_completion_qe))!=0)
		return -1;
	struct nvme_completion_queue_desc completionQueueDesc = {0};
	memset((void*)&completionQueueDesc, 0, sizeof(struct nvme_completion_queue_desc));
	*pCompletionQueueDesc = completionQueueDesc;
	return 0;
}
int nvme_get_current_completion_qe(struct nvme_completion_queue_desc* pCompletionQueueDesc, volatile struct nvme_completion_qe** ppCompletionQe){
	if (!pCompletionQueueDesc||!ppCompletionQe)
		return -1;
	volatile struct nvme_completion_qe* pCompletionQe = pCompletionQueueDesc->pCompletionQueue+pCompletionQueueDesc->completionEntry;
	*ppCompletionQe = pCompletionQe;	
	return 0;
}
int nvme_acknowledge_completion_qe(struct nvme_completion_queue_desc* pCompletionQueueDesc){
	if (!pCompletionQueueDesc)
		return -1;
	pCompletionQueueDesc->completionEntry++;
	return 0;
}
int nvme_alloc_submission_qe(struct nvme_submission_queue_desc* pSubmissionQueueDesc, struct nvme_submission_qe entry, struct nvme_submission_qe_desc* pSubmissionQeDesc){
	if (!pSubmissionQueueDesc||!pSubmissionQeDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (pSubmissionQueueDesc->submissionEntry>=pSubmissionQueueDesc->maxSubmissionEntryCount){
		pSubmissionQueueDesc->submissionEntry = 0;
	}	
	uint64_t submissionQeIndex = pSubmissionQueueDesc->submissionEntry;
	volatile struct nvme_submission_qe* pEntry = pSubmissionQueueDesc->pSubmissionQueue+submissionQeIndex;
	*pEntry = entry;
	pSubmissionQueueDesc->submissionEntry++;
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	memset((void*)&submissionQeDesc, 0, sizeof(struct nvme_submission_qe_desc));
	submissionQeDesc.qeIndex = submissionQeIndex;
	submissionQeDesc.pSubmissionQueueDesc = pSubmissionQueueDesc;
	*pSubmissionQeDesc = submissionQeDesc;
	mutex_unlock(&mutex);
	return 0;
}
int nvme_alloc_submission_queue(struct nvme_drive_desc* pDriveDesc, uint64_t maxEntryCount, uint64_t queueId, struct nvme_submission_queue_desc* pSubmissionQueueDesc, struct nvme_completion_queue_desc* pCompletionQueueDesc){
	if (!pDriveDesc||!pSubmissionQueueDesc||!pCompletionQueueDesc)
		return -1;
	if (maxEntryCount>NVME_MAX_SQE_COUNT)
		return -1;
	if (!maxEntryCount)
		maxEntryCount = NVME_DEFAULT_SQE_COUNT;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t submissionQueueSize = maxEntryCount*sizeof(struct nvme_submission_qe);
	volatile struct nvme_submission_qe* pSubmissionQueue = (volatile struct nvme_submission_qe*)0x0;
	uint64_t pSubmissionQueue_phys = 0;
	if (virtualAlloc((uint64_t*)&pSubmissionQueue, submissionQueueSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate submission queue\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualToPhysical((uint64_t)pSubmissionQueue, &pSubmissionQueue_phys)!=0){
		printf("failed to get physical address of submission queue\r\n");
		virtualFree((uint64_t)pSubmissionQueue, submissionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pSubmissionQueue, 0, submissionQueueSize);	
	struct nvme_submission_queue_desc submissionQueueDesc = {0};
	memset((void*)&submissionQueueDesc, 0, sizeof(struct nvme_submission_queue_desc));	
	submissionQueueDesc.pSubmissionQueue = pSubmissionQueue;
	submissionQueueDesc.pSubmissionQueue_phys = pSubmissionQueue_phys;	
	submissionQueueDesc.maxSubmissionEntryCount = maxEntryCount;
	submissionQueueDesc.queueId = queueId;
	submissionQueueDesc.pCompletionQueueDesc = pCompletionQueueDesc;	
	submissionQueueDesc.pDriveDesc = pDriveDesc;
	*pSubmissionQueueDesc = submissionQueueDesc;
	mutex_unlock(&mutex);
	return 0;
}
int nvme_free_submission_queue(struct nvme_submission_queue_desc* pSubmissionQueueDesc){
	if (!pSubmissionQueueDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (virtualFree((uint64_t)pSubmissionQueueDesc->pSubmissionQueue, pSubmissionQueueDesc->maxSubmissionEntryCount*sizeof(struct nvme_submission_qe))!=0){
		printf("failed to free submission queue\r\n");
		mutex_unlock(&mutex);
		return -1;
	}	
	struct nvme_submission_queue_desc submissionQueueDesc = {0};
	memset((void*)&submissionQueueDesc, 0, sizeof(struct nvme_submission_queue_desc));
	*pSubmissionQueueDesc = submissionQueueDesc;	
	mutex_unlock(&mutex);
	return 0;
}
int nvme_init_admin_submission_queue(struct nvme_drive_desc* pDriveDesc){
	if (!pDriveDesc)
		return -1;
	uint64_t lapic_id = 0;
	if (lapic_get_id(&lapic_id)!=0)
		return -1;
	struct nvme_completion_queue_desc completionQueueDesc = {0};
	memset((void*)&completionQueueDesc, 0, sizeof(struct nvme_completion_queue_desc));
	struct nvme_submission_queue_desc submissionQueueDesc = {0};
	memset((void*)&submissionQueueDesc, 0, sizeof(struct nvme_submission_queue_desc));
	if (nvme_alloc_completion_queue(pDriveDesc, NVME_MAX_CQE_COUNT, &completionQueueDesc)!=0)
		return -1;
	if (nvme_alloc_submission_queue(pDriveDesc, NVME_MAX_SQE_COUNT, 0x0, &submissionQueueDesc, &completionQueueDesc)!=0){
		nvme_free_completion_queue(&completionQueueDesc);	
		return -1;
	}
	pDriveDesc->adminSubmissionQueueDesc = submissionQueueDesc;
	pDriveDesc->adminCompletionQueueDesc = completionQueueDesc;
	uint8_t freeVector = 0;
	if (idt_get_free_vector(&freeVector)!=0){
		nvme_free_completion_queue(&completionQueueDesc);
		nvme_free_submission_queue(&submissionQueueDesc);
		return -1;
	}
	if (idt_add_entry(freeVector, (uint64_t)nvme_admin_completion_queue_isr, 0x8E, 0x0, 0x0)!=0){
		nvme_free_completion_queue(&completionQueueDesc);
		nvme_free_submission_queue(&submissionQueueDesc);
		return -1;
	}	
	volatile struct pcie_msix_msg_ctrl* pMsgControl = (volatile struct pcie_msix_msg_ctrl*)0x0;
	if (pcie_msix_get_msg_ctrl(pDriveDesc->location, &pMsgControl)!=0){
		printf("failed to get drive MSI-X msg control\r\n");
		nvme_free_submission_queue(&submissionQueueDesc);
		nvme_free_completion_queue(&completionQueueDesc);
		return -1;
	}	
	if (pcie_msix_enable(pMsgControl)!=0){
		printf("failed to enable MSI-X for drive\r\n");
		nvme_free_submission_queue(&submissionQueueDesc);
		nvme_free_completion_queue(&completionQueueDesc);
		return -1;
	}
	if (pcie_set_msix_entry(pDriveDesc->location, pMsgControl, 0x0, lapic_id, freeVector)!=0){
		printf("failed to set MSI-X entry for admin completion queue ISR\r\n");
		nvme_free_submission_queue(&submissionQueueDesc);
		nvme_free_completion_queue(&completionQueueDesc);
		return -1;
	}	
	if (pcie_msix_enable_entry(pDriveDesc->location, pMsgControl, 0x0)!=0){
		printf("failed to enable MSI-X entry for admin completion queue ISR\r\n");
		nvme_free_submission_queue(&submissionQueueDesc);	
		nvme_free_completion_queue(&completionQueueDesc);
		return -1;
	}
	struct nvme_admin_queue_attribs adminQueueAttribs = {0};
	memset((void*)&adminQueueAttribs, 0, sizeof(struct nvme_admin_queue_attribs));
	adminQueueAttribs.admin_submission_queue_size = NVME_MAX_SQE_COUNT-1;
	adminQueueAttribs.admin_completion_queue_size = NVME_MAX_CQE_COUNT-1;
	pDriveDesc->pBaseRegisters->admin_queue_attribs = adminQueueAttribs;	
	pDriveDesc->pBaseRegisters->admin_submission_queue_base = submissionQueueDesc.pSubmissionQueue_phys;
	pDriveDesc->pBaseRegisters->admin_completion_queue_base = completionQueueDesc.pCompletionQueue_phys;	
	driverInfo.pDriveDescMappingTable[freeVector] = pDriveDesc;	
	return 0;
}
int nvme_deinit_admin_submission_queue(struct nvme_drive_desc* pDriveDesc){
	if (!pDriveDesc)
		return -1;
	if (nvme_free_completion_queue(&pDriveDesc->adminCompletionQueueDesc)!=0){
		nvme_free_submission_queue(&pDriveDesc->adminSubmissionQueueDesc);
		return -1;
	}
	if (nvme_free_submission_queue(&pDriveDesc->adminSubmissionQueueDesc)!=0)
		return -1;
	return 0;
}
int nvme_admin_completion_queue_interrupt(uint8_t vector){
	struct nvme_drive_desc* pDriveDesc = driverInfo.pDriveDescMappingTable[vector];	
	if (!pDriveDesc){
		printf("invalid drive descriptor linked with admin completion queue ISR\r\n");
		return -1;
	}
	volatile struct nvme_completion_qe* pCompletionQe = (volatile struct nvme_completion_qe*)0x0;
	if (nvme_get_current_completion_qe(&pDriveDesc->adminCompletionQueueDesc, &pCompletionQe)!=0){
		printf("failed to get current completion queue entry\r\n");
		return -1;
	}	
	printf("admin completion queue ISR at vector: %d\r\n", vector);	
	printf("admin completion queue entry status: 0x%x\r\n", pCompletionQe->status);	
	if (nvme_acknowledge_completion_qe(&pDriveDesc->adminCompletionQueueDesc)!=0){
		printf("failed to acknowledge current completion queue entry\r\n");
		return -1;
	}	
	return 0;
}
int nvme_io_completion_queue_interrupt(uint8_t vector){
	printf("I/O completion queue ISR at vector %d\r\n", vector);
	return 0;
}
int nvme_drive_init(struct pcie_location location){
	if (pcie_function_exists(location)!=0){
		printf("attempted to initialize non-existent drive that follows NVMe protocol\r\n");
		return -1;	
	}	
	uint8_t progIf = 0;
	if (pcie_get_progif(location, &progIf)!=0){
		printf("failed to get drive programming interface\r\n");
		return -1;
	}	
	if (progIf!=NVME_DRIVE_PCIE_PROGIF){
		printf("PCIe non-volatile memory device does not follow NVMe protocol\r\n");
		return -1;
	}
	printf("initializing NVMe drive at bus %d, dev %d, func %d\r\n", location.bus, location.dev, location.func);
	uint64_t pBaseRegisters_phys = 0;
	if (pcie_get_bar(location, 0x0, (uint64_t*)&pBaseRegisters_phys)!=0){
		printf("failed to get BAR0\r\n");
		return -1;
	}
	volatile struct nvme_base_mmio* pBaseRegisters = (volatile struct nvme_base_mmio*)0x0;
	if (virtualGetSpace((uint64_t*)&pBaseRegisters, MEM_MB/PAGE_SIZE)!=0){
		printf("failed to get virtual addressing space for NVMe drive base registers\r\n");
		return -1;
	}	
	if (virtualMapPages((uint64_t)pBaseRegisters_phys, (uint64_t)pBaseRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, MEM_MB/PAGE_SIZE, 0, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map NVMe drive registers\r\n");
		return -1;
	}
	struct nvme_drive_desc* pDriveDesc = (struct nvme_drive_desc*)kmalloc(sizeof(struct nvme_drive_desc));
	if (!pDriveDesc){
		return -1;
	}	
	memset((void*)pDriveDesc, 0, sizeof(struct nvme_drive_desc));
	pDriveDesc->pBaseRegisters_phys = pBaseRegisters_phys;
	pDriveDesc->pBaseRegisters = pBaseRegisters;	
	pDriveDesc->pDoorbellBase = (uint16_t*)((uint64_t)pBaseRegisters+NVME_DOORBELL_LIST_OFFSET);	
	pDriveDesc->location = location;
	if (nvme_disable(pDriveDesc)!=0){
		printf("failed to disable NVMe drive controller\r\n");
		kfree((void*)pDriveDesc);
		return -1;
	}	
	if (nvme_init_admin_submission_queue(pDriveDesc)!=0){
		printf("failed to initialize admin submission and completion queue\r\n");
		kfree((void*)pDriveDesc);
		return -1;
	}
	return 0;
}
int nvme_drive_deinit(struct pcie_location location){

	return 0;
}
int nvme_subsystem_function_register(struct pcie_location location){
	if (nvme_drive_init(location)!=0)
		return -1;
	return 0;
}
int nvme_subsystem_function_unregister(struct pcie_location location){
	if (nvme_drive_deinit(location)!=0)
		return -1;
	return 0;
}
