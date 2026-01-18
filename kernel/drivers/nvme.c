#include <stdint.h>
#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "align.h"
#include "drivers/timer.h"
#include "drivers/pcie.h"
#include "drivers/nvme.h"
struct nvme_queue submissionQueue = {0};
struct nvme_queue completionQueue = {0};
int nvme_init(void){
	if (virtualAllocPage(&submissionQueue.queue, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate submission queue\r\n");
		return -1;
	}
	if (virtualAllocPage(&completionQueue.queue, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate completion queue\r\n");
		return -1;
	}
	memset((void*)submissionQueue.queue, 0, PAGE_SIZE);
	memset((void*)completionQueue.queue, 0, PAGE_SIZE);
	for (uint8_t bus = 0;bus<255;bus++){
	for (uint8_t dev = 0;dev<32;dev++){
		for (uint8_t func = 0;func<8;func++){
			struct pcie_location location = {0};
			location.bus = bus;
			location.dev = dev;
			location.func = func;
			if (nvme_drive_exists(location)!=0)
				continue;
			printf("NVME drive at bus %d, dev %d, func %d\r\n", bus, dev, func);
			struct nvme_driveinfo driveInfo = {0};
			struct nvme_controller_ident controllerIdent = {0};
			if (nvme_get_drive_info(location, &driveInfo)!=0){
				printf("failed to get NVME drive info\r\n");
				continue;
			}
			if (nvme_init_drive(driveInfo)!=0){
				printf("failed to initialize drive\r\n");
				continue;
			}
			if (nvme_get_controller_ident(driveInfo, &controllerIdent)!=0){
				printf("failed to get controller identification\r\n");
				continue;
			}
			printf("drive MMIO base: %p\r\n", (void*)driveInfo.mmio_base);
			printf("drive vendor: 0x%x\r\n", controllerIdent.vendorId);
			for (uint64_t i = 0;i<20;i++){
				printf("%c,", controllerIdent.serial[i]);
			}
			putchar(L'\n');
			continue;
			uint64_t sectorData[64] = {0};
			memset((void*)sectorData, 0, sizeof(sectorData));
			if (nvme_read_sectors(driveInfo, 1, 1, sectorData)!=0){
				printf("failed to read first sector\r\n");
				continue;
			}
			for (uint64_t i = 0;i<sizeof(sectorData)/sizeof(uint64_t);i++){
				printf("{0x%x},", sectorData[i]);
			}
			putchar('\n');
			continue;
		}
	}
	}
	return 0;
}
int nvme_drive_exists(struct pcie_location location){
	if (pcie_device_exists(location)!=0)
		return -1;
	uint8_t class = 0;
	uint8_t subclass = 0;
	if (pcie_get_class(location, &class)!=0)
		return -1;
	if (class!=PCIE_CLASS_DRIVE_CONTROLLER)
		return -1;
	if (pcie_get_subclass(location, &subclass)!=0)
		return -1;
	if (subclass!=PCIE_SUBCLASS_NVME_CONTROLLER)
		return -1;
	return 0;
}
int nvme_write_dword(uint64_t mmio_base, uint64_t reg, uint32_t value){
	if (!mmio_base)
		return -1;
	uint64_t mmio_base_page = align_down(mmio_base+reg, PAGE_SIZE);
	if (virtualMapPage(mmio_base_page, mmio_base_page, PTE_RW, 1, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	*((uint32_t*)(mmio_base+reg)) = value;
	return 0;
}
int nvme_read_dword(uint64_t mmio_base, uint64_t reg, uint32_t* pValue){
	if (!mmio_base||!pValue)
		return -1;
	uint64_t mmio_base_page = align_down(mmio_base+reg, PAGE_SIZE);
	if (virtualMapPage(mmio_base_page, mmio_base_page, PTE_RW, 1, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	*pValue = *((uint32_t*)(mmio_base+reg));
	return 0;
}
int nvme_write_qword(uint64_t mmio_base, uint64_t reg, uint64_t value){
	if (!mmio_base)
		return -1;
	uint64_t mmio_base_page = align_down(mmio_base+reg, PAGE_SIZE);
	if (virtualMapPage(mmio_base_page, mmio_base_page, PTE_RW, 1, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	*((uint64_t*)(mmio_base+reg)) = value;
	return 0;	
}
int nvme_read_qword(uint64_t mmio_base, uint64_t reg, uint64_t* pValue){
	if (!mmio_base||!pValue)
		return -1;
	uint64_t mmio_base_page = align_down(mmio_base+reg, PAGE_SIZE);
	if (virtualMapPage(mmio_base_page, mmio_base_page, PTE_RW, 1, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	*pValue = *((uint64_t*)(mmio_base+reg));
	return 0;
}
int nvme_push_sq(struct nvme_queue* pQueue, struct nvme_submission_qe** ppCmdEntry){
	if (!pQueue)
		return -1;
	if (pQueue->currentEntry>=MAX_SQ_ENTRIES)
		return -1;
	struct nvme_submission_qe* pCmdEntry = ((struct nvme_submission_qe*)pQueue->queue)+pQueue->currentEntry;
	*ppCmdEntry = pCmdEntry;
	pQueue->currentEntry++;
	return 0;
}
int nvme_pop_sq(struct nvme_queue* pQueue){
	if (!pQueue)
		return -1;
	if (!pQueue->currentEntry)
		return -1;
	pQueue->currentEntry--;
	return 0;
}
int nvme_push_cq(struct nvme_queue* pQueue, struct nvme_completion_qe** ppCompletionEntry){
	if (!pQueue)
		return -1;
	if (pQueue->currentEntry>=MAX_SQ_ENTRIES)
		return -1;
	struct nvme_completion_qe* pCompletionEntry = ((struct nvme_completion_qe*)pQueue->queue)+pQueue->currentEntry;
	*ppCompletionEntry = pCompletionEntry;
	pQueue->currentEntry++;
	return 0;
}
int nvme_pop_cq(struct nvme_queue* pQueue){
	if (!pQueue)
		return -1;
	if (!pQueue->currentEntry)
		return -1;
	pQueue->currentEntry--;
	return 0;
}
int nvme_send_admin_cmd(uint64_t mmio_base, struct nvme_submission_qe cmdEntry){
	if (!mmio_base)
		return -1;
	while (1){
		uint32_t status =0;
		nvme_read_dword(mmio_base, 0x1C, &status);
		if (status&(1<<0))
			break;
		continue;
	}
	struct nvme_base_registers* pBase = (struct nvme_base_registers*)mmio_base;
	struct nvme_submission_qe* pSe = (struct nvme_submission_qe*)0x0;
	struct nvme_completion_qe* pCe = (struct nvme_completion_qe*)0x0;
	uint64_t seIndex = submissionQueue.currentEntry;
	uint64_t ceIndex = completionQueue.currentEntry;
	uint64_t cmd_ident = seIndex;
	while (submissionQueue.currentEntry>=MAX_SQ_ENTRIES&&completionQueue.currentEntry>=MAX_CQ_ENTRIES){};
	if (nvme_push_sq(&submissionQueue, &pSe)!=0){
		printf("failed to push submission queue entry\r\n");
		return -1;
	}
	if (nvme_push_cq(&completionQueue, &pCe)!=0){
		printf("failed to push completion queue entry\r\n");
		return -1;
	}
	*pSe = cmdEntry;
	pSe->cmd_ident = cmd_ident;
	pCe->phase_bit = 0;
	uint64_t nvme_cap_strd = (mmio_base>>12)&0xF;
	nvme_write_dword(mmio_base, 0x1000, submissionQueue.currentEntry);
	sleep(100);
	nvme_write_dword(mmio_base, 0x1004, completionQueue.currentEntry);
	while (pCe->phase_bit!=0){};
	sleep(100);
	if (nvme_pop_sq(&submissionQueue)!=0){
		printf("failed to pop submission queue entry\r\n");
		return -1;
	}
	if (nvme_pop_cq(&completionQueue)!=0){
		printf("failed to pop completion queue entry\r\n");
		return -1;
	}
	completionQueue.currentEntry--;
	if (pCe->status!=0)
		return -1;
	return 0;
}
int nvme_get_drive_info(struct pcie_location location, struct nvme_driveinfo* pInfo){
	if (!pInfo)
		return -1;
	if (nvme_drive_exists(location)!=0)
		return -1;
	uint64_t base_low = 0;
	uint64_t base_high = 0;
	if (pcie_get_bar(location, 0, &base_low)!=0){
		printf("failed to get BAR0\r\n");
		return -1;
	}
	if (pcie_get_bar(location, 1, &base_high)!=0){
		printf("failed to get BAR1\r\n");
		return -1;
	}
	uint64_t mmio_base = ((base_high<<32)|(base_low&0xFFFFFFF0));
	pInfo->location = location;
	pInfo->mmio_base = mmio_base;
	if (virtualMapPage(mmio_base, mmio_base, PTE_RW, 1, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	return 0;
}
int nvme_get_controller_ident(struct nvme_driveinfo driveInfo, struct nvme_controller_ident* pIdent){
	if (!pIdent)
		return -1;
	struct nvme_controller_ident* pIdentPage = (struct nvme_controller_ident*)0x0;
	if (virtualAllocPage((uint64_t*)&pIdentPage, PTE_RW, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate drive identification page\r\n");
		return -1;
	}
	uint64_t pIdent_pa = 0;
	if (virtualToPhysical((uint64_t)pIdentPage, &pIdent_pa)!=0){
		printf("failed to get PA of ident page\r\n");
		return -1;
	}
	struct nvme_submission_qe identEntry = {0};
	memset((void*)&identEntry, 0, sizeof(struct nvme_submission_qe));
	identEntry.opcode = 0x06;
	identEntry.nsid = 0;
	identEntry.prp0 = (uint64_t)pIdent_pa;
	identEntry.cmd_specific[0] = 1;
	if (nvme_send_admin_cmd(driveInfo.mmio_base, identEntry)!=0){
		printf("failed to send drive ident cmd\r\n");
		return -1;
	}
	struct nvme_controller_ident ident = *pIdentPage;
	if (virtualFreePage((uint64_t)pIdentPage, 0)!=0){
		printf("failed to free drive ident page\r\n");
		return-1;
	}
	*pIdent = ident;
	return 0;
}
int nvme_init_drive(struct nvme_driveinfo driveInfo){
	uint64_t submissionQueue_pa = 0;
	uint64_t completionQueue_pa = 0;
	uint32_t init_conf = 0;
	struct nvme_base_registers* pBase = (struct nvme_base_registers*)driveInfo.mmio_base;
	if (virtualToPhysical(submissionQueue.queue, &submissionQueue_pa)!=0)
		return -1;
	if (virtualToPhysical(completionQueue.queue, &completionQueue_pa)!=0)
		return -1;
	pBase->controller_status&=~(1<<0);
	pBase->admin_queue_attribs = ((64-1)<<16)|(32-1);
	pBase->admin_submission_queue = submissionQueue_pa;
	pBase->admin_completion_queue = completionQueue_pa;
	pBase->controller_status|=(1<<0);
	printf("version: 0x%x\r\n", pBase->version);
	return 0;
}
int nvme_read_sectors(struct nvme_driveinfo driveInfo, uint64_t sector, uint64_t sectorCnt, uint64_t* pBuffer){
	if (!pBuffer||!sectorCnt)
		return -1;
	uint64_t buffer_pa = 0;
	if (virtualToPhysical((uint64_t)pBuffer, &buffer_pa)!=0)
		return -1;
	return 0;
}
int nvme_write_sectors(struct nvme_driveinfo driveInfo, uint64_t sector, uint64_t sectorCnt, uint64_t* pBuffer){
	if (!pBuffer)
		return -1;
	uint64_t buffer_pa = 0;
	if (virtualToPhysical((uint64_t)pBuffer, &buffer_pa)!=0)
		return -1;
	return 0;
}
