#ifndef _FLUXFS
#define _FLUXFS
#include <stdint.h>
#define FLUXFS_FILE_ATTRIBUTE_DIRECTORY (1<<0)
#define FLUXFS_FILE_ATTRIBUTE_HIDDEN (1<<1)
#define FLUXFS_CLUSTER_EOC 0xFFFFFFFFFFFFFFFF
#define FLUXFS_CLUSTER_FREE 0x0
#define FLUXFS_SIGNATURE 0x2020534658554C46
struct fluxfs_header{
	uint64_t signature;
	uint32_t headerChecksum; //ignore when creating CRC32 checksum
	uint32_t bytesPerCluster;
	uint64_t freeStackSector;
	uint64_t fatSector;
	uint64_t dataSector;
	uint64_t sectorsPerFreeStack;
	uint64_t sectorsPerFat;
	uint64_t rootCluster;
	uint64_t partitionSize;
	uint64_t freeStackIndex; //ignore when creating CRC32 checksum
	unsigned char padding0[432];
}__attribute__((packed));
struct fluxfs_file_entry{
	uint8_t filename[128];
	uint64_t firstDataCluster;
	uint64_t lastDataCluster;
	uint64_t fileSize;
	uint64_t fileAttributes;
	uint32_t checksum;
	uint8_t padding0[92];
}__attribute__((packed));
struct fluxfs_cache_info{
	uint8_t headerCached;
	uint64_t lastCachedSectorGroup;
	uint64_t cachedSectors;
};
struct fluxfs_mount{
	uint64_t drive_id;
	uint64_t partition_id;
	struct fluxfs_cache_info cacheInfo;
	struct fluxfs_header fsHeader;
	unsigned char* pSectorCache;
}__attribute__((packed));
struct fluxfs_file_handle{
	struct fluxfs_mount* pMount;
}__attribute__((packed));
int fluxfs_init(void);
int fluxfs_mount(uint64_t drive_id, uint64_t partition_id, struct fluxfs_mount** ppMount);
int fluxfs_unmount(struct fluxfs_mount* pMount);
int fluxfs_open(struct fluxfs_mount* pMount, unsigned char* filename, struct fluxfs_file_handle** ppFileHandle);
int fluxfs_close(struct fluxfs_file_handle* pFileHandle);
int fluxfs_create(struct fluxfs_mount* pMount, unsigned char* filename, uint64_t fileAttribs);
int fluxfs_delete(struct fluxfs_file_handle* pFileHandle);
int fluxfs_allocate_cluster(struct fluxfs_mount* pMount, uint64_t* pCluster);
int fluxfs_free_cluster(struct fluxfs_mount* pMount, uint64_t cluster);
int fluxfs_read_free_stack(struct fluxfs_mount* pMount, uint64_t entryIndex, uint64_t* pCluster);
int fluxfs_write_free_stack(struct fluxfs_mount* pMount, uint64_t entryIndex, uint64_t cluster);
int fluxfs_read_cluster(struct fluxfs_mount* pMount, uint64_t cluster, uint64_t* pValue);
int fluxfs_write_cluster(struct fluxfs_mount* pMount, uint64_t cluster, uint64_t value);
int fluxfs_verify(struct fluxfs_mount* pMount);
int fluxfs_get_header(struct fluxfs_mount* pMount, struct fluxfs_header* pHeader);
int fluxfs_get_header_checksum(struct fluxfs_header header, uint32_t* pChecksum);
int fluxfs_set_header(struct fluxfs_mount* pMount, struct fluxfs_header header);
int fluxfs_write_header(struct fluxfs_mount* pMount, struct fluxfs_header header);
int fluxfs_format(uint64_t drive_id, uint64_t partition_id);
int fluxfs_subsystem_mount(uint64_t drive_id, uint64_t partition_id, struct fs_mount* pMount);
int fluxfs_subsystem_unmount(struct fs_mount* pMount);
int fluxfs_subsystem_open(struct fs_mount* pMount, unsigned char* filename, void** ppFileHandle);
int fluxfs_subsystem_close(void* pHandle);
int fluxfs_subsystem_create(struct fs_mount* pMount, unsigned char* filename, uint64_t fileAttribs);
int fluxfs_subsystem_delete(struct fs_mount* pMount, void* pHandle);
int fluxfs_subsystem_verify(struct fs_mount* pMount);
#endif
