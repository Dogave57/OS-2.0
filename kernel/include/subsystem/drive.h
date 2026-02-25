#ifndef _DRIVE_SUBSYSTEM
#define _DRIVE_SUBSYSTEM
#include <stdint.h>
#include "kernel_include.h"
#define DRIVE_TYPE_INVALID (0x00)
#define DRIVE_TYPE_SATA (0x01)
#define DRIVE_TYPE_NVME (0x02)
#define DRIVE_TYPE_USB (0x03)
#define DRIVE_TYPE_UNKNOWN (0x04)
#define DRIVE_SECTOR_SIZE (512)
struct drive_info;
typedef int(*driveGetInfoFunc)(uint64_t drive_id, struct drive_info* pDriveInfo);
typedef int(*driveReadSectorsFunc)(uint64_t drive_id, uint64_t lba, uint64_t sectorCount, unsigned char* pBuffer);
typedef int(*driveWriteSectorsFunc)(uint64_t drive_id, uint64_t lba, uint64_t sectorCount, unsigned char* pBuffer);
typedef int(*driveRegisterFunc)(uint64_t drive_id);
typedef int(*driveUnregisterFunc)(uint64_t drive_id);
struct drive_info{
	uint8_t driveType;
	uint64_t sectorCount;
	uint64_t sectorSize;
};
struct drive_desc{
	uint64_t driveId;
	uint64_t driverId;
	uint64_t port;
	uint64_t extra;	
	struct drive_info driveInfo;
};
struct drive_driver_vtable{
	driveReadSectorsFunc readSectors;
	driveWriteSectorsFunc writeSectors;	
	driveGetInfoFunc getDriveInfo;
	driveRegisterFunc driveRegister;
	driveUnregisterFunc driveUnregister;
};
struct drive_driver_desc{
	struct drive_driver_vtable vtable;
	uint64_t driverId;
};
int drive_subsystem_init(void);
KAPI int drive_register_boot_drive(uint64_t driverId, uint8_t port);
KAPI int drive_driver_register(struct drive_driver_vtable vtable, uint64_t* pDriverId);
KAPI int drive_driver_unregister(uint64_t driverId);
KAPI int drive_driver_get_desc(uint64_t driverId, struct drive_driver_desc** ppDriverDesc);
KAPI int drive_register(uint64_t driverId, uint64_t port, uint64_t* pDriverId);
KAPI int drive_unregister(uint64_t driveId);
KAPI int drive_get_desc(uint64_t driveId, struct drive_desc** ppDriveDesc);
KAPI int drive_get_info(uint64_t drive_id, struct drive_info* pDriveInfo);
KAPI int drive_read_sectors(uint64_t drive_id, uint64_t lba, uint64_t sector_count, unsigned char* pBuffer);
KAPI int drive_write_sectors(uint64_t drive_id, uint64_t lba, uint64_t sector_count, unsigned char* pBuffer);
KAPI int partition_read_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer);
KAPI int partition_write_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer);
#endif
