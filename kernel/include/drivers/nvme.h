#ifndef _NVME
#define _NVME
#include <stdint.h>
#include "drivers/pcie.h"
#define MAX_SQ_ENTRIES 64
#define MAX_CQ_ENTRIES 256
struct nvme_queue{
	uint64_t queue;
	uint64_t queueSize;
	uint64_t currentEntry;
};
struct nvme_submission_qe{
	uint8_t opcode;
	uint8_t fused_operation : 2;
	uint8_t reserved0 : 4;
	uint8_t prp_info : 2;
	uint16_t cmd_ident : 16;
	uint32_t nsid;
	uint32_t reserved1[2];
	uint64_t metadata_ptr;
	uint64_t prp0;
	uint64_t prp1;
	uint32_t cmd_specific[6];
}__attribute__((packed));
struct nvme_completion_qe{
	uint64_t cmd_specific;
	uint16_t submission_queue_ident;
	uint16_t cmd_ident;
	uint8_t phase_bit : 1;
	uint16_t status : 15;
	uint16_t reserved0;
}__attribute__((packed));
struct nvme_base_registers{
	uint64_t capabilities;
	uint32_t version;
	uint32_t interrupt_mask;
	uint32_t interrupt_mask_clear;
	uint32_t controller_config;
	uint32_t controller_status;
	uint32_t admin_queue_attribs;
	uint64_t admin_submission_queue;
	uint64_t admin_completion_queue;
}__attribute__((packed));
struct nvme_controller_ident{
	uint16_t vendorId;
	uint16_t ssVendorId;
	unsigned char serial[20];
	unsigned char model[40];
	unsigned char firmware_version[8];
	uint8_t rab;
	uint8_t ieee[3];
	uint8_t multiInterfaceCapabilities;
	uint8_t maxDataTransferSize;
	uint16_t controllerId;
	uint32_t nameSpaceCount;
	uint16_t oncs;
	uint16_t fuses;
	uint8_t fna;
	uint8_t volatileWriteCache;
	uint16_t atomicWriteUnitNormal;
	uint16_t atomicWriteUnitPowerFail;
	uint8_t nvscc;
}__attribute__((packed));
struct nvme_driveinfo{
	struct pcie_location location;
	uint64_t mmio_base;
	uint64_t driveSectors;
};
int nvme_init(void);
int nvme_drive_exists(struct pcie_location location);
int nvme_write_dword(uint64_t mmio_base, uint64_t reg, uint32_t value);
int nvme_read_dword(uint64_t mmio_base, uint64_t, uint32_t* pValue);
int nvme_write_qword(uint64_t mmio_base, uint64_t reg, uint64_t value);
int nvme_read_qword(uint64_t mmio_base, uint64_t reg, uint64_t* pValue);
int nvme_push_sq(struct nvme_queue* pQueue, struct nvme_submission_qe** ppCmdEntry);
int nvme_pop_sq(struct nvme_queue* pQueue);
int nvme_push_cq(struct nvme_queue* pQueue, struct nvme_completion_qe** ppCompletionEntry);
int nvme_pop_cq(struct nvme_queue* pQueue);
int nvme_send_admin_cmd(uint64_t mmio_base, struct nvme_submission_qe cmdEntry);
int nvme_get_drive_info(struct pcie_location location, struct nvme_driveinfo* pInfo);
int nvme_init_drive(struct nvme_driveinfo driveInfo);
int nvme_get_controller_ident(struct nvme_driveinfo driveInfo, struct nvme_controller_ident* pIdent);
int nvme_read_sectors(struct nvme_driveinfo driveInfo, uint64_t sector, uint64_t sectorCnt, uint64_t* pBuffer);
int nvme_write_sectors(struct nvme_driveinfo driveInfo, uint64_t sector, uint64_t sectorCnt, uint64_t* pBuffer);
#endif
