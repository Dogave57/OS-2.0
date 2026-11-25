#ifndef _EXFAT
#define _EXFAT
#include <stdint.h>
#define EXFAT_SIGNATURE ((uint64_t)0x2020205441465845)
#define EXFAT_BOOT_REGION_SECTORS 12
#define EXFAT_BOOT_REGION_SECTOR 0
#define EXFAT_BACKUP_BOOT_REGION_SECTOR 12
#define EXFAT_ENTRY_TYPE_FILE 0x85
#define EXFAT_ENTRY_TYPE_STREAM_EXTENSION 0xC0
#define EXFAT_ENTRY_TYPE_FILENAME 0xC1
struct exfat_bootsector{
	uint8_t jmp[3];
	uint64_t signature;
	uint8_t reserved0[53];
	uint64_t volumeOffset;
	uint64_t volumeSectors;
	uint32_t fatOffset;
	uint32_t sectorsPerFat;
	uint32_t clusterHeapOffset;
	uint32_t clusterCount;
	uint32_t rootDirCluster;
	uint32_t volumeSerial;
	uint16_t version;
	uint16_t volumeFlags;
	uint8_t bytesPerSectorShift;
	uint8_t sectorsPerClusterShift;
	uint8_t fatCount;
	uint8_t driveNumber;
	uint8_t percentInUse;
	uint8_t reserved1[7];
	uint8_t bootCode[390];
	uint16_t bootSignature;
}__attribute__((packed));
struct exfat_cache_info{
	struct exfat_bootsector bootSector;
	struct exfat_bootsector backupBootSector;
	unsigned char* clusterCache;
	uint64_t clusterCache_start;
	uint64_t clusterCache_end;
	uint8_t bootSectorCached;
	uint8_t backupBootSectorCached;
};
struct exfat_mount_handle{
	uint64_t drive_id;
	uint64_t partition_id;
	struct exfat_cache_info cacheInfo;
};
struct exfat_file_handle{
	struct exfat_mount_handle* pMountHandle;
};
struct exfat_file_entry{
	uint8_t entryType;
	uint8_t entryCount;
	uint16_t checksum;
	uint16_t flags;
	uint16_t reserved0;
	uint32_t creation_time;
	uint32_t modification_time;
	uint32_t access_time;
	uint8_t padding0[12];
}__attribute__((packed));
struct exfat_stream_file_entry{
	uint8_t entryType;
	uint8_t flags;
	uint8_t reserved0;
	uint8_t fileNameLen;
	uint16_t fileNameHash;
	uint16_t reserved1;
	uint64_t valid_fileSize;
	uint32_t reserved2;
	uint32_t startCluster;
	uint64_t fileSize;
}__attribute__((packed));
struct exfat_file_name_entry{
	uint8_t entryType;
	uint8_t entryCount;
	uint16_t name[15];
}__attribute__((packed));
struct exfat_file_location{
	
};
int exfat_mount(uint64_t drive_id, uint64_t partition_id, struct exfat_mount_handle** ppHandle);
int exfat_unmount(struct exfat_mount_handle* pHandle);
int exfat_open_file(struct exfat_mount_handle* pHandle, uint16_t* filename, struct exfat_file_handle** ppFileHandle);
int exfat_close_file(struct exfat_file_handle* pFileHandle);
int exfat_read_cluster(struct exfat_mount_handle* pHandle, uint64_t cluster, uint8_t* pValue);
int exfat_write_cluster(struct exfat_mount_handle* pHandle, uint64_t cluster, uint8_t value);
int exfat_read_cluster_data(struct exfat_mount_handle* pHandle, uint64_t cluster, unsigned char* pClusterData);
int exfat_write_cluster_data(struct exfat_mount_handle* pHandle, uint64_t cluster, unsigned char* pClusterData);
int exfat_get_bytes_per_cluster(struct exfat_mount_handle* pHandle, uint64_t* pBytesPerCluster);
int exfat_get_first_data_sector(struct exfat_mount_handle* pHandle, uint64_t* pFirstDataSector);
int exfat_get_fat_sector(struct exfat_mount_handle* pHandle, uint64_t* pFatSector);
int exfat_get_backup_fat_sector(struct exfat_mount_handle* pHandle, uint64_t* pFatSector);
int exfat_cache_clusters(struct exfat_mount_handle* pHandle, unsigned char* pClusterData, uint64_t cache_start, uint64_t cache_end);
int exfat_verify(struct exfat_mount_handle* pHandle);
int exfat_boot_region_validate(struct exfat_mount_handle* pHandle);
int exfat_get_boot_region_checksum(unsigned char* pBootRegion, uint32_t* pChecksum);
int exfat_get_bootsector(struct exfat_mount_handle* pHandle, struct exfat_bootsector** ppBootSector);
#endif 
