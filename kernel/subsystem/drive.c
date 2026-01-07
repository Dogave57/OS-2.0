#include "subsystem/subsystem.h"
#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "drivers/gpt.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "panic.h"
#include "subsystem/drive.h"
struct subsystem_desc* pDriveSubsystem = (struct subsystem_desc*)0x0;
int drive_subsystem_init(void){
	if (subsystem_init(&pDriveSubsystem, MEM_KB*64)!=0){
		printf("failed to initialize drive subsystem\r\n");
		return -1;
	}
	if (!pDriveSubsystem){
		printf("failed to get subsystem\r\n");
		return -1;
	}
	uint64_t id = 0;
	if (pbootargs->driveInfo.driveType==DRIVE_TYPE_INVALID){
		panic("unsupported boot device\r\n");
		while (1){};
	}
	if (pbootargs->driveInfo.driveType==DRIVE_TYPE_AHCI){
		if (drive_ahci_register(pbootargs->driveInfo.port, &id)!=0){
			printf("failed to register boot device in drive subsystem\r\n");
			return -1;
		}
	}
	struct drive_info driveInfo = {0};
	if (drive_get_info(id, &driveInfo)!=0){
		printf("failed to get drive info\r\n");
		return -1;
	}
	printf("drive size: %dMB\r\n", (driveInfo.sector_count*512)/MEM_MB);
	printf("boot drive id: %d\r\n", id);
	return 0;
}
int drive_ahci_register(uint8_t port, uint64_t* pDriveId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t id = 0;
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)kmalloc(sizeof(struct drive_dev_ahci));
	if (!pDev){
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_alloc_entry(pDriveSubsystem, (unsigned char*)pDev, &id)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	pDev->hdr.type = DRIVE_TYPE_AHCI;
	pDev->hdr.sectors_read = (uint64_t)ahci_subsystem_read;
	pDev->hdr.sectors_write = (uint64_t)ahci_subsystem_write;
	pDev->hdr.getDriveInfo = (uint64_t)ahci_subsystem_get_drive_info;
	if (ahci_get_drive_info(port, &pDev->driveInfo)!=0){
		printf("failed to get drive info\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	*pDriveId = id;	
	mutex_unlock(&mutex);
	return 0;
}
int drive_unregister(uint64_t drive_id){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint64_t pEntry = 0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, &pEntry)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	kfree((void*)pEntry);
	if (subsystem_free_entry(pDriveSubsystem, drive_id)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int drive_get_info(uint64_t drive_id, struct drive_info* pDriveInfo){
	if (!pDriveInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct drive_dev_hdr* pHdr = (struct drive_dev_hdr*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pHdr)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)pHdr;
	driveGetInfoFunc getDriveInfo = (driveGetInfoFunc)pHdr->getDriveInfo;
	if (getDriveInfo(pHdr, pDriveInfo)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int drive_read_sectors(uint64_t drive_id, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct drive_dev_hdr* pHdr = (struct drive_dev_hdr*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pHdr)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	driveSectorReadFunc sectors_read = (driveSectorReadFunc)pHdr->sectors_read;
	if (sectors_read(pHdr, lba, sector_count, pBuffer)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int drive_write_sectors(uint64_t drive_id, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct drive_dev_hdr* pHdr = (struct drive_dev_hdr*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pHdr)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (!pHdr){
		mutex_unlock(&mutex);
		return -1;
	}
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)pHdr;
	driveSectorWriteFunc sectors_write = (driveSectorWriteFunc)pHdr->sectors_write;
	if (sectors_write(pHdr, lba, sector_count, pBuffer)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int partition_read_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpt_partition partition = {0};
	if (gpt_get_partition(drive_id, partition_id, &partition)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t lba = partition.start_lba+lba_offset;
	if (lba>partition.end_lba){
		mutex_unlock(&mutex);
		return -1;
	}
	if (drive_read_sectors(drive_id, lba, sector_count, pBuffer)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int partition_write_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpt_partition partition = {0};
	if (gpt_get_partition(drive_id, partition_id, &partition)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t lba = partition.start_lba+lba_offset;
	if (lba>partition.end_lba){
		mutex_unlock(&mutex);
		return -1;
	}
	if (drive_write_sectors(drive_id, lba, sector_count, pBuffer)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
