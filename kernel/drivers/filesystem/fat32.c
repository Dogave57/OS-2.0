#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/drive.h"
#include "drivers/gpt.h"
#include "drivers/filesystem/fat32.h"
struct fat32_cache_info cache_info = {
.cached_drive_id = 0xFFFFFFFFFFFFFFF,
.cached_partition_id = 0xFFFFFFFFFF,0
};
int fat32_openfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, uint64_t* pId){
	if (!pId)
		return -1;
	return 0;
}
int fat32_verify(uint64_t drive_id, uint64_t partition_id){
	struct fat32_bpb bpb = {0};
	struct fat32_ebr ebr = {0};
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	if (fat32_get_ebr(drive_id, partition_id, &ebr)!=0)
		return -1;
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	if (ebr.signature!=0x28&&ebr.signature!=0x29){
		printf(L"invalid EBR signature 0x%x\r\n", ebr.signature);
		return -1;
	}
	if (fsinfo.signature0!=FAT32_SIGNATURE0){
		printf(L"invalid signature 0 0x%x\r\n", fsinfo.signature0);
		return -1;
	}
	if (fsinfo.signature1!=FAT32_SIGNATURE1){
		printf(L"invalid signature 1 0x%x\r\n", fsinfo.signature1);
		return -1;
	}
	if (fsinfo.signature2!=FAT32_SIGNATURE2){
		printf(L"invalid signature 2 0x%x\r\n", fsinfo.signature2);
		return -1;
	}
	printf(L"size: %dMB\r\n", (bpb.large_sector_count*DRIVE_SECTOR_SIZE)/MEM_MB);
	printf(L"filesystem info sector: %d\r\n", ebr.fsinfo_sector);
	return 0;
}
int fat32_get_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb* pBpb){
	if (!pBpb)
		return -1;
	if (fat32_get_bpb_cache(drive_id, partition_id, pBpb)==0)
		return 0;
	if (partition_read_sectors(drive_id, partition_id, 0, 1, (unsigned char*)pBpb)!=0)
		return -1;
	fat32_cache_bpb(drive_id, partition_id, *pBpb);
	return 0;
}
int fat32_get_ebr(uint64_t drive_id, uint64_t partition_id, struct fat32_ebr* pEbr){
	if (!pEbr)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	*pEbr = bpb.ebr;
	return 0;
}
int fat32_get_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo* pFsinfo){
	if (!pFsinfo)
		return -1;
	if (fat32_get_fsinfo_cache(drive_id, partition_id, pFsinfo)==0)
		return 0;
	struct fat32_ebr ebr = {0};
	if (fat32_get_ebr(drive_id, partition_id, &ebr)!=0)
		return -1;
	if (partition_read_sectors(drive_id, partition_id, ebr.fsinfo_sector, 1,(unsigned char*)pFsinfo)!=0)
		return -1;

	return 0;
}
int fat32_get_bpb_cache(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb* pBpb){
	if (!pBpb)
		return -1;
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		return -1;
	if (fat32_bpb_cached()!=0)
		return -1;
	*pBpb = cache_info.bpb_cache;
	return 0;
}
int fat32_get_fsinfo_cache(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo* pFsinfo){
	if (!pFsinfo)
		return -1;
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		return -1;
	if (fat32_fsinfo_cached()!=0)
		return -1;
	*pFsinfo = cache_info.fsinfo_cache;
	return 0;
}
int fat32_cache_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb bpb){
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		fat32_flush_cache();
	cache_info.cached_drive_id = drive_id;
	cache_info.cached_partition_id = partition_id;
	cache_info.bpb_cache = bpb;
	cache_info.bpb_cached = 1;
	return 0;
}
int fat32_cache_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo fsinfo){
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		fat32_flush_cache();
	cache_info.cached_drive_id = drive_id;
	cache_info.cached_partition_id = partition_id;
	cache_info.fsinfo_cache = fsinfo;
	cache_info.fsinfo_cached = 1;
	return 0;
}
int fat32_bpb_cached(void){
	if (cache_info.bpb_cached!=0)
		return 0;
	return -1;
}
int fat32_fsinfo_cached(void){
	if (cache_info.fsinfo_cached!=0)
		return 0;
	return -1;
}
int fat32_flush_cache(void){
	cache_info.cached_drive_id = 0xFFFFFFFFFFFFFFFF;
	cache_info.cached_partition_id = 0xFFFFFFFFFFFFFFF;
	cache_info.bpb_cached = 0;
	cache_info.fsinfo_cached = 0;
	return 0;
}
int fat32_partition_cached(uint64_t drive_id, uint64_t partition_id){
	if (cache_info.cached_drive_id!=drive_id||cache_info.cached_partition_id!=partition_id)
		return -1;
	return 0;
}
