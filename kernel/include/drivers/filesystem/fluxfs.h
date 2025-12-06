#ifndef _FLUXFS
#define _FLUXFS
#include <stdint.h>
#define FLUXFS_FILE_ATTRIBUTE_DIRECTORY (1<<0)
#define FLUXFS_FILE_ATTRIBUTE_HIDDEN (1<<1)
#define FLUXFS_SIGNATURE 0x2020534658554C46
struct fluxfs_header{
	uint64_t signature;
	uint32_t headerChecksum;
	uint32_t bytesPerCluster;
	uint64_t freeStackSector;
	uint64_t sectorsPerFreeStack;
	uint64_t fatSector;
	uint64_t sectorsPerFat;
	uint64_t rootCluster;
	unsigned char padding0[456];
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
};
struct fluxfs_mount{
	uint64_t drive_id;
	uint64_t partition_id;
	struct fluxfs_cache_info cacheInfo;
	struct fluxfs_header fsHeader;
}__attribute__((packed));
struct fluxfs_file_handle{
	struct fluxfs_mount* pMount;
}__attribute__((packed));
int fluxfs_init(void);
int fluxfs_mount(uint64_t drive_id, uint64_t partition_id, struct fluxfs_mount** ppMount);
int fluxfs_unmount(struct fluxfs_mount* pMount);
int fluxfs_open(struct fluxfs_mount* pMount, unsigned char* filename, struct fluxfs_file_handle* pFileHandle);
int fluxfs_close(struct fluxfs_file_handle* pFileHandle);
int fluxfs_verify(struct fluxfs_mount* pMount);
int fluxfs_get_header(struct fluxfs_mount* pMount, struct fluxfs_header* pHeader);
#endif
