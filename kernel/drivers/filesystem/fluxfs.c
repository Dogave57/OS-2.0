#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/gpt.h"
#include "drivers/timer.h"
#include "crypto/crc.h"
#include "subsystem/filesystem.h"
#include "align.h"
#include "drivers/filesystem/fluxfs.h"
static struct fs_driver_desc* pDriverDesc = (struct fs_driver_desc*)0x0;
int fluxfs_init(void){
	struct fs_driver_vtable vtable = {0};
	vtable.mount = (fsMountFunc)fluxfs_subsystem_mount;
	vtable.unmount = (fsUnmountFunc)fluxfs_subsystem_unmount;
	vtable.open = (fsOpenFunc)fluxfs_subsystem_open;
	vtable.close = (fsCloseFunc)fluxfs_subsystem_close;
	vtable.create = (fsCreateFunc)fluxfs_subsystem_create;
	vtable.delete = (fsDeleteFunc)fluxfs_subsystem_delete;
	vtable.verify = (fsVerifyFunc)fluxfs_subsystem_verify;
	if (fs_driver_register(vtable, &pDriverDesc)!=0)
		return -1;
	return 0;
}
int fluxfs_mount(uint64_t drive_id, uint64_t partition_id, struct fluxfs_mount** ppMount){
	if (!ppMount)
		return -1;
	struct fluxfs_mount* pMount = (struct fluxfs_mount*)kmalloc(sizeof(struct fluxfs_mount));
	if (!pMount)
		return -1;
	memset((void*)pMount, 0, sizeof(struct fluxfs_mount));
	pMount->drive_id = drive_id;
	pMount->partition_id = partition_id;
	if (fluxfs_verify(pMount)!=0){
		kfree((void*)pMount);
		return -1;
	}
	pMount->cacheInfo.cachedSectors = 16;
	pMount->cacheInfo.lastCachedSectorGroup = 0xFFFFFFFFFFFFFFFF;
	unsigned char* pSectorCache = (unsigned char*)kmalloc(pMount->cacheInfo.cachedSectors*DRIVE_SECTOR_SIZE);
	if (!pSectorCache){
		kfree((void*)pMount);
		return -1;
	}
	pMount->pSectorCache = pSectorCache;
	*ppMount = pMount;
	return 0;
}
int fluxfs_unmount(struct fluxfs_mount* pMount){
	if (!pMount)
		return -1;
	if (kfree((void*)pMount->pSectorCache)!=0)
		return -1;
	if (kfree((void*)pMount)!=0)
		return -1;
	return 0;
}
int fluxfs_open(struct fluxfs_mount* pMount, unsigned char* filename, struct fluxfs_file_handle** ppFileHandle){
	if (!pMount||!filename||!ppFileHandle)
		return -1;
	
	return 0;
}
int fluxfs_close(struct fluxfs_file_handle* pFileHandle){
	if (!pFileHandle)
		return -1;
	if (kfree((void*)pFileHandle)!=0)
		return -1;
	return 0;
}
int fluxfs_create(struct fluxfs_mount* pMount, unsigned char* filename, uint64_t fileAttribs){
	if (!pMount||!filename){
		return -1;
	}
	struct fluxfs_header fluxHeader = {0};
	if (fluxfs_get_header(pMount, &fluxHeader)!=0){
		printf("failed to get flux header\r\n");
		return -1;
	}
	printf("root cluster: %d\r\n", fluxHeader.rootCluster);
	uint64_t currentCluster = fluxHeader.rootCluster;
	uint64_t maxFileEntries = fluxHeader.bytesPerCluster/sizeof(struct fluxfs_file_entry);
	while (currentCluster!=FLUXFS_CLUSTER_EOC&&currentCluster!=FLUXFS_CLUSTER_FREE){
		printf("cluster %d\r\n", currentCluster);
		if (fluxfs_read_cluster(pMount, currentCluster, &currentCluster)!=0)
			return -1;
	}	
	for (uint64_t i = 0;i<10;i++){
		uint64_t newCluster = 0;
		if (fluxfs_allocate_cluster(pMount, &newCluster)!=0)
			return -1;
		printf("new cluster: %d\r\n", newCluster);
	}
	return 0;
}
int fluxfs_delete(struct fluxfs_file_handle* pFileHandle){
	if (!pFileHandle)
		return -1;

	return 0;
}
int fluxfs_allocate_cluster(struct fluxfs_mount* pMount, uint64_t* pCluster){
	if (!pCluster)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	uint64_t newCluster = 0;
	if (fluxfs_read_free_stack(pMount,header.freeStackIndex ,&newCluster)!=0)
		return -1;
	header.freeStackIndex++;
	if (fluxfs_write_header(pMount, header)!=0)
		return -1;
	*pCluster = newCluster;
	return 0;
}
int fluxfs_free_cluster(struct fluxfs_mount* pMount, uint64_t cluster){
	if (!pMount)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	header.freeStackIndex--;
	if (fluxfs_write_free_stack(pMount, header.freeStackIndex, cluster)!=0)
		return -1;
	if (fluxfs_write_header(pMount, header)!=0)
		return -1;
	return 0;
}
int fluxfs_read_free_stack(struct fluxfs_mount* pMount, uint64_t entryIndex, uint64_t* pCluster){
	if (!pMount||!pCluster)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	uint64_t valueSector = header.freeStackSector+((entryIndex*sizeof(uint64_t))/DRIVE_SECTOR_SIZE);
	uint64_t valueOffset = (entryIndex*sizeof(uint64_t))%(DRIVE_SECTOR_SIZE*pMount->cacheInfo.cachedSectors);
	uint64_t sectorCacheRangeMin = pMount->cacheInfo.lastCachedSectorGroup;
	uint64_t sectorCacheRangeMax = pMount->cacheInfo.lastCachedSectorGroup+pMount->cacheInfo.cachedSectors;
	if (valueSector>=sectorCacheRangeMin&&valueSector<=sectorCacheRangeMax){
		valueOffset = (entryIndex*sizeof(uint64_t))%(DRIVE_SECTOR_SIZE*pMount->cacheInfo.cachedSectors);
		*pCluster = *(uint64_t*)(pMount->pSectorCache+valueOffset);
		return 0;
	}
	if (partition_read_sectors(pMount->drive_id, pMount->partition_id, valueSector, pMount->cacheInfo.cachedSectors, pMount->pSectorCache)!=0)
		return -1;
	pMount->cacheInfo.lastCachedSectorGroup = valueSector;
	*pCluster = *(uint64_t*)(pMount->pSectorCache+valueOffset);
	return 0;
}
int fluxfs_write_free_stack(struct fluxfs_mount* pMount, uint64_t entryIndex, uint64_t cluster){
	if (!pMount)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	uint64_t valueSector = header.freeStackSector+((entryIndex*sizeof(uint64_t))/DRIVE_SECTOR_SIZE);
	uint64_t valueOffset = (entryIndex*sizeof(uint64_t))%(DRIVE_SECTOR_SIZE*pMount->cacheInfo.cachedSectors);
	uint64_t sectorCacheRangeMin = pMount->cacheInfo.lastCachedSectorGroup;
	uint64_t sectorCacheRangeMax = pMount->cacheInfo.lastCachedSectorGroup+pMount->cacheInfo.cachedSectors;
	if (valueSector<sectorCacheRangeMin||valueSector>sectorCacheRangeMax){
		if (partition_read_sectors(pMount->drive_id, pMount->partition_id, valueSector, pMount->cacheInfo.cachedSectors, pMount->pSectorCache)!=0)
			return -1;
		pMount->cacheInfo.lastCachedSectorGroup = valueSector;
	}
	*(uint64_t*)(pMount->pSectorCache+valueOffset) = cluster;
	if (partition_write_sectors(pMount->drive_id, pMount->partition_id, valueSector, pMount->cacheInfo.cachedSectors, pMount->pSectorCache)!=0)
		return -1;
	return 0;
}
int fluxfs_read_cluster(struct fluxfs_mount* pMount, uint64_t cluster, uint64_t* pClusterValue){
	if (!pMount||!pClusterValue)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	uint64_t clusterValueSector = header.fatSector+((cluster*sizeof(uint64_t))/DRIVE_SECTOR_SIZE);
	uint64_t sectorCacheRangeMin = pMount->cacheInfo.lastCachedSectorGroup;
	uint64_t sectorCacheRangeMax = pMount->cacheInfo.lastCachedSectorGroup+pMount->cacheInfo.cachedSectors;
	if (clusterValueSector>=sectorCacheRangeMin&&clusterValueSector<=sectorCacheRangeMax){
		uint64_t valueOffset = (cluster*sizeof(uint64_t))%(DRIVE_SECTOR_SIZE*pMount->cacheInfo.cachedSectors);
		uint64_t clusterValue = *(uint64_t*)(pMount->pSectorCache+valueOffset);
		*pClusterValue = clusterValue;
		return 0;
	}
	uint64_t clusterValueOffset = (cluster*sizeof(uint64_t))%DRIVE_SECTOR_SIZE;
	if (partition_read_sectors(pMount->drive_id, pMount->partition_id, clusterValueSector, pMount->cacheInfo.cachedSectors, (unsigned char*)pMount->pSectorCache)!=0)
		return -1;
	pMount->cacheInfo.lastCachedSectorGroup = clusterValueSector;
	uint64_t clusterValue = *(uint64_t*)(pMount->pSectorCache+clusterValueOffset);
	*pClusterValue = clusterValue;
	return 0;
}
int fluxfs_write_cluster(struct fluxfs_mount* pMount, uint64_t cluster, uint64_t value){
	if (!pMount)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	uint64_t clusterValueSector = header.fatSector+((cluster*sizeof(uint64_t))/DRIVE_SECTOR_SIZE);
	uint64_t clusterValueOffset = (cluster*sizeof(uint64_t))%(DRIVE_SECTOR_SIZE*pMount->cacheInfo.cachedSectors);
	uint64_t sectorCacheRangeMin = pMount->cacheInfo.lastCachedSectorGroup;
	uint64_t sectorCacheRangeMax = pMount->cacheInfo.lastCachedSectorGroup+pMount->cacheInfo.cachedSectors;
	if (clusterValueSector<sectorCacheRangeMin||clusterValueSector>sectorCacheRangeMax){
		if (partition_read_sectors(pMount->drive_id, pMount->partition_id, clusterValueSector, pMount->cacheInfo.cachedSectors, (unsigned char*)pMount->pSectorCache)!=0)
			return -1;
		pMount->cacheInfo.lastCachedSectorGroup = clusterValueSector;
	}
	*(uint64_t*)(pMount->pSectorCache+clusterValueOffset) = value;
	if (partition_write_sectors(pMount->drive_id, pMount->partition_id, clusterValueSector, pMount->cacheInfo.cachedSectors, (unsigned char*)pMount->pSectorCache)!=0)
		return -1;
	return 0;
}
int fluxfs_verify(struct fluxfs_mount* pMount){
	if (!pMount)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	if (header.signature!=FLUXFS_SIGNATURE)
		return -1;
	uint32_t checksum = 0;
	if (fluxfs_get_header_checksum(header, &checksum)!=0)
		return -1;
	if (header.headerChecksum!=checksum)
		return -1;
	return 0;
}
int fluxfs_get_header(struct fluxfs_mount* pMount, struct fluxfs_header* pHeader){
	if (!pMount||!pHeader)
		return -1;
	if (pMount->cacheInfo.headerCached){
		*pHeader = pMount->fsHeader;
		return 0;
	}
	struct fluxfs_header header = {0};
	if (partition_read_sectors(pMount->drive_id, pMount->partition_id, 0, 1, (unsigned char*)&header)!=0)
		return -1;
	pMount->cacheInfo.headerCached = 1;
	pMount->fsHeader = header;
	*pHeader = header;
	return 0;
}
int fluxfs_get_header_checksum(struct fluxfs_header header, uint32_t* pChecksum){
	if (!pChecksum)
		return -1;
	header.headerChecksum = 0;
	header.freeStackIndex = 0;
	uint32_t checksum = 0;
	if (get_crc32((unsigned char*)&header, sizeof(struct fluxfs_header), &checksum)!=0)
		return -1;
	*pChecksum = checksum;
	return 0;
}
int fluxfs_set_header(struct fluxfs_mount* pMount, struct fluxfs_header header){
	if (!pMount)
		return -1;
	if (fluxfs_get_header_checksum(header, &header.headerChecksum)!=0)
		return -1;
	if (fluxfs_write_header(pMount, header)!=0)
		return -1;
	return 0;
}
int fluxfs_write_header(struct fluxfs_mount* pMount, struct fluxfs_header header){
	if (!pMount)
		return -1;
	if (partition_write_sectors(pMount->drive_id, pMount->partition_id, 0, 1, (unsigned char*)&header)!=0)
		return -1;
	pMount->cacheInfo.headerCached = 1;
	pMount->fsHeader = header;
	return 0;
}
int fluxfs_format(uint64_t drive_id, uint64_t partition_id){
	struct drive_info driveInfo = {0};
	if (drive_get_info(drive_id, &driveInfo)!=0)
		return -1;
	struct gpt_partition partition = {0};
	if (gpt_get_partition(drive_id, partition_id, &partition)!=0)
		return -1;
	uint64_t bytesPerCluster = 0x1000;
	uint64_t driveSize = driveInfo.sector_count*DRIVE_SECTOR_SIZE;
	uint64_t partitionSize = (partition.end_lba-partition.start_lba)*DRIVE_SECTOR_SIZE;
	uint64_t partitionClusters = partitionSize/bytesPerCluster;
	uint64_t freeStackSize = partitionClusters*sizeof(uint64_t);
	uint64_t fatSize = partitionClusters*sizeof(uint64_t);
	uint64_t sectorsPerFat = align_up(fatSize, DRIVE_SECTOR_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t sectorsPerFreeStack = align_up(freeStackSize, DRIVE_SECTOR_SIZE)/DRIVE_SECTOR_SIZE;
	struct fluxfs_header header = {0};
	uint32_t headerChecksum = 0;
	memset((void*)&header, 0, sizeof(struct fluxfs_header));
	header.signature = FLUXFS_SIGNATURE;
	header.freeStackSector = 1;
	header.fatSector = 1+sectorsPerFreeStack;
	header.sectorsPerFat = align_up(sectorsPerFat, 256);
	header.sectorsPerFreeStack = align_up(sectorsPerFreeStack, 256);
	header.bytesPerCluster = bytesPerCluster;
	header.partitionSize = partitionSize;
	header.rootCluster = 1;
	header.freeStackIndex = 0;
	uint64_t sectorGroupCached = 0xFFFFFFFFFFFFFFFF;
	uint64_t sectorsCached = MEM_MB/DRIVE_SECTOR_SIZE;
	uint64_t sectorCacheSize = DRIVE_SECTOR_SIZE*sectorsCached;
	unsigned char* pSectorData = (unsigned char*)0x0;
	if (virtualAlloc((uint64_t*)&pSectorData, sectorCacheSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	sectorGroupCached = 0xFFFFFFFFFFFFFFFF;	
	for (uint64_t i = 1;i<partitionClusters;i++){
		uint64_t sectorGroup = i/sectorCacheSize;
		uint64_t sectorIndex = i%sectorCacheSize;
		if (sectorGroup!=sectorGroupCached){
			if (partition_write_sectors(drive_id, partition_id, header.freeStackSector+(sectorGroup*sectorsCached), sectorsCached, pSectorData)!=0)
				return -1;
			sectorGroupCached = sectorGroup;
		}
		*(((uint64_t*)pSectorData)+sectorIndex) = i;
	}
	memset((void*)pSectorData, 0, sectorCacheSize);
	*(uint64_t*)pSectorData = 1;
	for (uint64_t i = 0;i<sectorsPerFat/sectorsCached;i++){
		if (partition_write_sectors(drive_id, partition_id, header.fatSector+(i*sectorsCached), sectorsCached, pSectorData)!=0)
			return -1;
		*(uint64_t*)pSectorData = 0;
	}
	if (virtualFree((uint64_t)pSectorData, sectorCacheSize)!=0)
		return -1;
	if (fluxfs_get_header_checksum(header, &header.headerChecksum)!=0)
		return -1;
	if (partition_write_sectors(drive_id, partition_id, 0, 1, (unsigned char*)&header)!=0)
		return -1;
	return 0;
}
int fluxfs_subsystem_mount(uint64_t drive_id, uint64_t partition_id, struct fs_mount* pMount){
	if (!pMount)
		return -1;
	struct fluxfs_mount* pFluxMount = (struct fluxfs_mount*)0x0;
	if (fluxfs_mount(drive_id, partition_id, &pFluxMount)!=0)
		return -1;
	pMount->drive_id = drive_id;
	pMount->partition_id = partition_id;
	pMount->pMountData = (void*)pFluxMount;
	return 0;
}
int fluxfs_subsystem_unmount(struct fs_mount* pMount){
	if (!pMount)
		return -1;
	struct fluxfs_mount* pFluxMount = (struct fluxfs_mount*)pMount->pMountData;
	if (!pFluxMount)
		return -1;
	if (fluxfs_unmount(pFluxMount)!=0)
		return -1;
	return 0;
}
int fluxfs_subsystem_open(struct fs_mount* pMount, unsigned char* filename, void** ppHandle){
	if (!pMount||!filename||!ppHandle)
		return -1;
	struct fluxfs_mount* pFluxMount = (struct fluxfs_mount*)pMount->pMountData;
	struct fluxfs_file_handle* pFileHandle = (struct fluxfs_file_handle*)0x0;
	if (fluxfs_open(pFluxMount, filename, &pFileHandle)!=0)
		return -1;
	*ppHandle = (void*)pFileHandle;
	return 0;
}
int fluxfs_subsystem_close(void* pHandle){
	if (!pHandle)
		return -1;
	if (fluxfs_close((struct fluxfs_file_handle*)pHandle)!=0)
		return -1;
	return 0;
}
int fluxfs_subsystem_create(struct fs_mount* pMount, unsigned char* filename, uint64_t fileAttribs){
	if (!pMount||!filename)
		return -1;
	struct fluxfs_mount* pFluxMount = (struct fluxfs_mount*)pMount->pMountData;
	if (fluxfs_create(pFluxMount, filename, fileAttribs)!=0)
		return -1;
	return 0;
}
int fluxfs_subsystem_delete(struct fs_mount* pMount, void* pHandle){
	if (!pMount||!pHandle)
		return -1;
	if (fluxfs_delete((struct fluxfs_file_handle*)pHandle)!=0)
		return -1;
	return 0;
}
int fluxfs_subsystem_verify(struct fs_mount* pMount){
	if (!pMount)
		return -1;
	struct fluxfs_mount* pFluxMount = (struct fluxfs_mount*)pMount->pMountData;
	if (!pFluxMount)
		return -1;
	if (fluxfs_verify(pFluxMount)!=0)
		return -1;
	return 0;
}
