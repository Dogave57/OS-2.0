#ifndef _FAT32
#define _FAT32
#define FAT32_SIGNATURE0 (0x41615252)
#define FAT32_SIGNATURE1 (0x61417272)
#define FAT32_SIGNATURE2 (0xAA550000)
#define FAT32_SYSTEM_IDENT_STRING ((uint64_t)0x2020203233544146)
#define FAT32_FILE_ATTRIBUTE_READONLY 0x01
#define FAT32_FILE_ATTRIBUTE_HIDDEN 0x02
#define FAT32_FILE_ATTRIBUTE_SYSTEM 0x04
#define FAT32_FILE_ATTRIBUTE_VOLUME_ID 0x08
#define FAT32_FILE_ATTRIBUTE_DIRECTORY 0x10
#define FAT32_FILE_ATTRIBUTE_ARCHIVE 0x20
#define FAT32_FILE_ATTRIBUTE_LFN (FAT32_FILE_ATTRIBUTE_READONLY|FAT32_FILE_ATTRIBUTE_HIDDEN|FAT32_FILE_ATTRIBUTE_SYSTEM|FAT32_FILE_ATTRIBUTE_VOLUME_ID)
#define FAT32_CLUSTER_FREE 0x0
#define FAT32_CLUSTER_BAD 0x0FFFFFF7
#define FAT32_CLUSTER_RESERVED 0x01
#define FAT32_CLUSTER_EOC 0xFFFFFF8
#define FAT32_CLUSTER_END_OF_CHAIN(cluster_value)(cluster_value>=0xFFFFFF8&&cluster_value<=0x0FFFFFFF)
#define FAT32_CONTINUE_CLUSTER(cluster_value)(cluster_value!=FAT32_CLUSTER_FREE&&cluster_value!=FAT32_CLUSTER_BAD&&cluster_value!=FAT32_CLUSTER_RESERVED&&!FAT32_CLUSTER_END_OF_CHAIN(cluster_value))
#define FAT32_FILENAME_LEN_MAX 11
struct fat32_fsinfo{
	uint32_t signature0;
	uint8_t reserved0[480];
	uint32_t signature1;
	uint32_t last_free_cluster;
	uint32_t start_lookup_cluster;
	uint8_t reserved1[12];
	uint32_t signature2;
}__attribute__((packed));
struct fat32_ebr{
	uint32_t sectors_per_fat;
	uint16_t flags;
	uint16_t fat_version;
	uint32_t root_cluster_number;
	uint16_t fsinfo_sector;
	uint16_t backup_boot_sector;
	uint8_t reserved0[12];
	uint8_t drive_number;
	uint8_t windows_nt_flags;
	uint8_t signature;
	uint8_t volume_id[4];
	uint8_t volume_label_string[11];
	uint64_t system_ident_string;
	uint8_t bootcode[420];
	uint16_t boot_partition_signature;
	uint8_t padding0[36];
}__attribute__((packed));
struct fat32_bpb{
	uint8_t loop_code[3];
	uint8_t oem_ident[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fat_count;
	uint16_t root_entry_count;
	uint16_t total_sectors;	
	uint8_t media_desc_type;
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t head_count;
	uint32_t hidden_sector_count;
	uint32_t large_sector_count;
	struct fat32_ebr ebr;
}__attribute__((packed));
struct fat32_time{
	uint16_t hour:5;
	uint16_t minutes:6;
	uint16_t seconds:5;
}__attribute__((packed));
struct fat32_date{
	uint16_t year:7;
	uint16_t month:4;
	uint16_t day:5;
}__attribute__((packed));
struct fat32_cluster_entry{
	uint32_t cluster_value:27;
	uint32_t reserved:4;
}__attribute__((packed));
struct fat32_file_entry{
	unsigned char filename[11];
	uint8_t file_attribs;
	uint8_t reserved_nt;
	uint8_t creation_calculation_time;
	struct fat32_time creation_time;
	struct fat32_date creation_date;
	struct fat32_date last_accessed_date;
	uint16_t entry_first_cluster_high;
	struct fat32_time last_modification_time;
	struct fat32_date last_modification_date;
	uint16_t entry_first_cluster_low;
	uint32_t file_size;
}__attribute__((packed));
struct fat32_file_entry_lfn{
	uint16_t filename0[5];
	uint8_t file_attribs;
	uint8_t type;
	uint8_t checksum;
	uint16_t filename1[6];
	uint16_t firstClusterLow;
	uint16_t filename2[2];
}__attribute__((packed));
struct fat32_cache_info{
	struct fat32_bpb bpb_cache;
	struct fat32_fsinfo fsinfo_cache;
	unsigned char lastSectorCache[512];
	uint32_t last_sector;
	uint8_t bpb_cached;
	uint8_t fsinfo_cached;
	uint8_t last_sector_cached;
};
struct fat32_file_location{
	uint32_t fileCluster;
	uint32_t fileIndex;
	uint32_t fileDataCluster;
};
struct fat32_file_handle{
	struct fat32_mount_handle* pMountHandle;
	struct fat32_file_location fileLocation;
	struct fat32_file_entry fileEntry;
};
struct fat32_simple_file_entry{
	unsigned char filename[12];
	uint32_t fileSize;
	uint8_t fileAttribs;
};
struct fat32_dir_desc{
	uint32_t firstDirCluster;
};
struct fat32_dir_handle{
	struct fat32_mount_handle* pMountHandle;
	struct fat32_dir_desc desc;
	struct fat32_cluster_entry currentDirCluster;
	uint64_t currentDirIndex;
};
struct fat32_mount_handle{
	uint64_t drive_id;
	uint64_t partition_id;
	struct fat32_cache_info cacheInfo;
};
int fat32_init(void);
int fat32_mount(uint64_t drive_id, uint64_t partition_id, struct fat32_mount_handle** ppHandle);
int fat32_unmount(struct fat32_mount_handle* pMountHandle);
int fat32_opendir(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_dir_handle** ppDirHandle);
int fat32_closedir(struct fat32_dir_handle* pDirHandle);
int fat32_read_dir(struct fat32_dir_handle* pDirHandle, struct fat32_simple_file_entry* pFileEntry);
int fat32_dir_rewind(struct fat32_dir_handle* pDirHandle);
int fat32_openfile(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_file_handle** ppFileHandle);
int fat32_closefile(struct fat32_file_handle* pFileHandle);
int fat32_get_file_size(struct fat32_file_handle* pFileHandle, uint32_t* pFileSize);
int fat32_get_file_attributes(struct fat32_file_handle* pFileHandle, uint8_t* pFileAttributes);
int fat32_get_filename(struct fat32_file_handle* pFileHandle, unsigned char* filename);
int fat32_renamefile(struct fat32_file_handle* pFileHandle, unsigned char* filename);
int fat32_readfile(struct fat32_file_handle* pFileHandle, unsigned char* pBuffer, uint32_t size);
int fat32_writefile(struct fat32_file_handle* pFileHandle, unsigned char* pFileBuffer, uint32_t size);
int fat32_createfile(struct fat32_mount_handle* pMountHandlle, unsigned char* filename, uint8_t file_attribs);
int fat32_deletefile(struct fat32_file_handle* pFileHandle);
int fat32_get_simple_file_entry(struct fat32_file_entry fileEntry, struct fat32_simple_file_entry* pSimpleFileEntry);
int fat32_filename_to_string(unsigned char* dest, unsigned char* src);
int fat32_string_to_filename(unsigned char* dest, unsigned char* src);
int fat32_file_entry_namecmp(struct fat32_file_entry fileEntry, unsigned char* filename);
int fat32_find_file(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_file_entry* pFileEntry, struct fat32_file_location* pFileLocation, uint8_t file_attribs);
int fat32_find_file_in_dir(struct fat32_mount_handle* pMountHandle, uint32_t dir_cluster, unsigned char* filename, struct fat32_file_entry* pFileEntry,struct fat32_file_location* pFileLocation, uint8_t file_attirbs);
int fat32_find_file_in_root(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_file_entry* pFileEntry,struct fat32_file_location* pFileLocation, uint8_t file_attribs);
int fat32_get_free_cluster(struct fat32_mount_handle* pMountHandle, uint32_t* pFreeCluster, uint64_t start_cluster);
int fat32_free_cluster(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id);
int fat32_allocate_cluster(struct fat32_mount_handle* pMountHandle, uint32_t* pClusterId);
int fat32_readcluster(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, struct fat32_cluster_entry* pClusterEntry);
int fat32_writecluster(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, struct fat32_cluster_entry clusterEntry);
int fat32_read_cluster_data(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, unsigned char* pClusterData);
int fat32_write_cluster_data(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, unsigned char* pClusterData);
int fat32_get_cluster_data_sector(struct fat32_mount_handle* pMountHandle, uint64_t* pClusterDataSector);
int fat32_get_bytes_per_cluster(struct fat32_mount_handle* pMountHandle, uint64_t* pBytesPerCluster);
int fat32_get_fat(struct fat32_mount_handle* pMountHandle, uint64_t* pFatSector);
int fat32_get_backup_fat(struct fat32_mount_handle* pMountHandle, uint64_t* pFatSector);
int fat32_get_sectors_per_fat(struct fat32_mount_handle* pMountHandle, uint64_t* pSectorsPerFat);
int fat32_get_sector_count(struct fat32_mount_handle* pMountHandle, uint64_t* pSectorCount);
int fat32_verify(struct fat32_mount_handle* pMountHandle);
int fat32_get_bpb(struct fat32_mount_handle* pMountHandle, struct fat32_bpb* pBpb);
int fat32_get_ebr(struct fat32_mount_handle* pMountHandle, struct fat32_ebr* pEbr);
int fat32_get_fsinfo(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo* pFsinfo);
int fat32_set_bpb(struct fat32_mount_handle* pMountHandle, struct fat32_bpb newBpb);
int fat32_set_ebr(struct fat32_mount_handle* pMountHandle, struct fat32_ebr newEbr);
int fat32_set_fsinfo(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo newFsinfo);
int fat32_get_bpb_cache(struct fat32_mount_handle* pMountHandle, struct fat32_bpb* pBpb);
int fat32_get_fsinfo_cache(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo* pFsinfo);
int fat32_get_cached_last_sector(struct fat32_mount_handle* pMountHandle, uint64_t* pLastSector);
int fat32_cache_bpb(struct fat32_mount_handle* pMountHandle, struct fat32_bpb bpb);
int fat32_cache_fsinfo(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo fsinfo);
int fat32_cache_last_sector(struct fat32_mount_handle* pMountHandle, uint64_t last_sector);
int fat32_flush_cache(struct fat32_mount_handle* pMountHandle);
int fat32_subsystem_verify(struct fs_mount* pMount);
int fat32_subsystem_mount(uint64_t drive_id, uint64_t partition_id, struct fs_mount* pMount);
int fat32_subsystem_unmount(struct fs_mount* pMount);
int fat32_subsystem_open(struct fs_mount* pMount, unsigned char* filename, void** pFileHandle);
int fat32_subsystem_close(void* pFileHandle);
int fat32_subsystem_read(struct fs_mount* pMount, void* pFileHandle, unsigned char* pBuffer, uint64_t size);
int fat32_subsystem_write(struct fs_mount* pMount, void* pFileHandle, unsigned char* pBuffer, uint64_t size);
int fat32_subsystem_getFileInfo(struct fs_mount* pMount, void* pFileHandle, struct fs_file_info* pFileInfo);
int fat32_subsystem_create_file(struct fs_mount* pMount, unsigned char* filename, uint64_t fileAttribs);
int fat32_subsystem_delete_file(struct fs_mount* pMount, void* pFileHandle);
int fat32_subsystem_open_dir(struct fs_mount* pMount, unsigned char* filename, void** ppFileHandle);
int fat32_subsystem_read_dir(struct fs_mount* pMount, void* pFileHandle, struct fs_file_info* pFileInfo);
int fat32_subsystem_rewind_dir(struct fs_mount* pMount, void* pFileHandle);
int fat32_subsystem_close_dir(void* pFileHandle);
#endif
