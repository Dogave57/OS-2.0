#ifndef _FAT32
#define _FAT32
#define FAT32_SIGNATURE0 (0x41615252)
#define FAT32_SIGNATURE1 (0x61417272)
#define FAT32_SIGNATURE2 (0xAA550000)
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
struct fat32_cache_info{
	uint64_t cached_drive_id;
	uint64_t cached_partition_id;
	struct fat32_bpb bpb_cache;
	struct fat32_fsinfo fsinfo_cache;
	uint8_t bpb_cached;
	uint8_t fsinfo_cached;
};
int fat32_openfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, uint64_t* pId);
int fat32_verify(uint64_t drive_id, uint64_t partition_id);
int fat32_get_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb* pBpb);
int fat32_get_ebr(uint64_t drive_id, uint64_t partition_id, struct fat32_ebr* pEbr);
int fat32_get_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo* pFsinfo);
int fat32_get_bpb_cache(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb* pBpb);
int fat32_get_fsinfo_cache(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo* pFsinfo);
int fat32_cache_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb bpb);
int fat32_cache_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo fsinfo);
int fat32_bpb_cached(void);
int fat32_fsinfo_cached(void);
int fat32_flush_cache(void);
int fat32_partition_cached(uint64_t drive_id, uint64_t partition_id);
#endif
