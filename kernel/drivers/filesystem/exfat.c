#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/drive.h"
#include "crypto/crc.h"
#include "drivers/filesystem/exfat.h"
int exfat_mount(uint64_t drive_id, uint64_t partition_id, struct exfat_mount_handle** ppHandle){
	if (!ppHandle)
		return -1;
	uint64_t handleSize = sizeof(struct exfat_mount_handle);
	struct exfat_mount_handle* pHandle = (struct exfat_mount_handle*)kmalloc(handleSize);
	if (!pHandle)
		return -1;
	memset((void*)pHandle, 0, handleSize);
	pHandle->drive_id = drive_id;
	pHandle->partition_id = partition_id;
	if (exfat_verify(pHandle)!=0){
		exfat_unmount(pHandle);
		return -1;
	}
	*ppHandle = pHandle;
	return 0;
}
int exfat_unmount(struct exfat_mount_handle* pHandle){
	if (!pHandle)
		return -1;
	if (pHandle->cacheInfo.clusterCache){
		uint64_t clusterCachePages = ((pHandle->cacheInfo.clusterCache_end-pHandle->cacheInfo.clusterCache_start)*DRIVE_SECTOR_SIZE)/PAGE_SIZE;
		virtualFreePages((uint64_t)pHandle->cacheInfo.clusterCache, clusterCachePages);
	}
	kfree((void*)pHandle);
	return 0;
}
int exfat_open_file(struct exfat_mount_handle* pHandle, uint16_t* filename, struct exfat_file_handle** ppFileHandle){
	if (!ppFileHandle)
		return -1;
	struct exfat_file_handle* pFileHandle = (struct exfat_file_handle*)kmalloc(sizeof(struct exfat_file_handle));
	if (!pFileHandle)
		return -1;
	uint64_t bytesPerCluster = 0;
	if (exfat_get_bytes_per_cluster(pHandle, &bytesPerCluster)!=0){
		return -1;
	}
	printf("bytes per cluster: %d\r\n", bytesPerCluster);
	return 0;
}
int exfat_close_file(struct exfat_file_handle* pFileHandle){
	if (!pFileHandle)
		return -1;
	kfree((void*)pFileHandle);
	return 0;
}
int exfat_find_file_in_root(struct exfat_mount_handle* pHandle, uint16_t* filename, struct exfat_file_location* pFileLocation){
	if (!pHandle||!filename||!pFileLocation)
		return -1;
	struct exfat_bootsector* pBootSector = (struct exfat_bootsector*)0x0;
	if (exfat_get_bootsector(pHandle, &pBootSector)!=0)
		return -1;
	uint64_t currentCluster = pBootSector->rootDirCluster;
	return 0;
}
int exfat_read_cluster(struct exfat_mount_handle* pHandle, uint64_t cluster, uint8_t* pValue){
	if (!pHandle||!pValue)
		return -1;
	uint64_t fatSector = 0;
	if (exfat_get_fat_sector(pHandle, &fatSector)!=0)
		return -1;
	uint64_t clusterByte = cluster/8;
	uint64_t clusterBit = cluster%8;
	uint64_t clusterSector = fatSector+((clusterByte)/DRIVE_SECTOR_SIZE);
	uint64_t clusterOffset = clusterByte%DRIVE_SECTOR_SIZE;
	unsigned char* pSectorData = (unsigned char*)0x0;
	if (pHandle->cacheInfo.clusterCache&&clusterSector>=pHandle->cacheInfo.clusterCache_start&&clusterSector<=pHandle->cacheInfo.clusterCache_end){
		if (pHandle->cacheInfo.clusterCache[clusterByte]&(1<<clusterBit))
			*pValue = 1;
		else
			*pValue = 0;
		return 0;
	}
	if (pHandle->cacheInfo.clusterCache){
		uint64_t clusterCachePages = ((pHandle->cacheInfo.clusterCache_end-pHandle->cacheInfo.clusterCache_start)*DRIVE_SECTOR_SIZE)/PAGE_SIZE;
		virtualFreePages((uint64_t)pHandle->cacheInfo.clusterCache, clusterCachePages);
		pHandle->cacheInfo.clusterCache = (unsigned char*)0x0;
	}
	if (virtualAllocPage((uint64_t*)&pSectorData, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	if (partition_read_sectors(pHandle->drive_id, pHandle->partition_id, clusterSector, 8, pSectorData)!=0){
		virtualFreePage((uint64_t)pSectorData, 0);
		return -1;
	}
	if (exfat_cache_clusters(pHandle, pSectorData, clusterSector, 8)!=0){
		virtualFreePage((uint64_t)pSectorData, 0);
		return -1;
	}
	unsigned char value = pSectorData[clusterOffset];
	if (value&(1<<clusterBit))
		*pValue = 1;
	else
		pValue = 0;
	return 0;
}
int exfat_write_cluster(struct exfat_mount_handle* pHandle, uint64_t cluster, uint8_t value){
	if (!pHandle)
		return -1;
	uint64_t fatSector = 0;
	uint64_t backupFatSector = 0;
	if (exfat_get_fat_sector(pHandle, &fatSector)!=0)
		return -1;
	if (exfat_get_backup_fat_sector(pHandle, &backupFatSector)!=0)
		return -1;
	uint64_t clusterByte = cluster/8;
	uint64_t clusterBit = cluster%8;
	uint64_t clusterSector = fatSector+(clusterByte/DRIVE_SECTOR_SIZE);
	uint64_t clusterOffset = clusterByte%DRIVE_SECTOR_SIZE;
	unsigned char* pSectorData = (unsigned char*)kmalloc(DRIVE_SECTOR_SIZE);
	if (!pSectorData)
		return -1;
	if (partition_read_sectors(pHandle->drive_id, pHandle->partition_id, clusterSector, 1, pSectorData)!=0){
		kfree((void*)pSectorData);
		return -1;
	}
	unsigned char* pValue = pSectorData+clusterOffset;
	if (value)
		*pValue|=(1<<clusterBit);
	else
		*pValue&=~(1<<clusterBit);
	if (partition_write_sectors(pHandle->drive_id, pHandle->partition_id, clusterSector, 1, pSectorData)!=0){
		kfree((void*)pSectorData);
		return -1;
	}
	if (pHandle->cacheInfo.clusterCache&&clusterSector>=pHandle->cacheInfo.clusterCache_start&&clusterSector<=pHandle->cacheInfo.clusterCache_end){
		uint64_t sectorOffset = clusterSector-pHandle->cacheInfo.clusterCache_start;	
		unsigned char* pCacheValue = pHandle->cacheInfo.clusterCache+(sectorOffset*DRIVE_SECTOR_SIZE)+clusterOffset;
		if (value)
			*pCacheValue|=(1<<clusterBit);
		else
			*pCacheValue&=~(1<<clusterBit);
	}
	kfree((void*)pSectorData);
	return 0;
}
int exfat_read_cluster_data(struct exfat_mount_handle* pHandle, uint64_t cluster, unsigned char* pClusterData){
	if (!pHandle||!pClusterData)
		return -1;
	uint64_t firstDataSector = 0;
	if (exfat_get_first_data_sector(pHandle, &firstDataSector)!=0)
		return -1;
	uint64_t bytesPerCluster = 0;
	if (exfat_get_bytes_per_cluster(pHandle, &bytesPerCluster)!=0)
		return -1;
	uint64_t sectorsPerCluster = bytesPerCluster/DRIVE_SECTOR_SIZE;
	uint64_t dataSectorOffset = ((cluster-2)*bytesPerCluster)/DRIVE_SECTOR_SIZE;
	uint64_t dataSector = firstDataSector+dataSectorOffset;
	if (partition_read_sectors(pHandle->drive_id, pHandle->partition_id, dataSector, sectorsPerCluster, pClusterData)!=0)
		return -1;
	return 0;
}
int exfat_write_cluster_data(struct exfat_mount_handle* pHandle, uint64_t cluster, unsigned char* pClusterData){
	if (!pHandle||!pClusterData)
		return -1;
	uint64_t firstDataSector = 0;
	if (exfat_get_first_data_sector(pHandle, &firstDataSector)!=0)
		return -1;
	uint64_t bytesPerCluster = 0;
	if (exfat_get_bytes_per_cluster(pHandle, &bytesPerCluster)!=0)
		return -1;
	uint64_t sectorsPerCluster = bytesPerCluster/DRIVE_SECTOR_SIZE;
	uint64_t dataSectorOffset = (cluster*bytesPerCluster)/DRIVE_SECTOR_SIZE;
	uint64_t dataSector = firstDataSector+dataSectorOffset;
	if (partition_write_sectors(pHandle->drive_id, pHandle->partition_id, dataSector, sectorsPerCluster, pClusterData)!=0)
		return -1;
	return 0;
}
int exfat_get_bytes_per_cluster(struct exfat_mount_handle* pHandle, uint64_t* pBytesPerCluster){
	if (!pHandle||!pBytesPerCluster)
		return -1;
	struct exfat_bootsector* pBootSector = (struct exfat_bootsector*)0x0;
	if (exfat_get_bootsector(pHandle, &pBootSector)!=0)
		return -1;
	uint64_t sectorsPerCluster = 1<<pBootSector->sectorsPerClusterShift;
	uint64_t bytesPerCluster = sectorsPerCluster*DRIVE_SECTOR_SIZE;
	*pBytesPerCluster = bytesPerCluster;
	return 0;
}
int exfat_get_first_data_sector(struct exfat_mount_handle* pHandle, uint64_t* pFirstDataSector){
	if (!pHandle||!pFirstDataSector)
		return -1;
	struct exfat_bootsector* pBootSector = (struct exfat_bootsector*)0x0;
	if (exfat_get_bootsector(pHandle, &pBootSector)!=0)
		return -1;
	*pFirstDataSector = pBootSector->clusterHeapOffset;
	return 0;
}
int exfat_get_fat_sector(struct exfat_mount_handle* pHandle, uint64_t* pFatSector){
	if (!pHandle||!pFatSector)
		return -1;
	struct exfat_bootsector* pBootSector = (struct exfat_bootsector*)0x0;
	if (exfat_get_bootsector(pHandle, &pBootSector)!=0)
		return -1;
	*pFatSector = pBootSector->fatOffset;
	return 0;
}
int exfat_get_backup_fat_sector(struct exfat_mount_handle* pHandle, uint64_t* pFatSector){
	if (!pHandle||!pFatSector)
		return -1;
	struct exfat_bootsector* pBootSector = (struct exfat_bootsector*)0x0;
	if (exfat_get_bootsector(pHandle, &pBootSector)!=0)
		return -1;
	*pFatSector = pBootSector->fatOffset+pBootSector->sectorsPerFat;
	return 0;
}
int exfat_cache_clusters(struct exfat_mount_handle* pHandle, unsigned char* pClusterData, uint64_t cache_start, uint64_t cache_end){
	if (!pClusterData)
		return -1;
	if (pHandle->cacheInfo.clusterCache){
		uint64_t clusterCachePages = ((pHandle->cacheInfo.clusterCache_end-pHandle->cacheInfo.clusterCache_start)*DRIVE_SECTOR_SIZE)/PAGE_SIZE;
		virtualFreePages((uint64_t)pHandle->cacheInfo.clusterCache, clusterCachePages);
		pHandle->cacheInfo.clusterCache = (unsigned char*)0x0;
		pHandle->cacheInfo.clusterCache_start = 0;
		pHandle->cacheInfo.clusterCache_end = 0;
	}
	pHandle->cacheInfo.clusterCache = pClusterData;
	pHandle->cacheInfo.clusterCache_start = cache_start;
	pHandle->cacheInfo.clusterCache_end = cache_end;
	return 0;
}
int exfat_verify(struct exfat_mount_handle* pHandle){
	if (!pHandle)
		return -1;
	struct exfat_bootsector* pBootSector = (struct exfat_bootsector*)0x0;
	if (exfat_get_bootsector(pHandle, &pBootSector)!=0)
		return -1;
	if (pBootSector->bootSignature!=0xAA55)
		return -1;
	if (pBootSector->signature!=EXFAT_SIGNATURE){
		return -1;
	}
	if (exfat_boot_region_validate(pHandle)!=0)
		return -1;
	uint64_t volumeSize = pBootSector->volumeSectors*DRIVE_SECTOR_SIZE;
	printf("volume size: %dMB\r\n", volumeSize/MEM_MB);
	printf("root cluster number: %d\r\n", pBootSector->rootDirCluster);
	return 0;
}
int exfat_boot_region_validate(struct exfat_mount_handle* pHandle){
	if (!pHandle)
		return -1;
	uint64_t bootRegionSize = EXFAT_BOOT_REGION_SECTORS*DRIVE_SECTOR_SIZE;
	unsigned char* pBootRegion = (unsigned char*)kmalloc(bootRegionSize);
	if (!pBootRegion)
		return -1;
	unsigned char* pBackupBootRegion = (unsigned char*)kmalloc(bootRegionSize);
	if (!pBackupBootRegion)
		return -1;
	if (partition_read_sectors(pHandle->drive_id, pHandle->partition_id, EXFAT_BOOT_REGION_SECTOR, EXFAT_BOOT_REGION_SECTORS, pBootRegion)!=0){
		kfree((void*)pBootRegion);
		kfree((void*)pBackupBootRegion);
		return -1;
	}
	if (partition_read_sectors(pHandle->drive_id, pHandle->partition_id, EXFAT_BACKUP_BOOT_REGION_SECTOR, EXFAT_BOOT_REGION_SECTORS, pBackupBootRegion)!=0){
		kfree((void*)pBootRegion);
		kfree((void*)pBackupBootRegion);
		return -1;
	}
	uint32_t bootRegionChecksum = 0;
	uint32_t backupBootRegionChecksum = 0;
	uint32_t validChecksum = (uint32_t)(*(uint32_t*)(pBootRegion+(DRIVE_SECTOR_SIZE*11)));
	uint32_t validBackupChecksum = (uint32_t)(*(uint32_t*)(pBackupBootRegion+(DRIVE_SECTOR_SIZE*11)));
	if (exfat_get_boot_region_checksum((unsigned char*)pBootRegion, &bootRegionChecksum)!=0){
		kfree((void*)pBootRegion);
		kfree((void*)pBackupBootRegion);
		return -1;
	}
	if (exfat_get_boot_region_checksum((unsigned char*)pBackupBootRegion, &backupBootRegionChecksum)!=0){
		kfree((void*)pBootRegion);
		kfree((void*)pBackupBootRegion);
		return -1;
	}
	if (validChecksum!=bootRegionChecksum){
		printf("valid checksum: 0x%x\r\n", validChecksum);
		printf("current checksum: 0x%x\r\n", bootRegionChecksum);
		return -1;
	}
	if (validBackupChecksum!=backupBootRegionChecksum){
		printf("valid backup checksum: 0x%x\r\n", validBackupChecksum);
		printf("current backup checksum: 0x%x\r\n", backupBootRegionChecksum);
		return -1;
	}
	if (bootRegionChecksum!=backupBootRegionChecksum)
		return -1;
	return 0;
}
int exfat_get_boot_region_checksum(unsigned char* pBootRegion, uint32_t* pChecksum){
	if (!pBootRegion||!pChecksum)
		return -1;
	uint64_t bootRegionSize = EXFAT_BOOT_REGION_SECTORS*DRIVE_SECTOR_SIZE;
	uint64_t checkRegionSize = (EXFAT_BOOT_REGION_SECTORS-1)*DRIVE_SECTOR_SIZE;
	uint32_t checksum = 0x0;
	for (uint64_t i = 0;i<checkRegionSize;i++){
		if (i==106||i==107||i==112)
			continue;
		uint32_t value = 0;
		value = (uint32_t)(pBootRegion[i]);
		checksum = ((checksum&1) ? 0x80000000 : 0)+(checksum>>1)+value;
	}
	*pChecksum = checksum;	
	return 0;
}
int exfat_get_bootsector(struct exfat_mount_handle* pHandle, struct exfat_bootsector** ppBootSector){
	if (!pHandle||!ppBootSector)
		return -1;
	if (pHandle->cacheInfo.bootSectorCached){
		*ppBootSector = &pHandle->cacheInfo.bootSector;
		return 0;
	}
	if (partition_read_sectors(pHandle->drive_id, pHandle->partition_id, EXFAT_BOOT_REGION_SECTOR, 1, (unsigned char*)(&pHandle->cacheInfo.bootSector))!=0)
		return -1;
	*ppBootSector = &pHandle->cacheInfo.bootSector;
	pHandle->cacheInfo.bootSectorCached = 1;
	return 0;
}
