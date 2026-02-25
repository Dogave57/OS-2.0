#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "cpu/mutex.h"
#include "cpu/idt.h"
#include "crypto/guid.h"
#include "crypto/random.h"
#include "drivers/apic.h"
#include "drivers/pcie.h"
#include "drivers/timer.h"
#include "subsystem/pcie.h"
#include "subsystem/drive.h"
#include "drivers/nvme.h"
static struct nvme_driver_info driverInfo = {0};
int nvme_driver_init(void){
	if (nvme_driver_init_isr_mapping_table()!=0){
		printf("failed to initialize CQ ISR mapping table\r\n");
		return -1;
	}
	struct drive_driver_vtable driveDriverVtable = {0};
	memset((void*)&driveDriverVtable, 0, sizeof(struct drive_driver_vtable));
	driveDriverVtable.readSectors = nvme_drive_subsystem_read;
	driveDriverVtable.writeSectors = nvme_drive_subsystem_write;
	driveDriverVtable.getDriveInfo = nvme_drive_subsystem_get_drive_info;
	driveDriverVtable.driveRegister = nvme_drive_subsystem_register_drive;
	driveDriverVtable.driveUnregister = nvme_drive_subsystem_unregister_drive;	
	if (drive_driver_register(driveDriverVtable, &driverInfo.driveDriverId)!=0){
		printf("failed to register PCIe NVMe protocol driver with drive subsystem\r\n");
		nvme_driver_deinit_isr_mapping_table();
		return -1;	
	}	
	struct pcie_driver_vtable pcieDriverVtable = {0};
	memset((void*)&pcieDriverVtable, 0, sizeof(struct pcie_driver_vtable));
	pcieDriverVtable.registerFunction = nvme_pcie_subsystem_function_register;
	pcieDriverVtable.unregisterFunction = nvme_pcie_subsystem_function_unregister;
	if (pcie_subsystem_driver_register(pcieDriverVtable, NVME_DRIVE_PCIE_CLASS, NVME_DRIVE_PCIE_SUBCLASS, &driverInfo.pcieDriverId)!=0){
		printf("failed to register PCIe NVMe protocol driver with PCIe subsystem\r\n");
		drive_driver_unregister(driverInfo.driveDriverId);
		nvme_driver_deinit_isr_mapping_table();
		return -1;
	}
	return 0;
}
int nvme_driver_init_isr_mapping_table(void){
	struct nvme_isr_mapping_table_entry* pMappingTable = (struct nvme_isr_mapping_table_entry*)0x0;
	uint64_t mappingTableSize = sizeof(struct nvme_isr_mapping_table_entry)*IDT_MAX_ENTRIES;
	if (virtualAlloc((uint64_t*)&pMappingTable, mappingTableSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	driverInfo.pIsrMappingTable = pMappingTable;
	driverInfo.isrMappingTableSize = mappingTableSize;
	return 0;
}
int nvme_driver_deinit_isr_mapping_table(void){
	if (virtualFree((uint64_t)driverInfo.pIsrMappingTable, driverInfo.isrMappingTableSize)!=0)
		return -1;
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
int nvme_ring_doorbell(struct nvme_drive_desc* pDriveDesc, uint64_t doorbellId, uint8_t doorbellType, uint16_t value){
	if (!pDriveDesc)
		return -1;
	struct nvme_capabilities_register capabilities = pDriveDesc->pBaseRegisters->capabilities;	
	uint64_t doorbellStride = (1<<(capabilities.doorbell_stride))*4;
	uint64_t doorbellOffset = (((doorbellId*2)+doorbellType))*doorbellStride;
	volatile uint16_t* pDoorbell = (volatile uint16_t*)(((uint64_t)pDriveDesc->pDoorbellBase)+doorbellOffset);	
	*pDoorbell = value;	
	return 0;
}
int nvme_run_submission_queue(struct nvme_submission_queue_desc* pSubmissionQueueDesc){
	if (!pSubmissionQueueDesc)
		return -1;
	if (nvme_enable(pSubmissionQueueDesc->pDriveDesc)!=0)
		return -1;	
	uint16_t tail = pSubmissionQueueDesc->submissionEntry%pSubmissionQueueDesc->maxSubmissionEntryCount;
	uint64_t doorbellId = (pSubmissionQueueDesc->queueType==NVME_QUEUE_TYPE_ADMIN_SQ) ? 0x0 : pSubmissionQueueDesc->queueIndex+1;
	if (nvme_ring_doorbell(pSubmissionQueueDesc->pDriveDesc, doorbellId, NVME_DOORBELL_TYPE_SQ, tail)!=0)
		return -1;
	return 0;
}
int nvme_alloc_completion_queue(struct nvme_drive_desc* pDriveDesc, uint64_t maxEntryCount, uint8_t queueType, struct nvme_completion_queue_desc** ppCompletionQueueDesc){
	if (!pDriveDesc||!ppCompletionQueueDesc||!queueType)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t lapic_id = 0;
	if (lapic_get_id(&lapic_id)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (maxEntryCount>NVME_MAX_CQE_COUNT||!maxEntryCount)
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
	uint64_t completionQueueIndex = pDriveDesc->completionQueueCount;
	struct nvme_completion_queue_desc* pCompletionQueueDesc = &pDriveDesc->completionQueueList[completionQueueIndex];	
	struct nvme_completion_queue_desc completionQueueDesc = {0};
	memset((void*)&completionQueueDesc, 0, sizeof(struct nvme_completion_queue_desc));
	uint64_t msixVector = completionQueueIndex;	
	uint8_t freeVector = 0;
	if (idt_get_free_vector(&freeVector)!=0){
		virtualFree((uint64_t)pCompletionQueue, completionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}
	volatile struct pcie_msix_msg_ctrl* pMsgControl = pDriveDesc->pMsgControl;	
	if (pcie_msix_enable(pMsgControl)!=0){
		virtualFree((uint64_t)pCompletionQueue, completionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pcie_msix_enable_entry(pDriveDesc->location, pMsgControl, msixVector)!=0){
		printf("failed to enable MSI-X entry\r\n");
		virtualFree((uint64_t)pCompletionQueue, completionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}
	if (pcie_set_msix_entry(pDriveDesc->location, pMsgControl, msixVector, lapic_id, freeVector)!=0){
		virtualFree((uint64_t)pCompletionQueue, completionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}	
	if (idt_add_entry(freeVector, (uint64_t)nvme_completion_queue_isr, 0x8E, 0x0, 0x0)!=0){
		printf("failed to set NVme CQ ISR entry\r\n");
		virtualFree((uint64_t)pCompletionQueue, completionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}	
	struct nvme_isr_mapping_table_entry* pMappingTableEntry = driverInfo.pIsrMappingTable+freeVector;
	struct nvme_isr_mapping_table_entry mappingTableEntry = {0};
	memset((void*)&mappingTableEntry, 0, sizeof(struct nvme_isr_mapping_table_entry));
	mappingTableEntry.pDriveDesc = pDriveDesc;
	mappingTableEntry.pCompletionQueueDesc = pCompletionQueueDesc;
	*pMappingTableEntry = mappingTableEntry;
	completionQueueDesc.pCompletionQueue = pCompletionQueue;
	completionQueueDesc.pCompletionQueue_phys = pCompletionQueue_phys;	
	completionQueueDesc.maxCompletionEntryCount = maxEntryCount;
	completionQueueDesc.phase = 1;	
	completionQueueDesc.queueType = queueType;	
	completionQueueDesc.queueIndex = completionQueueIndex;
	completionQueueDesc.queueId = (completionQueueIndex*2)+1;
	completionQueueDesc.msixVector = msixVector;	
	completionQueueDesc.pDriveDesc = pDriveDesc;	
	*pCompletionQueueDesc = completionQueueDesc;	
	*ppCompletionQueueDesc = pCompletionQueueDesc;	
	pDriveDesc->completionQueueCount++;
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
int nvme_get_completion_queue_desc(struct nvme_drive_desc* pDriveDesc, uint8_t queueId, struct nvme_completion_queue_desc** ppCompletionQueueDesc){
	if (!pDriveDesc||!ppCompletionQueueDesc)
		return -1;
	struct nvme_completion_queue_desc* pCompletionQueueDesc = &pDriveDesc->completionQueueList[queueId];
	*ppCompletionQueueDesc;	
	return 0;
}
int nvme_alloc_io_completion_queue(struct nvme_drive_desc* pDriveDesc, struct nvme_alloc_queue_packet* pPacket, struct nvme_completion_qe_desc* pCompletionQeDesc){
	if (!pDriveDesc||!pPacket||!pCompletionQeDesc)
		return -1;
	if (!pPacket->io_cq.maxEntryCount)
		pPacket->io_cq.maxEntryCount = NVME_DEFAULT_CQE_COUNT;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct nvme_completion_queue_desc* pCompletionQueueDesc = (struct nvme_completion_queue_desc*)0x0;
	if (nvme_alloc_completion_queue(pDriveDesc, pPacket->io_cq.maxEntryCount, NVME_QUEUE_TYPE_IO_CQ, &pCompletionQueueDesc)!=0){
		printf("failed to allocate I/O completion queue\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	memset((void*)&submissionQeDesc, 0, sizeof(struct nvme_submission_qe_desc));
	struct nvme_submission_qe submissionQe = {0};
	memset((void*)&submissionQe, 0, sizeof(struct nvme_submission_qe));
	submissionQe.create_io_cq.opcode = NVME_ADMIN_CREATE_IO_COMPLETION_QUEUE_OPCODE;
	submissionQe.create_io_cq.prp0 = pCompletionQueueDesc->pCompletionQueue_phys;
	submissionQe.create_io_cq.queue_id = pCompletionQueueDesc->queueId;	
	submissionQe.create_io_cq.queue_size = pPacket->io_cq.maxEntryCount-1;	
	submissionQe.create_io_cq.interrupt_enable = 1;
	submissionQe.create_io_cq.physically_contiguous = 1;
	submissionQe.create_io_cq.msix_vector = pCompletionQueueDesc->msixVector;
	if (nvme_alloc_submission_qe(&pDriveDesc->submissionQueueList[0], submissionQe, &submissionQeDesc)!=0){
		printf("failed to allocate I/O CQ creation SQE\r\n");
		nvme_free_completion_queue(pCompletionQueueDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (nvme_run_submission_queue(&pDriveDesc->submissionQueueList[0])!=0){
		printf("failed to run admin SQ\r\n");
		nvme_free_completion_queue(pCompletionQueueDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	while (!submissionQeDesc.cmdComplete){};
	pPacket->io_cq.pCompletionQueueDesc = pCompletionQueueDesc;	
	*pCompletionQeDesc = submissionQeDesc.completionQeDesc;	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int nvme_free_io_completion_queue(struct nvme_free_queue_packet* pPacket){
	if (!pPacket)
		return -1;	
	struct nvme_drive_desc* pDriveDesc = pPacket->io_cq.pCompletionQueueDesc->pDriveDesc;
	if (!pDriveDesc)
		return -1;
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	memset((void*)&submissionQeDesc, 0, sizeof(struct nvme_submission_qe_desc));
	struct nvme_submission_qe submissionQe = {0};
	memset((void*)&submissionQe, 0, sizeof(struct nvme_submission_qe));
	submissionQe.delete_io_cq.opcode = NVME_ADMIN_DELETE_IO_COMPLETION_QUEUE_OPCODE;
	submissionQe.delete_io_cq.queue_id = pPacket->io_cq.pCompletionQueueDesc->queueId;	
	if (nvme_alloc_submission_qe(&pDriveDesc->submissionQueueList[0], submissionQe, &submissionQeDesc)!=0){
		printf("failed to allocate admin SQE\r\n");
		nvme_free_completion_queue(pPacket->io_cq.pCompletionQueueDesc);	
		return -1;
	}	
	if (nvme_run_submission_queue(&pDriveDesc->submissionQueueList[0])!=0){
		nvme_free_completion_queue(pPacket->io_cq.pCompletionQueueDesc);
		return -1;
	}	
	if (nvme_free_completion_queue(pPacket->io_cq.pCompletionQueueDesc)!=0)
		return -1;
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
	if (pCompletionQueueDesc->completionEntry>pCompletionQueueDesc->maxCompletionEntryCount-1){
		pCompletionQueueDesc->completionEntry = 0;
		pCompletionQueueDesc->phase = !pCompletionQueueDesc->phase;
	}
	return 0;
}
int nvme_get_status_code_name(uint8_t statusCode, const unsigned char** ppName){
	if (!ppName)
		return -1;
	if (statusCode>0x81){
		*ppName = (const unsigned char*)"Unknown status code";
		return 0;
	}	
	const unsigned char* pName = statusCodeNameMap[statusCode];
	*ppName = pName;	
	return 0;
}
int nvme_get_status_code_type_name(uint8_t statusCodeType, const unsigned char** ppName){
	if (!ppName)
		return -1;
	if (statusCodeType>0x03){
		*ppName = (const unsigned char*)"Unknown status code type";	
		return 0;
	}
	const unsigned char* pName = statusCodeTypeNameMap[statusCodeType];
	*ppName = pName;	
	return 0;
}
int nvme_get_submission_qe_desc(struct nvme_submission_queue_desc* pSubmissionQueueDesc, uint64_t qeIndex, struct nvme_submission_qe_desc** ppSubmissionQeDesc){
	if (!pSubmissionQueueDesc||!ppSubmissionQeDesc)
		return -1;
	struct nvme_submission_qe_desc* pSubmissionQeDesc = pSubmissionQueueDesc->ppSubmissionQeDescList[qeIndex];
	if (!pSubmissionQeDesc)
		return -1;
	*ppSubmissionQeDesc = pSubmissionQeDesc;	
	return 0;
}
int nvme_alloc_submission_qe(struct nvme_submission_queue_desc* pSubmissionQueueDesc, struct nvme_submission_qe entry, struct nvme_submission_qe_desc* pSubmissionQeDesc){
	if (!pSubmissionQueueDesc||!pSubmissionQeDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t submissionQeIndex = pSubmissionQueueDesc->submissionEntry;	
	volatile struct nvme_submission_qe* pEntry = pSubmissionQueueDesc->pSubmissionQueue+submissionQeIndex;
	entry.generic.cmd_ident.submissionQueueIndex = pSubmissionQueueDesc->queueIndex;
	entry.generic.cmd_ident.submissionQeIndex = submissionQeIndex;	
	*pEntry = entry;
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	memset((void*)&submissionQeDesc, 0, sizeof(struct nvme_submission_qe_desc));
	submissionQeDesc.qeIndex = submissionQeIndex;
	submissionQeDesc.pSubmissionQueueDesc = pSubmissionQueueDesc;
	pSubmissionQueueDesc->ppSubmissionQeDescList[submissionQeIndex] = pSubmissionQeDesc;	
	pSubmissionQueueDesc->submissionEntry++;
	if (pSubmissionQueueDesc->submissionEntry>pSubmissionQueueDesc->maxSubmissionEntryCount-1){
		pSubmissionQueueDesc->submissionEntry = 0;
	}
	*pSubmissionQeDesc = submissionQeDesc;
	mutex_unlock(&mutex);
	return 0;
}
int nvme_alloc_submission_queue(struct nvme_drive_desc* pDriveDesc, uint64_t maxEntryCount, uint8_t queueType, struct nvme_submission_queue_desc** ppSubmissionQueueDesc, struct nvme_completion_queue_desc* pCompletionQueueDesc){
	if (!pDriveDesc||!ppSubmissionQueueDesc||!pCompletionQueueDesc||!queueType)
		return -1;
	if (maxEntryCount>NVME_MAX_SQE_COUNT)
		return -1;
	if (!maxEntryCount)
		maxEntryCount = NVME_DEFAULT_SQE_COUNT;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t maxSubmissionQueueCount = sizeof(pDriveDesc->submissionQueueList)/sizeof(struct nvme_submission_queue_desc);	
	if (pDriveDesc->submissionQueueCount>=maxSubmissionQueueCount){
		mutex_unlock(&mutex);
		return -1;
	}
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
	struct nvme_submission_qe_desc** ppSubmissionQeDescList = (struct nvme_submission_qe_desc**)0x0;
	uint64_t submissionQueueDescListSize = sizeof(struct nvme_submission_qe_desc*)*maxEntryCount;
	if (virtualAlloc((uint64_t*)&ppSubmissionQeDescList, submissionQueueDescListSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate submission queue desc list\r\n");
		virtualFree((uint64_t)pSubmissionQueue, submissionQueueSize);
		mutex_unlock(&mutex);
		return -1;
	}	
	memset((void*)ppSubmissionQeDescList, 0, submissionQueueDescListSize);
	uint8_t submissionQueueIndex = pDriveDesc->submissionQueueCount;
	struct nvme_submission_queue_desc* pSubmissionQueueDesc = &pDriveDesc->submissionQueueList[submissionQueueIndex];
	memset((void*)pSubmissionQueueDesc, 0, sizeof(struct nvme_submission_queue_desc));	
	pSubmissionQueueDesc->pSubmissionQueue = pSubmissionQueue;
	pSubmissionQueueDesc->pSubmissionQueue_phys = pSubmissionQueue_phys;	
	pSubmissionQueueDesc->ppSubmissionQeDescList = ppSubmissionQeDescList;	
	pSubmissionQueueDesc->maxSubmissionEntryCount = maxEntryCount;
	pSubmissionQueueDesc->queueType = queueType;	
	pSubmissionQueueDesc->queueIndex = submissionQueueIndex;	
	pSubmissionQueueDesc->queueId = (submissionQueueIndex*2);
	pSubmissionQueueDesc->pCompletionQueueDesc = pCompletionQueueDesc;	
	pSubmissionQueueDesc->pDriveDesc = pDriveDesc;
	pDriveDesc->submissionQueueCount++;
	*ppSubmissionQueueDesc = pSubmissionQueueDesc;	
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
int nvme_get_submission_queue_desc(struct nvme_drive_desc* pDriveDesc, uint8_t queueId, struct nvme_submission_queue_desc** ppSubmissionQueueDesc){
	if (!pDriveDesc||!ppSubmissionQueueDesc)
		return -1;
	struct nvme_submission_queue_desc* pSubmissionQueueDesc = &pDriveDesc->submissionQueueList[queueId];
	*ppSubmissionQueueDesc = pSubmissionQueueDesc;	
	return 0;
}
int nvme_alloc_io_submission_queue(struct nvme_drive_desc* pDriveDesc, struct nvme_alloc_queue_packet* pPacket, struct nvme_completion_qe_desc* pCompletionQeDesc){
	if (!pDriveDesc||!pPacket||!pCompletionQeDesc)
		return -1;
	if (!pPacket->io_sq.pCompletionQueueDesc)
		return -1;
	if (!pPacket->io_sq.maxEntryCount)
		pPacket->io_sq.maxEntryCount = NVME_DEFAULT_SQE_COUNT;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct nvme_completion_queue_desc* pCompletionQueueDesc = pPacket->io_sq.pCompletionQueueDesc;	
	struct nvme_submission_queue_desc* pSubmissionQueueDesc = (struct nvme_submission_queue_desc*)0x0;
	if (nvme_alloc_submission_queue(pDriveDesc, pPacket->io_sq.maxEntryCount, NVME_QUEUE_TYPE_IO_SQ, &pSubmissionQueueDesc, pCompletionQueueDesc)!=0){
		printf("failed to allocate I/O submission queue\r\n");	
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	memset((void*)&submissionQeDesc, 0, sizeof(struct nvme_submission_qe_desc));
	struct nvme_completion_qe* pCompletionQe = &submissionQeDesc.completionQeDesc.completionQe;
	struct nvme_submission_qe submissionQe = {0};
	memset((void*)&submissionQe, 0, sizeof(struct nvme_submission_qe));
	submissionQe.create_io_sq.opcode = NVME_ADMIN_CREATE_IO_SUBMISSION_QUEUE_OPCODE;
	submissionQe.create_io_sq.prp0 = pSubmissionQueueDesc->pSubmissionQueue_phys;	
	submissionQe.create_io_sq.queue_id = pSubmissionQueueDesc->queueId;
	submissionQe.create_io_sq.queue_size = pSubmissionQueueDesc->maxSubmissionEntryCount-1;
	submissionQe.create_io_sq.physically_contiguous = 1;
	submissionQe.create_io_sq.completion_queue_id = pCompletionQueueDesc->queueId;
	if (nvme_alloc_submission_qe(&pDriveDesc->submissionQueueList[0], submissionQe, &submissionQeDesc)!=0){
		printf("failed to allocate submission QE\r\n");
		nvme_free_submission_queue(pSubmissionQueueDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	if (nvme_run_submission_queue(&pDriveDesc->submissionQueueList[0])!=0){
		printf("failed to admin admin SQ\r\n");
		nvme_free_submission_queue(pSubmissionQueueDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	while (!submissionQeDesc.cmdComplete){};
	pPacket->io_sq.pSubmissionQueueDesc = pSubmissionQueueDesc;	
	*pCompletionQeDesc = submissionQeDesc.completionQeDesc;	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int nvme_free_io_submission_queue(struct nvme_free_queue_packet* pPacket){
	if (!pPacket)
		return -1;
	struct nvme_drive_desc* pDriveDesc = pPacket->io_sq.pSubmissionQueueDesc->pDriveDesc;
	if (!pDriveDesc)
		return -1;
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	memset((void*)&submissionQeDesc, 0, sizeof(struct nvme_submission_qe_desc));
	struct nvme_submission_qe submissionQe = {0};
	memset((void*)&submissionQe, 0, sizeof(struct nvme_submission_qe));
	submissionQe.delete_io_sq.opcode = NVME_ADMIN_DELETE_IO_SUBMISSION_QUEUE_OPCODE;
	submissionQe.delete_io_sq.queue_id = pPacket->io_sq.pSubmissionQueueDesc->queueId;
	if (nvme_alloc_submission_qe(&pDriveDesc->submissionQueueList[0], submissionQe, &submissionQeDesc)!=0){
		printf("failed to allocate admin SQE\r\n");
		nvme_free_submission_queue(pPacket->io_sq.pSubmissionQueueDesc);
		return -1;
	}	
	if (nvme_run_submission_queue(&pDriveDesc->submissionQueueList[0])!=0){
		printf("failed to run admin SQ\r\n");
		nvme_free_submission_queue(pPacket->io_sq.pSubmissionQueueDesc);
		return -1;
	}
	if (nvme_free_submission_queue(pPacket->io_sq.pSubmissionQueueDesc)!=0)
		return -1;	
	return 0;
}
int nvme_alloc_admin_submission_queue(struct nvme_drive_desc* pDriveDesc, struct nvme_alloc_queue_packet* pPacket){
	if (!pDriveDesc||!pPacket)
		return -1;
	struct nvme_submission_queue_desc* pSubmissionQueueDesc = (struct nvme_submission_queue_desc*)0x0;
	if (nvme_alloc_submission_queue(pDriveDesc, NVME_DEFAULT_SQE_COUNT, NVME_QUEUE_TYPE_ADMIN_SQ, &pSubmissionQueueDesc, pPacket->admin_sq.pCompletionQueueDesc)!=0){
		printf("failed to allocate admin SQ\r\n");
		return -1;
	}	
	struct nvme_free_queue_packet freePacket = {0};
	memset((void*)&freePacket, 0, sizeof(struct nvme_free_queue_packet));
	freePacket.queueType = NVME_QUEUE_TYPE_ADMIN_SQ;
	freePacket.admin_sq.pSubmissionQueueDesc = pSubmissionQueueDesc;
	pPacket->admin_sq.pSubmissionQueueDesc = pSubmissionQueueDesc;
	return 0;
}
int nvme_free_admin_submission_queue(struct nvme_free_queue_packet* pPacket){
	if (!pPacket)
		return -1;
	if (nvme_free_submission_queue(pPacket->admin_sq.pSubmissionQueueDesc)!=0)
		return -1;	
	return 0;
}
int nvme_alloc_admin_completion_queue(struct nvme_drive_desc* pDriveDesc, struct nvme_alloc_queue_packet* pPacket){
	if (!pDriveDesc||!pPacket)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);	
	uint64_t lapic_id = 0;
	if (lapic_get_id(&lapic_id)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct nvme_completion_queue_desc* pCompletionQueueDesc = (struct nvme_completion_queue_desc*)0x0;
	if (nvme_alloc_completion_queue(pDriveDesc, NVME_DEFAULT_CQE_COUNT, NVME_QUEUE_TYPE_ADMIN_CQ, &pCompletionQueueDesc)!=0){
		printf("failed to allocate admin completion queue\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	pPacket->admin_cq.pCompletionQueueDesc = pCompletionQueueDesc;	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int nvme_free_admin_completion_queue(struct nvme_free_queue_packet* pPacket){
	if (!pPacket)
		return -1;
	if (nvme_free_completion_queue(pPacket->admin_cq.pCompletionQueueDesc)!=0)
		return -1;
	return 0;
}
int nvme_completion_queue_interrupt(uint8_t vector){
	struct nvme_isr_mapping_table_entry* pMappingTableEntry = driverInfo.pIsrMappingTable+vector;
	struct nvme_drive_desc* pDriveDesc = pMappingTableEntry->pDriveDesc;	
	volatile struct nvme_completion_qe* pCompletionQe = (volatile struct nvme_completion_qe*)0x0;
	struct nvme_completion_queue_desc* pCompletionQueueDesc = pMappingTableEntry->pCompletionQueueDesc;	
	uint64_t processedCount = 0;
	while (!nvme_get_current_completion_qe(pCompletionQueueDesc, &pCompletionQe)&&processedCount<pCompletionQueueDesc->maxCompletionEntryCount){
		if (!pCompletionQe){
			printf("invalid completion QE\r\n");
			break;
		}
		struct nvme_submission_queue_desc* pSubmissionQueueDesc = &pDriveDesc->submissionQueueList[pCompletionQe->cmd_ident.submissionQueueIndex];	
		uint8_t phase_bit = (pCompletionQe->status.phase_bit!=0) ? 1 : 0;
		if (phase_bit!=pCompletionQueueDesc->phase&&!processedCount){
			printf("ghost admin completion queue ISR trigger\r\n");	
			const unsigned char* statusCodeName = "Unknown status code\r\n";
			nvme_get_status_code_name(pCompletionQe->status.status_code, &statusCodeName);
			printf("status code (%s)\r\n", statusCodeName);	
			while (1){};
			return -1;
		}
		if (phase_bit!=pCompletionQueueDesc->phase){
			break;
		}
		struct nvme_submission_qe_desc* pSubmissionQeDesc = (struct nvme_submission_qe_desc*)0x0;
		if (nvme_get_submission_qe_desc(pSubmissionQueueDesc, pCompletionQe->cmd_ident.submissionQeIndex, &pSubmissionQeDesc)!=0){
			printf("failed to get submission queue entry descriptor\r\n");
			return -1;
		}
		struct nvme_completion_qe_desc completionQeDesc = {0};
		memset((void*)&completionQeDesc, 0, sizeof(struct nvme_completion_qe_desc));
		completionQeDesc.completionQe = *pCompletionQe;
		completionQeDesc.qeIndex = pSubmissionQueueDesc->pCompletionQueueDesc->completionEntry;
		pSubmissionQeDesc->cmdComplete = 1;
		pSubmissionQeDesc->completionQeDesc = completionQeDesc;
		pSubmissionQeDesc->pSubmissionQueueDesc = pSubmissionQueueDesc;	
		if (nvme_acknowledge_completion_qe(pCompletionQueueDesc)!=0){
			printf("failed to acknowledge completion QE\r\n");
			return -1;
		}
		processedCount++;
		if (!pCompletionQueueDesc->completionEntry)
			break;
	}
	uint64_t doorbellIndex = pCompletionQueueDesc->queueType==NVME_QUEUE_TYPE_ADMIN_CQ ? 0x0 : (pCompletionQueueDesc->queueIndex+2);	
	if (nvme_ring_doorbell(pDriveDesc, doorbellIndex, NVME_DOORBELL_TYPE_CQ, pCompletionQueueDesc->completionEntry)!=0){
		printf("failed to ring doorbell\r\n");
		return -1;
	}
	return 0;
}
int nvme_identify(struct nvme_drive_desc* pDriveDesc, uint32_t namespaceId, uint32_t identifyType, unsigned char* pBuffer, struct nvme_completion_qe_desc* pCompletionQeDesc){
	if (!pDriveDesc||!pBuffer||!pCompletionQeDesc)
		return -1;
	uint64_t pBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)pBuffer, &pBuffer_phys)!=0)
		return -1;
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	memset((void*)&submissionQeDesc, 0, sizeof(struct nvme_submission_qe_desc));
	struct nvme_submission_qe submissionQe = {0};
	memset((void*)&submissionQe, 0, sizeof(struct nvme_submission_qe));
	submissionQe.identify.opcode = NVME_ADMIN_IDENTIFY_OPCODE;
	submissionQe.identify.namespace_id = namespaceId;
	submissionQe.identify.prp1 = pBuffer_phys;
	submissionQe.identify.identify_type = identifyType;
	if (nvme_alloc_submission_qe(&pDriveDesc->submissionQueueList[0], submissionQe, &submissionQeDesc)!=0){
		printf("failed to allocate submission QE\r\n");
		return -1;
	}
	if (nvme_run_submission_queue(&pDriveDesc->submissionQueueList[0])!=0){
		printf("failed to run submission queue\r\n");
		return -1;
	}	
	while (!submissionQeDesc.cmdComplete){};
	*pCompletionQeDesc = submissionQeDesc.completionQeDesc;	
	return 0;
}
int nvme_get_namespace_desc(struct nvme_drive_desc* pDriveDesc, uint32_t namespaceId, struct nvme_namespace_desc** ppNamespaceDesc){
	if (!pDriveDesc||!ppNamespaceDesc)
		return -1;
	struct nvme_namespace_desc* pNamespaceDesc = pDriveDesc->pActiveNamespaceDescList+namespaceId;
	*ppNamespaceDesc = pNamespaceDesc;	
	return 0;
}
int nvme_read(struct nvme_drive_desc* pDriveDesc, uint32_t namespaceId, uint64_t lba, uint32_t sectorCount, unsigned char* pBuffer, struct nvme_completion_qe_desc* pCompletionQeDesc){
	if (!pDriveDesc||!namespaceId||!pBuffer||!pCompletionQeDesc)
		return -1;
	struct nvme_namespace_desc* pNamespaceDesc = (struct nvme_namespace_desc*)0x0;
	if (nvme_get_namespace_desc(pDriveDesc, namespaceId, &pNamespaceDesc)!=0)
		return -1;
	uint64_t maxTransferLength = (pDriveDesc->maxTransferLength<((PAGE_SIZE/sizeof(uint64_t))*PAGE_SIZE)) ? pDriveDesc->maxTransferLength : MEM_KB*256;
	uint64_t maxSectorCount = (maxTransferLength/2)/pNamespaceDesc->sectorSize;
	if (sectorCount>maxSectorCount){
		uint64_t cmdCount = (sectorCount/maxSectorCount)+((maxSectorCount%sectorCount) ? 1 : 0);
		for (uint64_t i = 0;i<cmdCount;i++){
			uint64_t toRead = (i==cmdCount-1&&sectorCount%maxSectorCount) ? (sectorCount%maxSectorCount) : maxSectorCount;
			uint64_t readLba = lba+(i*maxSectorCount);	
			unsigned char* pReadBuffer = pBuffer+(i*(maxSectorCount*pNamespaceDesc->sectorSize));
			struct nvme_completion_qe_desc completionQeDesc = {0};
			if (nvme_read(pDriveDesc, namespaceId, readLba, toRead, pReadBuffer, &completionQeDesc)!=0){
				*pCompletionQeDesc = completionQeDesc;
				return -1;
			}
			if (completionQeDesc.completionQe.status.status_code==NVME_STATUS_CODE_SUCCESS)
				continue;
			*pCompletionQeDesc = completionQeDesc;
			return 0;
		}	
		return 0;
	}
	uint64_t sectorCountPerPage = PAGE_SIZE/pNamespaceDesc->sectorSize;
	uint64_t pageCount = ((sectorCount-1)/sectorCountPerPage);	
	uint64_t pBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)pBuffer, &pBuffer_phys)!=0)
		return -1;
	uint64_t* pPageList = (uint64_t*)0x0;
	uint64_t pPageList_phys = 0;
	if (sectorCount>sectorCountPerPage){
		if (virtualAllocPage((uint64_t*)&pPageList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
			printf("failed to allocate page for page list\r\n");
			return -1;
		}	
		if (virtualToPhysical((uint64_t)pPageList, &pPageList_phys)!=0){
			virtualFreePage((uint64_t)pPageList, 0);
			return -1;
		}
	}
	for (uint64_t i = 0;i<pageCount;i++){
		unsigned char* pPage = pBuffer+((i+1)*PAGE_SIZE);
		uint64_t pPage_phys = 0;
		if (virtualToPhysical((uint64_t)pPage, &pPage_phys)!=0){
			virtualFreePage((uint64_t)pPageList, 0);
			return -1;
		}
		if (pageCount==1){
			pPageList_phys = pPage_phys;
			break;
		}	
		pPageList[i] = pPage_phys;
	}
	struct nvme_submission_queue_desc* pSubmissionQueueDesc = &pDriveDesc->submissionQueueList[1];	
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	struct nvme_submission_qe submissionQe = {0};
	memset((void*)&submissionQe, 0, sizeof(struct nvme_submission_qe));
	submissionQe.read_sectors.opcode = NVME_IO_READ_OPCODE;
	submissionQe.read_sectors.prp0 = pBuffer_phys;
	submissionQe.read_sectors.prp1 = pPageList_phys;
	submissionQe.read_sectors.namespace_id = namespaceId;
	submissionQe.read_sectors.lba = lba;
	submissionQe.read_sectors.sector_count = sectorCount-1;
	if (nvme_alloc_submission_qe(pSubmissionQueueDesc, submissionQe, &submissionQeDesc)!=0){
		printf("failed to allocate read sectors I/O SQE\r\n");
		return -1;
	}	
	if (nvme_run_submission_queue(pSubmissionQueueDesc)!=0){
		printf("failed to run I/O SQ\r\n");
		return -1;
	}	
	while (!submissionQeDesc.cmdComplete){};
	if (pPageList)
		virtualFreePage((uint64_t)pPageList, 0);
	*pCompletionQeDesc = submissionQeDesc.completionQeDesc;	
	return 0;
}
int nvme_write(struct nvme_drive_desc* pDriveDesc, uint32_t namespaceId, uint64_t lba, uint32_t sectorCount, unsigned char* pBuffer, struct nvme_completion_qe_desc* pCompletionQeDesc){
	if (!pDriveDesc||!namespaceId||!pBuffer||!pCompletionQeDesc)
		return -1;
	struct nvme_namespace_desc* pNamespaceDesc = (struct nvme_namespace_desc*)0x0;
	if (nvme_get_namespace_desc(pDriveDesc, namespaceId, &pNamespaceDesc)!=0)
		return -1;
	uint64_t maxTransferLength = (pDriveDesc->maxTransferLength<((PAGE_SIZE/sizeof(uint64_t))*PAGE_SIZE)) ? pDriveDesc->maxTransferLength : MEM_KB*256;
	uint64_t maxSectorCount = (maxTransferLength/2)/pNamespaceDesc->sectorSize;
	if (sectorCount>maxSectorCount){
		uint64_t cmdCount = (sectorCount/maxSectorCount)+((maxSectorCount%sectorCount) ? 1 : 0);
		for (uint64_t i = 0;i<cmdCount;i++){
			uint64_t toWrite = (i==cmdCount-1&&sectorCount%maxSectorCount) ? (sectorCount%maxSectorCount) : maxSectorCount;
			uint64_t writeLba = lba+(i*maxSectorCount);	
			unsigned char* pWriteBuffer = pBuffer+(i*(maxSectorCount*pNamespaceDesc->sectorSize));
			struct nvme_completion_qe_desc completionQeDesc = {0};
			if (nvme_read(pDriveDesc, namespaceId, writeLba, toWrite, pWriteBuffer, &completionQeDesc)!=0){
				*pCompletionQeDesc = completionQeDesc;
				return -1;
			}
			if (completionQeDesc.completionQe.status.status_code==NVME_STATUS_CODE_SUCCESS)
				continue;
			*pCompletionQeDesc = completionQeDesc;
			return 0;
		}	
		return 0;
	}
	uint64_t sectorCountPerPage = PAGE_SIZE/pNamespaceDesc->sectorSize;
	uint64_t pageCount = ((sectorCount-1)/sectorCountPerPage);	
	uint64_t pBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)pBuffer, &pBuffer_phys)!=0)
		return -1;
	uint64_t* pPageList = (uint64_t*)0x0;
	uint64_t pPageList_phys = 0;
	if (sectorCount>sectorCountPerPage){
		if (virtualAllocPage((uint64_t*)&pPageList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
			printf("failed to allocate page for page list\r\n");
			return -1;
		}	
		if (virtualToPhysical((uint64_t)pPageList, &pPageList_phys)!=0){
			virtualFreePage((uint64_t)pPageList, 0);
			return -1;
		}
	}
	for (uint64_t i = 0;i<pageCount;i++){
		unsigned char* pPage = pBuffer+((i+1)*PAGE_SIZE);
		uint64_t pPage_phys = 0;
		if (virtualToPhysical((uint64_t)pPage, &pPage_phys)!=0){
			virtualFreePage((uint64_t)pPageList, 0);
			return -1;
		}
		if (pageCount==1){
			pPageList_phys = pPage_phys;
			break;
		}	
		pPageList[i] = pPage_phys;
	}
	struct nvme_submission_queue_desc* pSubmissionQueueDesc = &pDriveDesc->submissionQueueList[1];	
	struct nvme_submission_qe_desc submissionQeDesc = {0};
	struct nvme_submission_qe submissionQe = {0};
	memset((void*)&submissionQe, 0, sizeof(struct nvme_submission_qe));
	submissionQe.read_sectors.opcode = NVME_IO_WRITE_OPCODE;
	submissionQe.read_sectors.prp0 = pBuffer_phys;
	submissionQe.read_sectors.prp1 = pPageList_phys;
	submissionQe.read_sectors.namespace_id = namespaceId;
	submissionQe.read_sectors.lba = lba;
	submissionQe.read_sectors.sector_count = sectorCount-1;
	if (nvme_alloc_submission_qe(pSubmissionQueueDesc, submissionQe, &submissionQeDesc)!=0){
		printf("failed to allocate read sectors I/O SQE\r\n");
		return -1;
	}	
	if (nvme_run_submission_queue(pSubmissionQueueDesc)!=0){
		printf("failed to run I/O SQ\r\n");
		return -1;
	}	
	while (!submissionQeDesc.cmdComplete){};
	if (pPageList)
		virtualFreePage((uint64_t)pPageList, 0);
	*pCompletionQeDesc = submissionQeDesc.completionQeDesc;	
	return 0;
}
int nvme_namespace_init(struct nvme_drive_desc* pDriveDesc, uint32_t namespaceId, struct nvme_namespace_desc** ppNamespaceDesc){
	if (!pDriveDesc||!ppNamespaceDesc)
		return -1;
	struct nvme_namespace_info* pNamespaceInfo = (struct nvme_namespace_info*)0x0;
	if (virtualAllocPage((uint64_t*)&pNamespaceInfo, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate page for namespace info\r\n");
		return -1;
	}	
	struct nvme_completion_qe_desc completionQeDesc = {0};
	memset((void*)&completionQeDesc, 0, sizeof(struct nvme_completion_qe_desc));
	struct nvme_completion_qe* pCompletionQe = &completionQeDesc.completionQe;
	if (nvme_identify(pDriveDesc, namespaceId, NVME_IDENTIFY_CODE_GET_ALLOCATED_NAMESPACE_INFO_LEGACY, (unsigned char*)pNamespaceInfo, &completionQeDesc)!=0){
		printf("failed to identify namespace with ID: %d\r\n", namespaceId);
		return -1;
	}	
	if (pCompletionQe->status.status_code!=NVME_STATUS_CODE_SUCCESS){
		const unsigned char* statusCodeName = "Unknown status code";
		const unsigned char* statusCodeTypeName = "Unknown status code type";
		nvme_get_status_code_name(pCompletionQe->status.status_code, &statusCodeName);
		nvme_get_status_code_type_name(pCompletionQe->status.status_code_type, &statusCodeTypeName);
		printf("failed to identify namespace with ID: %d\r\n", namespaceId);
		printf("status code: %s\r\n", statusCodeName);
		printf("status code type: %s\r\n", statusCodeTypeName);	
		virtualFreePage((uint64_t)pNamespaceInfo, 0);
		return -1;
	}
	struct nvme_lba_format currentLbaFormat = pNamespaceInfo->lbaFormatList[pNamespaceInfo->active_lba_format_index];
	uint64_t sectorSize = 1<<((uint64_t)currentLbaFormat.lba_data_size);
	uint64_t namespaceSize = sectorSize*pNamespaceInfo->namespace_lba_count;
	struct nvme_namespace_desc* pNamespaceDesc = &pDriveDesc->pActiveNamespaceDescList[namespaceId];
	struct nvme_namespace_desc namespaceDesc = {0};
	memset((void*)&namespaceDesc, 0, sizeof(struct nvme_namespace_desc));	
	namespaceDesc.pDriveDesc = pDriveDesc;
	namespaceDesc.namespaceId = namespaceId;	
	namespaceDesc.lbaCount = pNamespaceInfo->namespace_lba_count;
	namespaceDesc.sectorSize = sectorSize;
	namespaceDesc.namespaceSize = namespaceSize;	
	memcpy((void*)namespaceDesc.guid, (void*)pNamespaceInfo->namespace_guid, GUID_SIZE);
	if (!pDriveDesc->pFirstActiveNamespace)
		pDriveDesc->pFirstActiveNamespace = pNamespaceDesc;
	if (pDriveDesc->pLastActiveNamespace){
		pDriveDesc->pLastActiveNamespace->pFlink = pNamespaceDesc;
		pNamespaceDesc->pBlink = pDriveDesc->pLastActiveNamespace;
	}	
	pDriveDesc->pLastActiveNamespace = pNamespaceDesc;
	pDriveDesc->activeNamespaceCount++;
	*pNamespaceDesc = namespaceDesc;	
	virtualFreePage((uint64_t)pNamespaceInfo, 0);
	*ppNamespaceDesc = pNamespaceDesc;
	return 0;
}
int nvme_namespace_deinit(struct nvme_namespace_desc* pNamespaceDesc){
	if (!pNamespaceDesc)
		return -1;
	struct nvme_drive_desc* pDriveDesc = pNamespaceDesc->pDriveDesc;
	if (!pDriveDesc)
		return -1;
	if (pNamespaceDesc->pFlink)
		pNamespaceDesc->pFlink->pBlink = pNamespaceDesc->pBlink;
	if (pNamespaceDesc->pBlink)
		pNamespaceDesc->pBlink->pFlink = pNamespaceDesc->pFlink;
	if (pNamespaceDesc==pDriveDesc->pFirstActiveNamespace)
		pDriveDesc->pFirstActiveNamespace = pNamespaceDesc->pFlink;
	if (pNamespaceDesc==pDriveDesc->pLastActiveNamespace)
		pDriveDesc->pLastActiveNamespace = pNamespaceDesc->pBlink;
	pDriveDesc->activeNamespaceCount--;	
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
	volatile struct pcie_msix_msg_ctrl* pMsgControl = (struct pcie_msix_msg_ctrl*)0x0;
	if (pcie_msix_get_msg_ctrl(location, &pMsgControl)!=0){
		printf("failed to get MSI-X msg control\r\n");
		kfree((void*)pDriveDesc);
		return -1;
	}
	pDriveDesc->pBaseRegisters_phys = pBaseRegisters_phys;
	pDriveDesc->pBaseRegisters = pBaseRegisters;	
	pDriveDesc->pDoorbellBase = (volatile uint32_t*)((uint64_t)pBaseRegisters+NVME_DOORBELL_LIST_OFFSET);	
	pDriveDesc->pMsgControl = pMsgControl;
	pDriveDesc->location = location;
	if (nvme_disable(pDriveDesc)!=0){
		printf("failed to disable NVMe drive controller\r\n");
		kfree((void*)pDriveDesc);
		return -1;
	}	
	struct nvme_completion_qe_desc completionQeDesc = {0};
	memset((void*)&completionQeDesc, 0, sizeof(struct nvme_completion_qe_desc));
	struct nvme_completion_qe* pCompletionQe = &completionQeDesc.completionQe;	
	struct nvme_alloc_queue_packet allocQueuePacket = {0};
	memset((void*)&allocQueuePacket, 0, sizeof(struct nvme_alloc_queue_packet));
	allocQueuePacket.queueType = NVME_QUEUE_TYPE_ADMIN_CQ;
	if (nvme_alloc_admin_completion_queue(pDriveDesc, &allocQueuePacket)!=0){
		printf("failed to allocate admin completion queue\r\n");
		kfree((void*)pDriveDesc);
		return -1;
	}	
	struct nvme_completion_queue_desc* pAdminCompletionQueueDesc = allocQueuePacket.admin_cq.pCompletionQueueDesc;	
	memset((void*)&allocQueuePacket, 0, sizeof(struct nvme_alloc_queue_packet));
	allocQueuePacket.queueType = NVME_QUEUE_TYPE_ADMIN_SQ;	
	allocQueuePacket.admin_sq.pCompletionQueueDesc = pAdminCompletionQueueDesc;
	if (nvme_alloc_admin_submission_queue(pDriveDesc, &allocQueuePacket)!=0){
		printf("failed to allocate admin submission queue\r\n");
		kfree((void*)pDriveDesc);
		return -1;
	}
	struct nvme_submission_queue_desc* pAdminSubmissionQueueDesc = allocQueuePacket.admin_sq.pSubmissionQueueDesc;
	struct nvme_admin_queue_attribs adminQueueAttribs = {0};
	memset((void*)&adminQueueAttribs, 0, sizeof(struct nvme_admin_queue_attribs));
	adminQueueAttribs.admin_submission_queue_size = NVME_DEFAULT_SQE_COUNT-1;
	adminQueueAttribs.admin_completion_queue_size = NVME_DEFAULT_CQE_COUNT-1;
	pDriveDesc->pBaseRegisters->admin_queue_attribs = adminQueueAttribs;
	pDriveDesc->pBaseRegisters->admin_submission_queue_base = pAdminSubmissionQueueDesc->pSubmissionQueue_phys;
	pDriveDesc->pBaseRegisters->admin_completion_queue_base = pAdminCompletionQueueDesc->pCompletionQueue_phys;	
	struct nvme_controller_config controllerConfig = pDriveDesc->pBaseRegisters->controller_config;
	controllerConfig.memory_page_size = 0;
	controllerConfig.io_submission_queue_entry_size = 6;
	controllerConfig.io_completion_queue_entry_size = 4;	
	pDriveDesc->pBaseRegisters->controller_config = controllerConfig;	
	struct nvme_controller_info* pControllerInfo = (struct nvme_controller_info*)0x0;
	if (virtualAllocPage((uint64_t*)&pControllerInfo, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate NVMe controller info page\r\n");	
		nvme_drive_deinit(pDriveDesc);	
		return -1;
	}	
	memset((void*)pControllerInfo, 0, PAGE_SIZE);	
	pDriveDesc->pControllerInfo = pControllerInfo;
	if (nvme_identify(pDriveDesc, 0x0, NVME_IDENTIFY_CODE_CONTROLLER, (unsigned char*)pControllerInfo, &completionQeDesc)!=0){
		printf("failed to identify host controller\r\n");
		nvme_drive_deinit(pDriveDesc);	
		return -1;
	}	
	if (pCompletionQe->status.status_code!=NVME_STATUS_CODE_SUCCESS){
		const unsigned char* statusCodeName = "Unknown status code";
		nvme_get_status_code_name(pCompletionQe->status.status_code, &statusCodeName);
		printf("failed to get NVMe controller info (%s)\r\n", statusCodeName);	
		nvme_drive_deinit(pDriveDesc);	
		return -1;
	}
	uint64_t maxTransferLength = (1<<pControllerInfo->max_data_transfer_size)*PAGE_SIZE;
	if (!maxTransferLength)
		maxTransferLength = MEM_KB*256;
	pDriveDesc->maxTransferLength = maxTransferLength;
	uint32_t* pActiveNamespaceList = (uint32_t*)0x0;
	if (virtualAllocPage((uint64_t*)&pActiveNamespaceList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate active namespace list\r\n");
		nvme_drive_deinit(pDriveDesc);	
		return -1;
	}
	memset((void*)pActiveNamespaceList, 0, PAGE_SIZE);
	if (nvme_identify(pDriveDesc, 0x0, NVME_IDENTIFY_CODE_GET_ACTIVE_NAMESPACE_LIST, (unsigned char*)pActiveNamespaceList, &completionQeDesc)!=0){
		printf("failed to get active namespace list\r\n");
		nvme_drive_deinit(pDriveDesc);
		virtualFreePage((uint64_t)pActiveNamespaceList, 0);
		return -1;
	}	
	if (pCompletionQe->status.status_code!=NVME_STATUS_CODE_SUCCESS){
		const unsigned char* statusCodeName = "Unknown status code";
		const unsigned char* statusCodeTypeName = "Unknown status code type";
		nvme_get_status_code_name(pCompletionQe->status.status_code, &statusCodeName);
		nvme_get_status_code_type_name(pCompletionQe->status.status_code_type, &statusCodeTypeName);
		printf("failed to get active namespace list\r\n");
		printf("status code (%s)\r\n", statusCodeName);
		printf("status code type (%s)\r\n", statusCodeTypeName);
		nvme_drive_deinit(pDriveDesc);
		virtualFreePage((uint64_t)pActiveNamespaceList, 0);
		return -1;
	}	
	struct nvme_namespace_desc* pActiveNamespaceDescList = (struct nvme_namespace_desc*)0x0;
	uint64_t activeNamespaceDescListSize = pControllerInfo->namespace_count*sizeof(struct nvme_namespace_desc);
	if (virtualAlloc((uint64_t*)&pActiveNamespaceDescList, activeNamespaceDescListSize, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate active namespace descriptor list\r\n");
		nvme_drive_deinit(pDriveDesc);
		virtualFreePage((uint64_t)pActiveNamespaceList, 0);
		return -1;
	}	
	pDriveDesc->pActiveNamespaceDescList = pActiveNamespaceDescList;
	pDriveDesc->activeNamespaceDescListSize = activeNamespaceDescListSize;
	for (uint64_t i = 0;i<PAGE_SIZE/sizeof(uint32_t)&&pActiveNamespaceList[i];i++){
		uint32_t namespaceId = pActiveNamespaceList[i];
		struct nvme_namespace_desc* pNamespaceDesc = (struct nvme_namespace_desc*)0x0;
		if (nvme_namespace_init(pDriveDesc, namespaceId, &pNamespaceDesc)!=0){
			printf("failed to initialize namspace\r\n");
			nvme_drive_deinit(pDriveDesc);	
			return -1;
		}
	}
	pDriveDesc->pControllerInfo = pControllerInfo;	
	virtualFree((uint64_t)pActiveNamespaceList, 0);
	memset((void*)&allocQueuePacket, 0, sizeof(struct nvme_alloc_queue_packet));
	allocQueuePacket.queueType = NVME_QUEUE_TYPE_IO_CQ;	
	allocQueuePacket.io_cq.maxEntryCount = NVME_DEFAULT_CQE_COUNT;
	struct nvme_completion_queue_desc* pCompletionQueueDesc = (struct nvme_completion_queue_desc*)0x0;
	if (nvme_alloc_io_completion_queue(pDriveDesc, &allocQueuePacket, &completionQeDesc)!=0){
		printf("failed to allocate I/O completion queue\r\n");
		nvme_drive_deinit(pDriveDesc);
		return -1;
	}
	if (completionQeDesc.completionQe.status.status_code!=NVME_STATUS_CODE_SUCCESS){
		const unsigned char* statusCodeName = "Unknown status code";
		const unsigned char* statusCodeTypeName = "Unknown status code type";
		nvme_get_status_code_name(pCompletionQe->status.status_code, &statusCodeName);
		nvme_get_status_code_type_name(pCompletionQe->status.status_code_type, &statusCodeTypeName);
		printf("failed to allocate I/O completion queue\r\n");
		printf("status code: %s\r\n", statusCodeName);
		printf("status code type: %s\r\n", statusCodeTypeName);	
		nvme_drive_deinit(pDriveDesc);
		return -1;
	}
	pCompletionQueueDesc = allocQueuePacket.io_cq.pCompletionQueueDesc;
	memset((void*)&allocQueuePacket, 0, sizeof(struct nvme_alloc_queue_packet));
	allocQueuePacket.queueType = NVME_QUEUE_TYPE_IO_SQ;
	allocQueuePacket.io_sq.maxEntryCount = NVME_DEFAULT_SQE_COUNT;	
	allocQueuePacket.io_sq.pCompletionQueueDesc = pCompletionQueueDesc;	
	if (nvme_alloc_io_submission_queue(pDriveDesc, &allocQueuePacket, &completionQeDesc)!=0){
		printf("failed to allocate I/O submission queue\r\n");
		nvme_drive_deinit(pDriveDesc);
		return -1;
	}	
	if (completionQeDesc.completionQe.status.status_code!=NVME_STATUS_CODE_SUCCESS){
		const unsigned char* statusCodeName = "Unknown status code";
		const unsigned char* statusCodeTypeName = "Unknown status code type";
		nvme_get_status_code_name(completionQeDesc.completionQe.status.status_code, &statusCodeName);
		nvme_get_status_code_type_name(completionQeDesc.completionQe.status.status_code_type, &statusCodeTypeName);
		printf("failed to allocate I/O submission queue\r\n");
		printf("status code: %s\r\n", statusCodeName);
		printf("status code type: %s\r\n", statusCodeTypeName);
		nvme_drive_deinit(pDriveDesc);	
		return -1;
	}
	struct nvme_namespace_desc* pCurrentNamespace = pDriveDesc->pFirstActiveNamespace;	
	for (uint64_t i = 0;i<pDriveDesc->activeNamespaceCount&&pCurrentNamespace;i++){
		uint64_t driveSubsystemId = 0;
		struct pcie_location bootDriveLocation = *(struct pcie_location*)&pbootargs->driveInfo.extra;
		uint8_t isBootDrive = pbootargs->driveInfo.driveType==DRIVE_TYPE_NVME&&*(uint32_t*)&location==*(uint32_t*)&bootDriveLocation&&pbootargs->driveInfo.port==pCurrentNamespace->namespaceId;
		if ((isBootDrive ? (drive_register_boot_drive(driverInfo.driveDriverId, pCurrentNamespace->namespaceId)) : (drive_register(driverInfo.driveDriverId, pCurrentNamespace->namespaceId, &driveSubsystemId)))!=0){
			printf("failed to register namespace with ID: %d into drive subsystem\r\n", pCurrentNamespace->namespaceId);
			nvme_drive_deinit(pDriveDesc);
			return -1;
		}	
		struct drive_desc* pSubsystemDriveDesc = (struct drive_desc*)0x0;
		if (drive_get_desc(driveSubsystemId, &pSubsystemDriveDesc)!=0){
			printf("failed to get drive descriptor for drive with ID: %d\r\n", driveSubsystemId);
			drive_unregister(driveSubsystemId);	
			nvme_drive_deinit(pDriveDesc);
			return -1;
		}	
		pSubsystemDriveDesc->extra = (uint64_t)pDriveDesc;
		pCurrentNamespace->driveSubsystemId = driveSubsystemId;	
		pCurrentNamespace = pCurrentNamespace->pFlink;
	}	
	return 0;
}
int nvme_drive_deinit(struct nvme_drive_desc* pDriveDesc){
	if (!pDriveDesc)
		return -1;
	struct nvme_namespace_desc* pCurrentNamespace = pDriveDesc->pFirstActiveNamespace;
	for (uint64_t i = 0;i<pDriveDesc->activeNamespaceCount&&pCurrentNamespace;i++){
		drive_unregister(pCurrentNamespace->driveSubsystemId);	
		pCurrentNamespace = pCurrentNamespace->pFlink;
	}	
	for (uint64_t i = 1;i<pDriveDesc->submissionQueueCount;i++){
		struct nvme_submission_queue_desc* pSubmissionQueueDesc = &pDriveDesc->submissionQueueList[i];
		if (!pSubmissionQueueDesc->pSubmissionQueue)
			break;
		struct nvme_free_queue_packet packet = {0};
		memset((void*)&packet, 0, sizeof(struct nvme_free_queue_packet));
		packet.queueType = NVME_QUEUE_TYPE_IO_SQ;	
		packet.io_sq.pSubmissionQueueDesc = pSubmissionQueueDesc;
		nvme_free_io_submission_queue(&packet);
	}
	for (uint64_t i = 1;i<pDriveDesc->completionQueueCount;i++){
		struct nvme_completion_queue_desc* pCompletionQueueDesc = &pDriveDesc->completionQueueList[i];
		if (!pCompletionQueueDesc->pCompletionQueue)
			break;
		struct nvme_free_queue_packet packet = {0};
		memset((void*)&packet, 0, sizeof(struct nvme_free_queue_packet));
		packet.queueType = NVME_QUEUE_TYPE_IO_CQ;
		packet.io_cq.pCompletionQueueDesc = pCompletionQueueDesc;
		nvme_free_io_completion_queue(&packet);
	}
	struct nvme_free_queue_packet freePacket = {0};
	memset((void*)&freePacket, 0, sizeof(struct nvme_free_queue_packet));
	freePacket.queueType = NVME_QUEUE_TYPE_ADMIN_SQ;
	freePacket.admin_sq.pSubmissionQueueDesc = &pDriveDesc->submissionQueueList[0];
	nvme_free_admin_submission_queue(&freePacket);	
	memset((void*)&freePacket, 0, sizeof(struct nvme_free_queue_packet));
	freePacket.queueType = NVME_QUEUE_TYPE_ADMIN_CQ;
	freePacket.admin_cq.pCompletionQueueDesc = &pDriveDesc->completionQueueList[0];
	nvme_free_admin_completion_queue(&freePacket);
	if (pDriveDesc->pActiveNamespaceDescList)
		virtualFreePage((uint64_t)pDriveDesc->pActiveNamespaceDescList, 0);
	if (pDriveDesc->pControllerInfo)
		virtualFreePage((uint64_t)pDriveDesc->pControllerInfo, 0);
	kfree((void*)pDriveDesc);
	return 0;
}
int nvme_pcie_subsystem_function_register(struct pcie_location location){
	if (nvme_drive_init(location)!=0)
		return -1;
	return 0;
}
int nvme_pcie_subsystem_function_unregister(struct pcie_location location){

	return 0;
}
int nvme_drive_subsystem_get_drive_info(uint64_t drive_id, struct drive_info* pDriveInfo){
	if (!pDriveInfo)
		return -1;
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (drive_get_desc(drive_id, &pDriveDesc)!=0){
		printf("failed to get drive descriptor for drive with ID: %d\r\n", drive_id);
		return -1;
	}	
	struct nvme_drive_desc* pNvmeDriveDesc = (struct nvme_drive_desc*)pDriveDesc->extra;
	if (!pNvmeDriveDesc){
		printf("no NVMe drive descriptor attached to drive with ID: %d\r\n", drive_id);
		return -1;
	}	
	struct nvme_namespace_desc* pNamespaceDesc = pNvmeDriveDesc->pActiveNamespaceDescList+pDriveDesc->port;
	struct drive_info driveInfo = {0};
	memset((void*)&driveInfo, 0, sizeof(struct drive_info));
	driveInfo.driveType = DRIVE_TYPE_NVME;
	driveInfo.sectorCount = pNamespaceDesc->lbaCount;
	driveInfo.sectorSize = pNamespaceDesc->sectorSize;
	*pDriveInfo = driveInfo;	
	return 0;
}
int nvme_drive_subsystem_read(uint64_t drive_id, uint64_t lba, uint64_t sectorCount, unsigned char* pBuffer){
	if (!pBuffer||!sectorCount)
		return -1;
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (drive_get_desc(drive_id, &pDriveDesc)!=0){
		printf("failed to get drive subsystem descriptor for drive with ID: %d\r\n", drive_id);
		return -1;
	}
	struct nvme_drive_desc* pNvmeDriveDesc = (struct nvme_drive_desc*)pDriveDesc->extra;
	if (!pNvmeDriveDesc){
		printf("no NVMe drive descriptor attached to drive with ID: %d\r\n", drive_id);
		return -1;
	}	
	struct nvme_namespace_desc* pNamespaceDesc = pNvmeDriveDesc->pActiveNamespaceDescList+pDriveDesc->port;
	struct nvme_completion_qe_desc completionQeDesc = {0};
	struct nvme_completion_qe* pCompletionQe = &completionQeDesc.completionQe;	
	if (nvme_read(pNvmeDriveDesc, pDriveDesc->port, lba, sectorCount, pBuffer, &completionQeDesc)!=0){
		printf("failed to read drive sectors\r\n");
		return -1;
	}	
	if (pCompletionQe->status.status_code!=NVME_STATUS_CODE_SUCCESS){
		const unsigned char* statusCodeName = "Unknown status code";
		const unsigned char* statusCodeTypeName = "Unknown status code type";
		nvme_get_status_code_name(pCompletionQe->status.status_code, &statusCodeName);
		nvme_get_status_code_type_name(pCompletionQe->status.status_code_type, &statusCodeTypeName);
		printf("failed to read drive sectors\r\n");
		printf("status code: %s\r\n", statusCodeName);
		printf("status code type: %s\r\n", statusCodeTypeName);	
		return -1;
	}	
	return 0;
}
int nvme_drive_subsystem_write(uint64_t drive_id, uint64_t lba, uint64_t sectorCount, unsigned char* pBuffer){
	if (!pBuffer||!sectorCount)
		return -1;
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (drive_get_desc(drive_id, &pDriveDesc)!=0){
		printf("failed to get drive subsystem descriptor for drive with ID: %d\r\n", drive_id);
		return -1;
	}	
	struct nvme_drive_desc* pNvmeDriveDesc = (struct nvme_drive_desc*)pDriveDesc->extra;
	if (!pNvmeDriveDesc){
		printf("no NVMe drive descriptor attached to drive with ID: %d\r\n", drive_id);
		return -1;
	}
	struct nvme_namespace_desc* pNamespaceDesc = pNvmeDriveDesc->pActiveNamespaceDescList+pDriveDesc->port;
	struct nvme_completion_qe_desc completionQeDesc = {0};
	struct nvme_completion_qe* pCompletionQe = &completionQeDesc.completionQe;
	if (nvme_write(pNvmeDriveDesc, pDriveDesc->port, lba, sectorCount, pBuffer, &completionQeDesc)!=0){
		printf("failed to write drive sectors\r\n");
		return -1;
	}	
	if (pCompletionQe->status.status_code!=NVME_STATUS_CODE_SUCCESS){
		const unsigned char* statusCodeName = "Unknown status code";
		const unsigned char* statusCodeTypeName = "Unknown status code type";
		nvme_get_status_code_name(pCompletionQe->status.status_code, &statusCodeName);
		nvme_get_status_code_type_name(pCompletionQe->status.status_code_type, &statusCodeTypeName);
		printf("failed to write drive sectors\r\n");
		printf("status code: %s\r\n", statusCodeName);
		printf("status code type: %s\r\n", statusCodeTypeName);	
		return -1;
	}
	return 0;
}
int nvme_drive_subsystem_register_drive(uint64_t drive_id){

	return 0;
}
int nvme_drive_subsystem_unregister_drive(uint64_t drive_id){

	return 0;
}
