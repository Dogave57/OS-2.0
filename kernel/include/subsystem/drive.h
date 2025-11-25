#ifndef _DRIVE_SUBSYSTEM
#define _DRIVE_SUBSYSTEM
#include <stdint.h>
struct ahci_drive_info{
	uint8_t port;
	uint64_t sector_count;
};
#define DRIVE_TYPE_INVALID (0)
#define DRIVE_TYPE_AHCI (1)
#define DRIVE_SECTOR_SIZE (512)
struct drive_dev_hdr{
	uint8_t type;
	uint64_t sectors_read;
	uint64_t sectors_write;
	uint64_t getDriveInfo;
};
struct drive_dev_ahci{
	struct drive_dev_hdr hdr;
	struct ahci_drive_info driveInfo;
};
struct drive_info{
	uint8_t driveType;
	uint64_t sector_count;
};
int ahci_subsystem_read(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int ahci_subsystem_write(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int ahci_subsystem_get_drive_info(struct drive_dev_hdr* pHdr, struct drive_info* pDriveInfo);
int ahci_get_drive_info(uint8_t port, struct ahci_drive_info* pDriveInfo);
typedef int(*driveSectorReadFunc)(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
typedef int(*driveSectorWriteFunc)(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
typedef int(*driveGetInfoFunc)(struct drive_dev_hdr* pHdr, struct drive_info* pDriveInfo);
int drive_subsystem_init(void);
int drive_ahci_register(uint8_t port, uint64_t* pDriveId);
int drive_get_info(uint64_t drive_id, struct drive_info* pDriveInfo);
int drive_read_sectors(uint64_t drive_id, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int drive_write_sectors(uint64_t drive_id, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int partition_read_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer);
int partition_write_sectors(uint64_t drive_id, uint64_t partition_id, uint64_t lba_offset, uint16_t sector_count, unsigned char* pBuffer);
#endif
