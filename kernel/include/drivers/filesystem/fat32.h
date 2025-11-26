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
#define FAT32_FILE_ATTRIBUTE_LFN (FAT32_FILE_ATTRIBUTE_READONLY|FAT32_ATTRIBUTE_HIDDEN|FAT32_ATTRIBUTE_SYSETM|FAT32_ATTRIBUTE_VOLUME_ID)
#define FAT32_CLUSTER_FREE 0x0
#define FAT32_CLUSTER_BAD 0x0FFFFFF7
#define FAT32_CLUSTER_RESERVED 0x01
#define FAT32_CLUSTER_END_OF_CHAIN(cluster_value)(cluster_value>=0xFFFFFF8&&cluster_value<=0x0FFFFFFF)
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
struct fat32_cache_info{
	uint64_t cached_drive_id;
	uint64_t cached_partition_id;
	struct fat32_bpb bpb_cache;
	struct fat32_fsinfo fsinfo_cache;
	unsigned char lastSectorCache[512];
	uint32_t last_sector;
	uint8_t bpb_cached;
	uint8_t fsinfo_cached;
	uint8_t last_sector_cached;
};
int fat32_openfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, uint64_t* pId);
int fat32_find_file_in_root(uint64_t drive_id, uint64_t partition_id, unsigned char* filename);
int fat32_get_free_cluster(uint64_t drive_id, uint64_t partition_id, uint32_t* pFreeCluster, uint64_t start_cluster);
int fat32_free_cluster(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id);
int fat32_allocate_cluster(uint64_t drive_id, uint64_t partition_id, uint32_t* pClusterId);
int fat32_readcluster(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, struct fat32_cluster_entry* pClusterEntry);
int fat32_writecluster(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, struct fat32_cluster_entry clusterEntry);
int fat32_read_cluster_data(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, unsigned char* pClusterData);
int fat32_write_cluster_data(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, unsigned char* pClusterData);
int fat32_get_cluster_data_sector(uint64_t drive_id, uint64_t partition_id, uint64_t* pClusterDataSector);
int fat32_get_bytes_per_cluster(uint64_t drive_id, uint64_t partition_id, uint64_t* pBytesPerCluster);
int fat32_get_fat(uint64_t drive_id, uint64_t partition_id, uint64_t* pFatSector);
int fat32_get_backup_fat(uint64_t drive_id, uint64_t partition_id, uint64_t* pFatSector);
int fat32_verify(uint64_t drive_id, uint64_t partition_id);
int fat32_get_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb* pBpb);
int fat32_get_ebr(uint64_t drive_id, uint64_t partition_id, struct fat32_ebr* pEbr);
int fat32_get_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo* pFsinfo);
int fat32_set_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb newBpb);
int fat32_set_ebr(uint64_t drive_id, uint64_t partition_id, struct fat32_ebr newEbr);
int fat32_set_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo newFsinfo);
int fat32_get_bpb_cache(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb* pBpb);
int fat32_get_fsinfo_cache(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo* pFsinfo);
int fat32_get_cached_last_sector(uint64_t drive_id, uint64_t partition_id, uint64_t* pLastSector);
int fat32_cache_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb bpb);
int fat32_cache_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo fsinfo);
int fat32_cache_last_sector(uint64_t drive_id, uint64_t partition_id, uint64_t last_sector);
int fat32_bpb_cached(void);
int fat32_fsinfo_cached(void);
int fat32_last_sector_cached(void);
int fat32_flush_cache(void);
int fat32_partition_cached(uint64_t drive_id, uint64_t partition_id);
#endif
