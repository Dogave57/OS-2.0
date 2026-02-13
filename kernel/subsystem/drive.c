#include "subsystem/subsystem.h"
#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "drivers/gpt.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "panic.h"
#include "subsystem/drive.h"
static struct subsystem_desc* pDriveSubsystem = (struct subsystem_desc*)0x0;
static struct subsystem_desc* pDriveDriverSubsystem = (struct subsystem_desc*)0x0;
int drive_subsystem_init(void){
	if (subsystem_init(&pDriveSubsystem, 8192)!=0){
		printf("failed to initialize drive subsystem\r\n");
		return -1;
	}
	if (subsystem_init(&pDriveDriverSubsystem, 8192)!=0){
		printf("failed to initialize drive driver subsystem\r\n");
		return -1;
	}	
	if (pbootargs->driveInfo.driveType==DRIVE_TYPE_INVALID){
		panic("unsupported boot device\r\n");
		while (1){};
	}
	uint64_t driveId = 0;
	if (drive_register(0xFFFFFFFFFFFFFFFF, 0xFF, &driveId)!=0){
		printf("failed to register boot drive placeholder\r\n");
		return -1;
	}
	return 0;
}
KAPI int drive_register_boot_drive(uint64_t driverId, uint8_t port){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	if (drive_unregister(0x0)!=0){
		printf("failed to unregister boot drive\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	uint64_t driveId = 0;
	if (drive_register(driverId, port, &driveId)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int drive_driver_register(struct drive_driver_vtable vtable, uint64_t* pDriverId){
	if (!pDriverId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)kmalloc(sizeof(struct drive_driver_desc));
	if (!pDriverDesc){
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pDriverDesc, 0, sizeof(struct drive_driver_desc));
	pDriverDesc->vtable = vtable;
	uint64_t driverId = 0;
	if (subsystem_alloc_entry(pDriveDriverSubsystem, (unsigned char*)pDriverDesc, &driverId)!=0){
		printf("failed to allocate driver descriptor subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	*pDriverId = driverId;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int drive_driver_unregister(uint64_t driverId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);	
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)0x0;
	if (subsystem_get_entry(pDriveDriverSubsystem, driverId, (uint64_t*)&pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;	
	}
	kfree((void*)pDriverDesc);
	if (subsystem_free_entry(pDriveDriverSubsystem, driverId)!=0){
		mutex_unlock(&mutex);
		return -1;	
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int drive_driver_get_desc(uint64_t driverId, struct drive_driver_desc** ppDriverDesc){
	if (!ppDriverDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)0x0;
	if (subsystem_get_entry(pDriveDriverSubsystem, driverId, (uint64_t*)&pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;	
	}
	*ppDriverDesc = pDriverDesc;
	mutex_unlock(&mutex);	
	return 0;
}
KAPI int drive_register(uint64_t driverId, uint8_t port, uint64_t* pDriveId){
	if (!pDriveId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct drive_desc* pDriveDesc = (struct drive_desc*)kmalloc(sizeof(struct drive_desc));
	if (!pDriveDesc){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	memset((void*)pDriveDesc, 0, sizeof(struct drive_desc));
	pDriveDesc->driverId = driverId;
	pDriveDesc->port = port;
	uint64_t driveId = 0;
	if (subsystem_alloc_entry(pDriveSubsystem, (unsigned char*)pDriveDesc, &driveId)!=0){
		kfree((void*)pDriveDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	pDriveDesc->driveId = driveId;
	if (driverId==0xFFFFFFFFFFFFFFFF){
		mutex_unlock_isr_safe(&mutex);
		return 0;
	}
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)0x0;
	if (drive_driver_get_desc(driverId, &pDriverDesc)!=0){
		kfree((void*)pDriveDesc);
		subsystem_free_entry(pDriveSubsystem, driveId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (!pDriverDesc->vtable.driveRegister){
		printf("invalid drive register routine\r\n");
		kfree((void*)pDriveDesc);
		subsystem_free_entry(pDriveSubsystem, driveId);
		mutex_unlock_isr_safe(&mutex);
		return -1;	
	}
	if (pDriverDesc->vtable.driveRegister(driveId)!=0){
		kfree((void*)pDriveDesc);
		subsystem_free_entry(pDriveSubsystem, driveId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	*pDriveId = driveId;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int drive_unregister(uint64_t driveId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	if (!driveId){
		if (subsystem_free_entry(pDriveSubsystem, driveId)!=0){
			mutex_unlock_isr_safe(&mutex);
			return -1;
		}
		mutex_unlock_isr_safe(&mutex);
		return 0;
	}	
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, driveId, (uint64_t*)&pDriveDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)0x0;
	if (drive_driver_get_desc(pDriveDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.driveUnregister(driveId)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	kfree((void*)pDriveDesc);
	if (subsystem_free_entry(pDriveSubsystem, driveId)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int drive_get_desc(uint64_t driveId, struct drive_desc** ppDriveDesc){
	if (!ppDriveDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, driveId, (uint64_t*)&pDriveDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	*ppDriveDesc = pDriveDesc;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int drive_get_info(uint64_t drive_id, struct drive_info* pDriveInfo){
	if (!pDriveInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pDriveDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (pDriveDesc->driveInfo.sectorCount){
		*pDriveInfo = pDriveDesc->driveInfo;
		mutex_unlock_isr_safe(&mutex);
		return 0;
	}	
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)0x0;
	if (subsystem_get_entry(pDriveDriverSubsystem, pDriveDesc->driverId, (uint64_t*)&pDriverDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct drive_info driveInfo = {0};
	if (pDriverDesc->vtable.getDriveInfo(drive_id, &driveInfo)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	pDriveDesc->driveInfo = driveInfo;
	*pDriveInfo = driveInfo;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int drive_read_sectors(uint64_t drive_id, uint64_t lba, uint64_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pDriveDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)0x0;
	if (subsystem_get_entry(pDriveDriverSubsystem, pDriveDesc->driverId, (uint64_t*)&pDriverDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.readSectors(drive_id, lba, sector_count, pBuffer)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int drive_write_sectors(uint64_t drive_id, uint64_t lba, uint64_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (subsystem_get_entry(pDriveSubsystem, drive_id, (uint64_t*)&pDriveDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct drive_driver_desc* pDriverDesc = (struct drive_driver_desc*)0x0;
	if (subsystem_get_entry(pDriveDriverSubsystem, pDriveDesc->driverId, (uint64_t*)&pDriverDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (pDriverDesc->vtable.writeSectors(drive_id, lba, sector_count, pBuffer)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int partition_read_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct gpt_partition partition = {0};
	if (gpt_get_partition(drive_id, partition_id, &partition)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	uint64_t lba = partition.start_lba+lba_offset;
	if (lba>partition.end_lba){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (drive_read_sectors(drive_id, lba, sector_count, pBuffer)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int partition_write_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct gpt_partition partition = {0};
	if (gpt_get_partition(drive_id, partition_id, &partition)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	uint64_t lba = partition.start_lba+lba_offset;
	if (lba>partition.end_lba){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (drive_write_sectors(drive_id, lba, sector_count, pBuffer)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
