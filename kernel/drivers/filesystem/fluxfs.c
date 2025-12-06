#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/gpt.h"
#include "subsystem/filesystem.h"
#include "drivers/filesystem/fluxfs.h"
int fluxfs_init(void){


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
	return 0;
}
int fluxfs_unmount(struct fluxfs_mount* pMount){
	if (!pMount)
		return -1;
	if (kfree((void*)pMount)!=0)
		return -1;
	return 0;
}
int fluxfs_open(struct fluxfs_mount* pMount, unsigned char* filename, struct fluxfs_file_handle* pFileHandle){
	if (!pMount||!filename||!pFileHandle)
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
int fluxfs_verify(struct fluxfs_mount* pMount){
	if (!pMount)
		return -1;
	struct fluxfs_header header = {0};
	if (fluxfs_get_header(pMount, &header)!=0)
		return -1;
	if (header.signature!=FLUXFS_SIGNATURE)
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
	*pHeader = header;
	return 0;
}
