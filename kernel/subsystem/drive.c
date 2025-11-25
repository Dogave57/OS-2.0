#include "subsystem/subsystem.h"
#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "drivers/gpt.h"
#include "subsystem/drive.h"
struct subsystem_desc* pDriveSubsystem = (struct subsystem_desc*)0x0;
int drive_subsystem_init(void){
	if (subsystem_init(&pDriveSubsystem, 512)!=0){
		printf(L"failed to initialize drive subsystem\r\n");
		return -1;
	}
	if (!pDriveSubsystem){
		printf(L"failed to get subsystem\r\n");
		return -1;
	}
	uint64_t id = 0;
	if (drive_ahci_register(0, &id)!=0)
		return -1;
	struct drive_info driveInfo = {0};
	if (drive_get_info(id, &driveInfo)!=0){
		printf(L"failed to get drive info\r\n");
		return -1;
	}
	printf(L"drive size: %dMB\r\n", (driveInfo.sector_count*512)/MEM_MB);
	printf(L"boot drive id: %d\r\n", id);
	return 0;
}
int drive_ahci_register(uint8_t port, uint64_t* pDriveId){
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)0x0;
	uint64_t id = 0;
	if (subsystem_alloc_entry(pDriveSubsystem, sizeof(struct drive_dev_ahci), &id)!=0)
		return -1;
	if (subsystem_get_entry(pDriveSubsystem, id, (uint64_t*)&pDev)!=0)
		return -1;
	if (!pDev){
		printf(L"failed to allocate entry\r\n");
		return -1;
	}
	pDev->hdr.type = DRIVE_TYPE_AHCI;
	pDev->hdr.sectors_read = (uint64_t)ahci_subsystem_read;
	pDev->hdr.sectors_write = (uint64_t)ahci_subsystem_write;
	pDev->hdr.getDriveInfo = (uint64_t)ahci_subsystem_get_drive_info;
	if (ahci_get_drive_info(port, &pDev->driveInfo)!=0){
		printf(L"failed to get drive info\r\n");
		return -1;
	}
	*pDriveId = id;	
	return 0;
}
int drive_unregister(uint64_t drive_id){
	if (subsystem_free_entry(pDriveSubsystem, drive_id)!=0)
		return -1;
	return 0;
}
int drive_get_info(uint64_t drive_id, struct drive_info* pDriveInfo){
	if (!pDriveInfo)
		return -1;
	struct drive_dev_hdr* pHdr = (struct drive_dev_hdr*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pHdr)!=0)
		return -1;
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)pHdr;
	driveGetInfoFunc getDriveInfo = (driveGetInfoFunc)pHdr->getDriveInfo;
	return getDriveInfo(pHdr, pDriveInfo);
}
int drive_read_sectors(uint64_t drive_id, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	struct drive_dev_hdr* pHdr = (struct drive_dev_hdr*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pHdr)!=0)
		return -1;
	driveSectorReadFunc sectors_read = (driveSectorReadFunc)pHdr->sectors_read;
	return sectors_read(pHdr, lba, sector_count, pBuffer);
}
int drive_write_sectors(uint64_t drive_id, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	struct drive_dev_hdr* pHdr = (struct drive_dev_hdr*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pHdr)!=0){
		return -1;
	}
	if (!pHdr)
		return -1;
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)pHdr;
	driveSectorWriteFunc sectors_write = (driveSectorWriteFunc)pHdr->sectors_write;
	return sectors_write(pHdr, lba, sector_count, pBuffer);
}
int partition_read_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	struct gpt_partition partition = {0};
	if (gpt_get_partition(drive_id, partition_id, &partition)!=0)
		return -1;
	uint64_t lba = partition.start_lba+lba_offset;
	if (lba>partition.end_lba)
		return -1;
	if (drive_read_sectors(drive_id, lba, sector_count, pBuffer)!=0)
		return -1;
	return 0;
}
int partition_write_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	struct gpt_partition partition = {0};
	if (gpt_get_partition(drive_id, partition_id, &partition)!=0)
		return -1;
	uint64_t lba = partition.start_lba+lba_offset;
	if (lba>partition.end_lba)
		return -1;
	if (drive_write_sectors(drive_id, lba, sector_count, pBuffer)!=0)
		return -1;
	return 0;
}
