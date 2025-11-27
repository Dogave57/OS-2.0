#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/drive.h"
#include "align.h"
#include "drivers/gpt.h"
#include "drivers/timer.h"
#include "drivers/filesystem/fat32.h"
struct fat32_cache_info cache_info = {
.cached_drive_id = 0xFFFFFFFFFFFFFFF,
.cached_partition_id = 0xFFFFFFFFFF,0
};
int fat32_openfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, uint64_t* pId){
	if (!pId)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	uint64_t time_ms = get_time_ms();
	uint64_t cluster_size = MEM_MB;
	uint64_t cluster_count = cluster_size/PAGE_SIZE;
	for (uint64_t i = 0;i<cluster_count;i++){
		uint32_t cluster_id = 0;
		if (fat32_allocate_cluster(drive_id, partition_id, &cluster_id)!=0){
			printf(L"failed to allocate cluster\r\n");
			return -1;
		}
	}
	uint64_t elapsed_ms = get_time_ms()-time_ms;
	printf(L"took %dms to allocate %dMB worth of clusters at a rate of %dMB/s\r\n", elapsed_ms, cluster_size/MEM_MB, (1000/elapsed_ms)*(cluster_size/MEM_MB));
	uint32_t fileCluster = 0;
	uint64_t fileIndex = 0;
	struct fat32_file_location fileLocation = {0};
	if (fat32_find_file(drive_id, partition_id, filename, &fileLocation, 0)!=0){
		printf(L"failed to find file\r\n");
		return -1;
	}
	return 0;
}
int fat32_file_entry_namecmp(struct fat32_file_entry fileEntry, unsigned char* filename){
	if (!filename)
		return -1;
	if (!fileEntry.filename[0])
		return -1;
	for (uint64_t i = 0;i<sizeof(fileEntry.filename);i++){
		unsigned char ch = filename[i];
		if (ch&&fileEntry.filename[i]==' '&&i!=7){
			return -1;
		}
		if (!ch&&fileEntry.filename[i]!=' '&&i!=7){
			return -1;
		}
		if (!ch)
			break;
		if (fileEntry.filename[i]==' '&&i!=7){
			return -1;
		}
		if (ch=='.'&&i==7){
			ch =' ';
		}
		if (ch!=fileEntry.filename[i]){
			return -1;
		}
		if (i==sizeof(fileEntry.filename)-1&&filename[i+1])
			return -1;
	}
	return 0;
}
int fat32_find_file(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, struct fat32_file_location* pFileLocation, uint8_t file_attribs){
	if (!filename||!pFileLocation)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint32_t root_cluster = bpb.ebr.root_cluster_number;
	uint64_t pathstart = 0;
	struct fat32_file_location fileLocation = {0};
	fileLocation.fileDataCluster = root_cluster;
	for (uint64_t i = 0;;i++){
		if (!fileLocation.fileDataCluster)
			fileLocation.fileDataCluster = root_cluster;
		unsigned char ch = filename[i];
		if (ch!='/'&&ch)
			continue;
		filename[i] = 0;
		uint8_t mask = FAT32_FILE_ATTRIBUTE_DIRECTORY;
		if (!ch)
			mask = 0;
		if (fat32_find_file_in_dir(drive_id, partition_id, fileLocation.fileDataCluster, filename+pathstart, &fileLocation, mask)!=0){
			printf_ascii("failed to find %s\r\n", filename+pathstart);
			return -1;
		}
		if (ch)
			filename[i] = '/';
		pathstart = i+1;
		if (!ch)
			break;
	}
	return 0;
}
int fat32_find_file_in_dir(uint64_t drive_id, uint64_t partition_id, uint32_t dirCluster, unsigned char* filename, struct fat32_file_location* pFileLocation, uint8_t file_attrib){
	if (!filename||!pFileLocation)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(drive_id, partition_id, &bytes_per_cluster)!=0)
		return -1;
	uint64_t max_dir_entries = bytes_per_cluster/sizeof(struct fat32_file_entry);
	struct fat32_cluster_entry currentCluster = {0};
	currentCluster.cluster_value = dirCluster;
	uint64_t clusterDataPages = align_up(bytes_per_cluster, PAGE_SIZE)/PAGE_SIZE;
	struct fat32_file_entry* pClusterData = (struct fat32_file_entry*)0x0;
	if (virtualAllocPages((uint64_t*)&pClusterData, clusterDataPages, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	while (FAT32_CONTINUE_CLUSTER(currentCluster.cluster_value)){
		if (fat32_read_cluster_data(drive_id, partition_id, currentCluster.cluster_value, (unsigned char*)pClusterData)!=0){
			break;
		}
		for (uint64_t file_index = 0;file_index<max_dir_entries;file_index++){
			struct fat32_file_entry* pFileEntry = pClusterData+file_index;
			if (fat32_file_entry_namecmp(*pFileEntry, filename)!=0)
				continue;
			if (file_attrib&&!(pFileEntry->file_attribs&file_attrib))
				continue;
			struct fat32_file_location fileLocation = {0};
			fileLocation.fileCluster = currentCluster.cluster_value;
			fileLocation.fileIndex = file_index;
			fileLocation.fileDataCluster = (((uint32_t)pFileEntry->entry_first_cluster_high)<<16)|(uint32_t)pFileEntry->entry_first_cluster_low;
			virtualFreePages((uint64_t)pClusterData, clusterDataPages);
			*pFileLocation = fileLocation;
			return 0;
		}
		if (fat32_readcluster(drive_id, partition_id, currentCluster.cluster_value, &currentCluster)!=0){
			break;
		}
	}
	virtualFreePages((uint64_t)pClusterData, clusterDataPages);
	return -1;
}
int fat32_find_file_in_root(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, struct fat32_file_location* pFileLocation, uint8_t file_attribs){
	if (!filename||!pFileLocation)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint32_t rootDirCluster = bpb.ebr.root_cluster_number;
	if (fat32_find_file_in_dir(drive_id, partition_id, rootDirCluster, filename, pFileLocation, file_attribs)!=0)
		return -1;
	return 0;
}
int fat32_get_free_cluster(uint64_t drive_id, uint64_t partition_id, uint32_t* pFreeCluster, uint64_t start_cluster){
	if (!pFreeCluster)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	if (!start_cluster){
		start_cluster = fsinfo.start_lookup_cluster;
	}
	if (fsinfo.last_free_cluster!=0xFFFFFFFF){
		struct fat32_cluster_entry clusterValue = {0};
		if (fat32_readcluster(drive_id, partition_id, fsinfo.last_free_cluster, &clusterValue)!=0)
			return -1;
		if (clusterValue.cluster_value==FAT32_CLUSTER_FREE){
			*pFreeCluster = fsinfo.last_free_cluster;
			return 0;
		}
	}
	for (uint32_t i = start_cluster;;){
		struct fat32_cluster_entry clusterEntry = {0};
		if (fat32_readcluster(drive_id, partition_id, i, &clusterEntry)!=0)
			return -1;
		if (clusterEntry.cluster_value==FAT32_CLUSTER_FREE){
			*pFreeCluster = i;
			fsinfo.last_free_cluster = i;
			fat32_set_fsinfo(drive_id, partition_id, fsinfo);
			return 0;
		}
		if (clusterEntry.cluster_value==FAT32_CLUSTER_BAD){
			i++;
			continue;
		}
		if (FAT32_CLUSTER_END_OF_CHAIN(clusterEntry.cluster_value)){
			return -1;
		}
		i = clusterEntry.cluster_value;
	}
	return -1;
}
int fat32_free_cluster(uint64_t drive_id, uint64_t partition_id, uint32_t freeCluster){
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	struct fat32_cluster_entry newClusterEntry = {0};
	newClusterEntry.cluster_value = FAT32_CLUSTER_FREE;
	if (fat32_writecluster(drive_id, partition_id, freeCluster, newClusterEntry)!=0)
		return -1;
	if (freeCluster>fsinfo.last_free_cluster)
		fsinfo.last_free_cluster = freeCluster;
	fat32_set_fsinfo(drive_id, partition_id, fsinfo);
	return 0;
}
int fat32_allocate_cluster(uint64_t drive_id, uint64_t partition_id, uint32_t* pNewCluster){
	if (!pNewCluster)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	uint32_t newCluster = 0;
	if (fat32_get_free_cluster(drive_id, partition_id, &newCluster, 0)!=0){
		printf(L"failed to get free cluster\r\n");
		return -1;
	}
	struct fat32_cluster_entry newEntry = {0};
	newEntry.cluster_value = newCluster+1;
	if (fat32_writecluster(drive_id, partition_id, newCluster, newEntry)!=0){
		printf(L"failed to write to cluster\r\n");
		return -1;
	}
	*pNewCluster = newCluster;
	return 0;
}
int fat32_readcluster(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, struct fat32_cluster_entry* pClusterEntry){
	if (!pClusterEntry)
		return -1;
	uint64_t fatSector = 0;
	if (fat32_get_fat(drive_id, partition_id, &fatSector)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(drive_id, partition_id, &bytes_per_cluster)!=0)
		return -1;
	uint64_t cluster_byte_offset = cluster_id*sizeof(uint32_t);
	uint64_t cluster_sector_offset = cluster_byte_offset/DRIVE_SECTOR_SIZE;
	uint64_t cluster_offset = cluster_byte_offset%DRIVE_SECTOR_SIZE;
	uint64_t cluster_sector = fatSector+cluster_sector_offset;
	unsigned char* pSectorData = (unsigned char*)cache_info.lastSectorCache;
	uint64_t cached_last_sector = 0;
	fat32_get_cached_last_sector(drive_id, partition_id, &cached_last_sector);
	if (cached_last_sector==cluster_sector){
		*(uint32_t*)pClusterEntry = *(uint32_t*)(cache_info.lastSectorCache+cluster_offset);
		return 0;
	}
	if (fat32_cache_last_sector(drive_id, partition_id, cluster_sector)!=0)
		return -1;
	if (partition_read_sectors(drive_id, partition_id, cluster_sector, 1, (unsigned char*)pSectorData)!=0)
		return -1;
	uint32_t cluster_value = *(uint32_t*)(cache_info.lastSectorCache+cluster_offset);
	*(uint32_t*)pClusterEntry = cluster_value;
	return 0;
}
int fat32_writecluster(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, struct fat32_cluster_entry clusterEntry){
	uint64_t fatSector = 0;
	uint64_t backupFatSector = 0;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	if (fat32_get_fat(drive_id, partition_id, &fatSector)!=0)
		return -1;
	if (fat32_get_backup_fat(drive_id, partition_id, &backupFatSector)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(drive_id, partition_id, &bytes_per_cluster)!=0)
		return -1;
	uint64_t cluster_byte_offset = cluster_id*sizeof(uint32_t);
	uint64_t cluster_sector_offset = cluster_byte_offset/DRIVE_SECTOR_SIZE;
	uint64_t cluster_offset = cluster_byte_offset%DRIVE_SECTOR_SIZE;
	uint64_t cluster_sector = fatSector+cluster_sector_offset;
	uint64_t cluster_backup_sector = backupFatSector+cluster_sector_offset;
	unsigned char sectorData[512] = {0};
	if (partition_read_sectors(drive_id, partition_id, cluster_sector, 1, (unsigned char*)sectorData)!=0)
		return -1;
	*(uint32_t*)(sectorData+cluster_offset) = *(uint32_t*)(&clusterEntry);
	if (partition_write_sectors(drive_id, partition_id, cluster_sector, 1, (unsigned char*)sectorData)!=0)
		return -1;
	if (partition_write_sectors(drive_id, partition_id, cluster_backup_sector, 1, (unsigned char*)sectorData)!=0)
		return -1;
	uint64_t cached_last_sector = 0;
	if (fat32_get_cached_last_sector(drive_id, partition_id, &cached_last_sector)!=0)
		return 0;
	if (cluster_sector!=cached_last_sector)
		return 0;
	*(uint32_t*)(cache_info.lastSectorCache+cluster_offset) = *(uint32_t*)(&clusterEntry);
	return 0;
}
int fat32_read_cluster_data(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, unsigned char* pClusterData){
	if (!pClusterData)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(drive_id, partition_id, &bytes_per_cluster)!=0)
		return -1;
	uint64_t sectors_per_cluster = bytes_per_cluster/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_start_sector = 0;
	if (fat32_get_cluster_data_sector(drive_id, partition_id, &cluster_data_start_sector)!=0)
		return -1;
	uint64_t cluster_sector_offset = ((cluster_id-2)*bytes_per_cluster)/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_sector = cluster_data_start_sector+cluster_sector_offset;
	if (partition_read_sectors(drive_id, partition_id, cluster_data_sector, sectors_per_cluster, pClusterData)!=0)
		return -1;
	return 0;
}
int fat32_write_cluster_data(uint64_t drive_id, uint64_t partition_id, uint32_t cluster_id, unsigned char* pClusterData){
	if (!pClusterData)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(drive_id, partition_id, &bytes_per_cluster)!=0)
		return -1;
	uint64_t sectors_per_cluster = bytes_per_cluster/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_start_sector = 0;
	if (fat32_get_cluster_data_sector(drive_id, partition_id, &cluster_data_start_sector)!=0)
		return -1;
	uint64_t cluster_sector_offset = ((cluster_id-2)*bytes_per_cluster)/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_sector = cluster_data_start_sector+cluster_sector_offset;
	if (partition_write_sectors(drive_id, partition_id, cluster_data_sector, sectors_per_cluster, pClusterData)!=0)
		return -1;
	return 0;
}
int fat32_get_cluster_data_sector(uint64_t drive_id, uint64_t partition_id, uint64_t* pClusterDataSector){
	if (!pClusterDataSector)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t sectors_per_fat = 0;
	if (fat32_get_sectors_per_fat(drive_id, partition_id, &sectors_per_fat)!=0)
		return -1;
	uint64_t clusterDataSector = ((uint64_t)bpb.reserved_sectors)+sectors_per_fat*bpb.fat_count;
	*pClusterDataSector = clusterDataSector;
	return 0;
}
int fat32_get_bytes_per_cluster(uint64_t drive_id, uint64_t partition_id, uint64_t* pBytesPerCluster){
	if (!pBytesPerCluster)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t bytesPerCluster = ((uint64_t)bpb.sectors_per_cluster)*DRIVE_SECTOR_SIZE;
	*pBytesPerCluster = bytesPerCluster;
	return 0;
}
int fat32_get_fat(uint64_t drive_id, uint64_t partition_id, uint64_t* pFatSector){
	if (!pFatSector)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t fat_sector = (uint64_t)bpb.reserved_sectors;
	*pFatSector = fat_sector;
	return 0;
}
int fat32_get_backup_fat(uint64_t drive_id, uint64_t partition_id, uint64_t* pFatSector){
	if (!pFatSector)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t sectors_per_fat = 0;
	if (fat32_get_sectors_per_fat(drive_id, partition_id, &sectors_per_fat)!=0)
		return -1;
	uint64_t fat_sector = (uint64_t)(bpb.reserved_sectors+sectors_per_fat);
	*pFatSector = fat_sector;
	return 0;
}
int fat32_get_sectors_per_fat(uint64_t drive_id, uint64_t partition_id, uint64_t* pSectorsPerFat){
	if (!pSectorsPerFat)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t sectors_per_fat = bpb.ebr.sectors_per_fat;
	if (!sectors_per_fat)
		return -1;
	*pSectorsPerFat = sectors_per_fat;
	return 0;
}
int fat32_get_sector_count(uint64_t drive_id, uint64_t partition_id, uint64_t* pSectorCount){
	if (!pSectorCount)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t sectorCount = 0;
	if (bpb.large_sector_count)
		sectorCount = bpb.large_sector_count;
	else
		sectorCount = bpb.total_sectors;
	*pSectorCount = sectorCount;
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
	if (ebr.system_ident_string!=FAT32_SYSTEM_IDENT_STRING){
		printf(L"invalid system identifier 0x%x\r\n", ebr.system_ident_string);
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
	uint64_t fat_sectors = 0;
	if (fat32_get_sectors_per_fat(drive_id, partition_id, &fat_sectors)!=0)
		return -1;
	printf(L"size: %dMB\r\n", (bpb.large_sector_count*DRIVE_SECTOR_SIZE)/MEM_MB);
	printf(L"sectors per fat: %d\r\n", fat_sectors);
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
int fat32_set_bpb(uint64_t drive_id, uint64_t partition_id, struct fat32_bpb newBpb){
	if (partition_write_sectors(drive_id, partition_id, 0, 1, (unsigned char*)&newBpb)!=0)
		return -1;
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		return 0;
	if (fat32_bpb_cached()!=0)
		return -1;
	cache_info.bpb_cache = newBpb;
	return 0;
}
int fat32_set_ebr(uint64_t drive_id, uint64_t partition_id, struct fat32_ebr newEbr){
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	bpb.ebr = newEbr;
	if (fat32_set_bpb(drive_id, partition_id, bpb)!=0)
		return -1;
	return 0;
}
int fat32_set_fsinfo(uint64_t drive_id, uint64_t partition_id, struct fat32_fsinfo newFsinfo){
	struct fat32_ebr ebr = {0};
	if (fat32_get_ebr(drive_id, partition_id, &ebr)!=0)
		return -1;
	if (partition_write_sectors(drive_id, partition_id, ebr.fsinfo_sector, 1, (unsigned char*)&newFsinfo)!=0)
		return -1;
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		return 0;
	if (fat32_fsinfo_cached()!=0)
		return -1;
	cache_info.fsinfo_cache = newFsinfo;
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
int fat32_get_cached_last_sector(uint64_t drive_id, uint64_t partition_id, uint64_t* pLastSector){
	if (!pLastSector)
		return -1;
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		return -1;
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		return -1;
	*pLastSector = cache_info.last_sector;
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
int fat32_cache_last_sector(uint64_t drive_id, uint64_t partition_id, uint64_t last_sector){
	if (fat32_partition_cached(drive_id, partition_id)!=0)
		fat32_flush_cache();
	cache_info.cached_drive_id = drive_id;
	cache_info.cached_partition_id = partition_id;
	cache_info.last_sector = last_sector;
	cache_info.last_sector_cached = 1;
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
int fat32_last_sector_cached(void){
	if (cache_info.last_sector_cached!=0)
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
