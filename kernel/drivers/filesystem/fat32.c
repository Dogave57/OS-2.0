#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/drive.h"
#include "subsystem/filesystem.h"
#include "align.h"
#include "drivers/gpt.h"
#include "drivers/timer.h"
#include "drivers/filesystem/fat32.h"
struct fs_driver_desc* pDriverDesc = (struct fs_driver_desc*)0x0;
int fat32_init(void){
	struct fs_driver_vtable vtable = {0};
	vtable.verify = (fsVerifyFunc)fat32_subsystem_verify;
	vtable.mount = (fsMountFunc)fat32_subsystem_mount;
	vtable.unmount = (fsUnmountFunc)fat32_subsystem_unmount;
	vtable.open = (fsOpenFunc)fat32_subsystem_open;
	vtable.close = (fsCloseFunc)fat32_subsystem_close;
	vtable.read = (fsReadFunc)fat32_subsystem_read;
	vtable.write = (fsWriteFunc)fat32_subsystem_write;
	vtable.getFileInfo = (fsGetFileInfoFunc)fat32_subsystem_getFileInfo;
	vtable.create = (fsCreateFunc)fat32_subsystem_create_file;
	vtable.delete = (fsDeleteFunc)fat32_subsystem_delete_file;
	if (fs_driver_register(vtable, &pDriverDesc)!=0)
		return -1;
	return 0;
}
int fat32_mount(uint64_t drive_id, uint64_t partition_id, struct fat32_mount_handle** ppHandle){
	if (!ppHandle)
		return -1;
	struct fat32_mount_handle* pHandle = (struct fat32_mount_handle*)kmalloc(sizeof(struct fat32_mount_handle));
	if (!pHandle)
		return -1;
	memset((void*)pHandle, 0, sizeof(struct fat32_mount_handle));
	pHandle->drive_id = drive_id;
	pHandle->partition_id = partition_id;
	if (fat32_verify(pHandle)!=0)
		return -1;
	*ppHandle = pHandle;
	return 0;
}
int fat32_unmount(struct fat32_mount_handle* pHandle){
	if (!pHandle)
		return -1;
	kfree((void*)pHandle);
	return 0;
}
int fat32_opendir(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_dir_handle** ppDirHandle){
	if (!pMountHandle||!filename||!ppDirHandle)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint32_t firstDirCluster = bpb.ebr.root_cluster_number;
	if (filename[0]!='/'||filename[1]){
		struct fat32_file_entry fileEntry = {0};
		struct fat32_file_location fileLocation = {0};	
		if (fat32_find_file(pMountHandle, filename, &fileEntry, &fileLocation, 0)!=0)
			return -1;
		firstDirCluster = ((uint32_t)fileEntry.entry_first_cluster_high)|((uint32_t)fileEntry.entry_first_cluster_low);
	}
	struct fat32_dir_handle* pDirHandle = (struct fat32_dir_handle*)kmalloc(sizeof(struct fat32_dir_handle));
	if (!pDirHandle)
		return -1;
	memset((void*)pDirHandle, 0, sizeof(struct fat32_dir_handle));
	pDirHandle->desc.firstDirCluster = firstDirCluster;
	pDirHandle->currentDirCluster.cluster_value = firstDirCluster;
	pDirHandle->pMountHandle = pMountHandle;
	*ppDirHandle = pDirHandle;
	return 0;
}
int fat32_closedir(struct fat32_dir_handle* pDirHandle){
	if (!pDirHandle)
		return -1;
	kfree((void*)pDirHandle);
	return 0;
}
int fat32_read_dir(struct fat32_dir_handle* pDirHandle, struct fat32_simple_file_entry* pFileEntry){
	if (!pDirHandle||!pFileEntry)
		return -1;
	uint64_t bytesPerCluster = 0;
	struct fat32_mount_handle* pMountHandle = pDirHandle->pMountHandle;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytesPerCluster)!=0)
		return -1;
	uint64_t maxFileEntries = bytesPerCluster/sizeof(struct fat32_file_entry);
	if (pDirHandle->currentDirIndex>=maxFileEntries){
		if (fat32_readcluster(pMountHandle, pDirHandle->currentDirCluster.cluster_value, &pDirHandle->currentDirCluster)!=0)
			return -1;
		if (FAT32_CLUSTER_END_OF_CHAIN(pDirHandle->currentDirCluster.cluster_value))
			return -1;
		pDirHandle->currentDirIndex = 0;
	}
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pMountHandle, pDirHandle->currentDirCluster.cluster_value, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	struct fat32_file_entry* pCurrentFileEntry = ((struct fat32_file_entry*)pClusterData)+pDirHandle->currentDirIndex;
	if (!pCurrentFileEntry->filename[0]||pCurrentFileEntry->filename[0]==' '){
		pDirHandle->currentDirIndex++;
		return fat32_read_dir(pDirHandle, pFileEntry);
	}
	if (fat32_get_simple_file_entry(*pCurrentFileEntry, pFileEntry)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	kfree((void*)pClusterData);
	pDirHandle->currentDirIndex++;
	return 0;
}
int fat32_dir_rewind(struct fat32_dir_handle* pDirHandle){
	if (!pDirHandle)
		return -1;
	pDirHandle->currentDirCluster.cluster_value = pDirHandle->desc.firstDirCluster;
	pDirHandle->currentDirIndex = 0;
	return 0;
}
int fat32_openfile(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_file_handle** ppFileHandle){
	if (!pMountHandle||!ppFileHandle)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	struct fat32_file_location fileLocation = {0};
	struct fat32_file_entry fileEntry = {0};
	if (fat32_find_file(pMountHandle, filename, &fileEntry, &fileLocation,0)!=0){
		printf(L"failed to find file\r\n");
		return -1;
	}
	struct fat32_file_handle* pFileHandle = (struct fat32_file_handle*)kmalloc(sizeof(struct fat32_file_handle));
	if (!pFileHandle)
		return -1;
	memset((void*)pFileHandle, 0, sizeof(struct fat32_file_handle));
	pFileHandle->fileLocation = fileLocation;
	pFileHandle->fileEntry = fileEntry;
	pFileHandle->pMountHandle = pMountHandle;
	*ppFileHandle = pFileHandle;
	return 0;
}
int fat32_closefile(struct fat32_file_handle* pFileHandle){
	if (!pFileHandle)
		return -1;
	kfree((void*)pFileHandle);
	return 0;
}
int fat32_get_file_size(struct fat32_file_handle* pFileHandle, uint32_t* pFileSize){
	if (!pFileHandle||!pFileSize)
		return -1;
	*pFileSize = pFileHandle->fileEntry.file_size;
	return 0;
}
int fat32_get_file_attributes(struct fat32_file_handle* pFileHandle, uint8_t* pFileAttributes){
	if (!pFileHandle||!pFileAttributes)
		return -1;
	*pFileAttributes = pFileHandle->fileEntry.file_attribs;
	return 0;
}
int fat32_renamefile(struct fat32_file_handle* pFileHandle, unsigned char* filename){
	if (!pFileHandle||!filename)
		return -1;
	uint64_t bytesPerCluster = 0;
	struct fat32_mount_handle* pMountHandle = pFileHandle->pMountHandle;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytesPerCluster)!=0)
		return -1;
	struct fat32_file_location* pFileLocation = &pFileHandle->fileLocation;
	struct fat32_file_entry* pFileEntry = &pFileHandle->fileEntry;
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pMountHandle, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	struct fat32_file_entry* pNewFileEntry = ((struct fat32_file_entry*)pClusterData)+pFileLocation->fileIndex;
	if (fat32_string_to_filename(pNewFileEntry->filename, filename)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	if (fat32_write_cluster_data(pMountHandle, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	kfree((void*)pClusterData);
	return 0;
}
int fat32_readfile(struct fat32_file_handle* pFileHandle, unsigned char* pFileBuffer, uint32_t size){
	if (!pFileHandle||!pFileBuffer)
		return -1;
	uint64_t bytesPerCluster = 0;
	struct fat32_file_entry fileEntry = pFileHandle->fileEntry;
	struct fat32_file_location fileLocation = pFileHandle->fileLocation;
	struct fat32_mount_handle* pMountHandle = pFileHandle->pMountHandle;
	if (!fileEntry.file_size)
		return -1;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytesPerCluster)!=0)
		return -1;
	if (size>fileEntry.file_size)
		return -1;
	if (!size)
		size = fileEntry.file_size;
	uint32_t firstDataCluster = ((uint32_t)fileEntry.entry_first_cluster_high)|fileEntry.entry_first_cluster_low;
	uint64_t clusterCount = align_up(size, bytesPerCluster)/bytesPerCluster;
	struct fat32_cluster_entry currentCluster = {0};
	currentCluster.cluster_value = firstDataCluster;
	uint64_t sizeAligned = !(size%bytesPerCluster);
	for (uint64_t i = 0;i<clusterCount;i++){
		if (!FAT32_CONTINUE_CLUSTER(currentCluster.cluster_value))
			return -1;
		if (!sizeAligned&&i==clusterCount-1){
			uint64_t alignOffset = size%bytesPerCluster;
			unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
			if (!pClusterData)
				return -1;
			if (fat32_read_cluster_data(pMountHandle, currentCluster.cluster_value, pClusterData)!=0){
				kfree((void*)pClusterData);
				return -1;
			}
			memcpy((unsigned char*)(pFileBuffer+(i*bytesPerCluster)), (unsigned char*)pClusterData, alignOffset);
			kfree((void*)pClusterData);
			if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &currentCluster)!=0)
				return -1;
			continue;
		}
		unsigned char* pClusterData = pFileBuffer+(i*bytesPerCluster);
		if (fat32_read_cluster_data(pMountHandle, currentCluster.cluster_value, pClusterData)!=0)
			return -1;
		if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &currentCluster)!=0)
			return -1;
	}
	return 0;
}
int fat32_writefile(struct fat32_file_handle* pFileHandle, unsigned char* pFileBuffer, uint32_t size){
	if (!pFileHandle||!pFileBuffer)
		return -1;
	if (pFileHandle->fileEntry.file_attribs&FAT32_FILE_ATTRIBUTE_READONLY)
		return -1;
	uint64_t bytesPerCluster = 0;
	struct fat32_mount_handle* pMountHandle = pFileHandle->pMountHandle;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytesPerCluster)!=0)
		return -1;
	struct fat32_file_entry* pFileEntry = &pFileHandle->fileEntry;
	struct fat32_file_location* pFileLocation = &pFileHandle->fileLocation;
	uint64_t clustersNeeded = align_up(size, bytesPerCluster)/bytesPerCluster;
	uint64_t fileClusters = 0;
	if (pFileEntry->file_size)
		fileClusters = align_up(pFileEntry->file_size, bytesPerCluster)/bytesPerCluster;
	if (!fileClusters){
		uint32_t newCluster = 0;
		if (fat32_allocate_cluster(pMountHandle, &newCluster)!=0)
			return -1;
		fileClusters++;
		pFileEntry->entry_first_cluster_low = (uint16_t)(newCluster&0xFFFF);
		pFileEntry->entry_first_cluster_high = (uint16_t)((newCluster>>16)&0xFFFF);
	}
	struct fat32_cluster_entry firstDataCluster = {0};
	firstDataCluster.cluster_value = (((uint32_t)pFileEntry->entry_first_cluster_high)<<16)|((uint32_t)pFileEntry->entry_first_cluster_low);
	struct fat32_cluster_entry lastClusterEntry = {0};
	lastClusterEntry = firstDataCluster;
	while (FAT32_CONTINUE_CLUSTER(lastClusterEntry.cluster_value)){
		if (fat32_readcluster(pMountHandle, lastClusterEntry.cluster_value, &lastClusterEntry)!=0)
			break;
	}
	uint64_t clustersToAlloc = 0;
	if (fileClusters<clustersNeeded)
		clustersToAlloc = clustersNeeded-fileClusters;
	struct fat32_cluster_entry currentCluster = {0};
	currentCluster = lastClusterEntry;
	for (uint64_t i = 0;i<clustersToAlloc;i++){
		struct fat32_cluster_entry newCluster = {0};
		if (fat32_allocate_cluster(pMountHandle, (uint32_t*)&newCluster)!=0)
			return -1;
		if (fat32_writecluster(pMountHandle, currentCluster.cluster_value, newCluster)!=0)
			return -1;
		currentCluster = newCluster;
		lastClusterEntry = currentCluster;
		fileClusters++;
	}
	uint64_t clustersToFree = 0;
	if (fileClusters>clustersNeeded)
		clustersToFree = fileClusters-clustersNeeded;
	currentCluster = firstDataCluster;
	for (uint64_t i = 0;i<fileClusters&&clustersToFree;i++){
		if (i<fileClusters-clustersToFree){
			if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &currentCluster)!=0){
				printf(L"failed to read next cluster\r\n");
				return -1;
			}
			continue;
		}
		struct fat32_cluster_entry nextCluster = {0};
		if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &nextCluster)!=0){
			printf(L"failed to read next cluster\r\n");
			return -1;
		}
		if (fat32_free_cluster(pMountHandle, currentCluster.cluster_value)!=0){
			printf(L"failed to free cluster\r\n");
			return -1;	
		}
		currentCluster = nextCluster;
	}	
	fileClusters = fileClusters-clustersToFree;
	currentCluster = firstDataCluster;
	uint64_t isAligned = !(size%bytesPerCluster);
	uint64_t alignOffset = size%bytesPerCluster;
	for (uint64_t i = 0;i<clustersNeeded;i++){
		if (!isAligned&&i==clustersNeeded-1){
			unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
			if (!pClusterData)
				return -1;
			memcpy((unsigned char*)pClusterData, (pFileBuffer+(i*bytesPerCluster)), alignOffset);
			if (fat32_write_cluster_data(pMountHandle, currentCluster.cluster_value, pClusterData)!=0){
				printf(L"failed to write last misaligned cluster\r\n");
				return -1;
			}
			kfree((void*)pClusterData);
			fat32_readcluster(pMountHandle, currentCluster.cluster_value, &currentCluster);
			continue;
		}
		unsigned char* pClusterData = pFileBuffer+(i*bytesPerCluster);
		if (fat32_write_cluster_data(pMountHandle, currentCluster.cluster_value, pClusterData)!=0){
			printf(L"failed to write cluster %d with id %d\r\n", i, currentCluster.cluster_value);
			return -1;
		}
		if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &currentCluster)!=0&&i!=fileClusters-1){
			printf(L"failed to read next cluster\r\n");
			return -1;
		}
	}
	pFileEntry->file_size = size;
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pMountHandle, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	struct fat32_file_entry* pNewFileEntry = ((struct fat32_file_entry*)pClusterData)+pFileLocation->fileIndex;
	*pNewFileEntry = pFileHandle->fileEntry;
	uint32_t entry = ((uint32_t)pFileHandle->fileEntry.entry_first_cluster_high<<16)|((uint32_t)pFileHandle->fileEntry.entry_first_cluster_low);
	if (fat32_write_cluster_data(pMountHandle, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	kfree((void*)pClusterData);	
	return 0;
}
int fat32_createfile(struct fat32_mount_handle* pMountHandle, unsigned char* filename, uint8_t file_attribs){
	if (!pMountHandle||!filename)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t bytesPerCluster = 0;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytesPerCluster)!=0)
		return -1;
	struct fat32_file_location dirLocation = {0};
	struct fat32_file_entry dirEntry = {0};
	uint64_t pathstart = 0;
	dirLocation.fileCluster = bpb.ebr.root_cluster_number;
	for (uint64_t i = 0;filename[i];i++){
		if (filename[i]!='/')
			continue;
		if (!filename[i+1])
			continue;
		filename[i] = 0;
		if (fat32_find_file_in_dir(pMountHandle, dirLocation.fileCluster, filename+pathstart, &dirEntry, &dirLocation, FAT32_FILE_ATTRIBUTE_DIRECTORY)!=0){
			return -1;
		}
		filename[i] = '/';
		pathstart = i+1;
	}
	struct fat32_cluster_entry firstDataCluster = {0};
	struct fat32_cluster_entry currentCluster = {0};
	firstDataCluster.cluster_value = bpb.ebr.root_cluster_number;
	if (dirLocation.fileCluster!=bpb.ebr.root_cluster_number){
		unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
		if (!pClusterData)
			return -1;
		if (fat32_read_cluster_data(pMountHandle, dirLocation.fileCluster, pClusterData)!=0){
			kfree((void*)pClusterData);
			return -1;
		}
		struct fat32_file_entry* pFileEntry = ((struct fat32_file_entry*)pClusterData)+dirLocation.fileIndex;
		firstDataCluster.cluster_value = ((uint32_t)pFileEntry->entry_first_cluster_high)|((uint32_t)pFileEntry->entry_first_cluster_low);
		kfree((void*)pClusterData);
	}
	currentCluster = firstDataCluster;
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	uint64_t max_file_entries = bytesPerCluster/sizeof(struct fat32_file_entry);
	while (FAT32_CONTINUE_CLUSTER(currentCluster.cluster_value)){
		if (fat32_read_cluster_data(pMountHandle, currentCluster.cluster_value, pClusterData)!=0)
			return -1;
		for (uint64_t i = 0;i<max_file_entries;i++){
			struct fat32_file_entry* pFileEntry = ((struct fat32_file_entry*)pClusterData)+i;
			if (pFileEntry->filename[0])
				continue;
			if (fat32_string_to_filename(pFileEntry->filename, filename+pathstart)!=0)
				continue;
			pFileEntry->file_size = 0;
			pFileEntry->file_attribs = file_attribs;
			if (fat32_write_cluster_data(pMountHandle, currentCluster.cluster_value, pClusterData)!=0){
				kfree((void*)pClusterData);
				return -1;
			}
			kfree((void*)pClusterData);
			return 0;
		}
		if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &currentCluster)!=0)
			return -1;
	}
	kfree((void*)pClusterData);
	return -1;
}
int fat32_deletefile(struct fat32_file_handle* pFileHandle){
	if (!pFileHandle)
		return -1;
	struct fat32_mount_handle* pMountHandle = pFileHandle->pMountHandle;
	uint64_t bytesPerCluster = 0;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytesPerCluster)!=0)
		return -1;
	struct fat32_file_entry* pFileEntry = &pFileHandle->fileEntry;
	struct fat32_file_location* pFileLocation = &pFileHandle->fileLocation;
	struct fat32_cluster_entry firstDataCluster = {0};
	firstDataCluster.cluster_value = (((uint32_t)pFileEntry->entry_first_cluster_high)<<16)|((uint32_t)pFileEntry->entry_first_cluster_low);
	struct fat32_cluster_entry currentCluster = firstDataCluster;
	uint64_t fileClusters = 0;
	if (pFileEntry->file_size)
		fileClusters = align_up(pFileEntry->file_size, bytesPerCluster)/bytesPerCluster;
	for (uint64_t i = 0;i<fileClusters;i++){
		if (!FAT32_CONTINUE_CLUSTER(currentCluster.cluster_value))
			return -1;

		struct fat32_cluster_entry nextCluster = {0};
		if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &nextCluster)!=0)
			return -1;
		if (fat32_free_cluster(pMountHandle, currentCluster.cluster_value)!=0)
			return -1;
		currentCluster = nextCluster;
	}
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pMountHandle, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	struct fat32_file_entry* pNewFileEntry = ((struct fat32_file_entry*)pClusterData)+pFileLocation->fileIndex;
	memset((void*)pNewFileEntry, 0, sizeof(struct fat32_file_entry));
	if (fat32_write_cluster_data(pMountHandle, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	kfree((void*)pClusterData);
	return 0;
}
int fat32_get_simple_file_entry(struct fat32_file_entry fileEntry, struct fat32_simple_file_entry* pSimpleFileEntry){
	if (!pSimpleFileEntry)
		return -1;
	struct fat32_simple_file_entry simpleFileEntry = {0};
	simpleFileEntry.fileSize = fileEntry.file_size;
	simpleFileEntry.fileAttribs = fileEntry.file_attribs;
	if (fat32_filename_to_string(simpleFileEntry.filename, fileEntry.filename)!=0)
		return -1;
	*pSimpleFileEntry = simpleFileEntry;
	return 0;
}
int fat32_filename_to_string(unsigned char* dest, unsigned char* src){
	if (!dest||!src)
		return -1;
	uint64_t index = 0;
	for (uint64_t i = 0;i<FAT32_FILENAME_LEN_MAX;i++){
		unsigned char ch = src[i];
		if (ch==' '&&i==7&&src[8]!=' '){
			dest[index] = '.';
			dest[index+1] = src[8];
			dest[index+2] = src[9];
			dest[index+3] = src[10];
			break;
		}
		if (ch==' ')
			continue;
		dest[index] = ch;
		index++;
	}
	dest[11] = 0;
	return 0;
}
int fat32_string_to_filename(unsigned char* dest, unsigned char* src){
	if (!dest||!src)
		return -1;
	for (uint64_t i = 0;i<FAT32_FILENAME_LEN_MAX;i++){
		dest[i] = ' ';
	}
	for (uint64_t i = 0;src[i]&&i<FAT32_FILENAME_LEN_MAX;i++){
		unsigned char ch = src[i];
		if (ch=='.'){
			if (src[i+1])
				dest[8] = src[i+1];
			else
				return -1;
			if (src[i+2])
				dest[9] = src[i+2];
			else
				return 0;
			if (src[i+3])
				dest[10] = src[i+3];
			else
				return 0;
			return 0;
		}
		dest[i] = src[i];
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
		if (!ch)
			break;
		if (ch=='.'){
			if (fileEntry.filename[8]!=filename[i+1]||fileEntry.filename[9]!=filename[i+2]||fileEntry.filename[10]!=filename[i+3])
				return -1;
			return 0;
		}
		if (ch!=fileEntry.filename[i]){
			return -1;
		}
		if (i==sizeof(fileEntry.filename)-1&&filename[i+1])
			return -1;
	}
	return 0;
}
int fat32_find_file(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_file_entry* pFileEntry, struct fat32_file_location* pFileLocation, uint8_t file_attribs){
	if (!pMountHandle||!filename||!pFileLocation||!pFileEntry)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint32_t root_cluster = bpb.ebr.root_cluster_number;
	uint64_t pathstart = 0;
	struct fat32_file_location fileLocation = {0};
	struct fat32_file_location lastFileLocation = {0};
	struct fat32_file_entry fileEntry = {0};
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
		lastFileLocation = fileLocation;
		if (fat32_find_file_in_dir(pMountHandle, fileLocation.fileDataCluster, filename+pathstart, &fileEntry, &fileLocation, mask)!=0){
			printf_ascii("failed to find %s in cluster %d\r\n", filename+pathstart, fileLocation.fileDataCluster);
			return -1;
		}
		if (ch)
			filename[i] = '/';
		pathstart = i+1;
		if (!ch)
			break;
	}
	*pFileLocation = fileLocation;
	*pFileEntry = fileEntry;
	return 0;
}
int fat32_find_file_in_dir(struct fat32_mount_handle* pMountHandle, uint32_t dirCluster, unsigned char* filename, struct fat32_file_entry* pFileEntry, struct fat32_file_location* pFileLocation, uint8_t file_attrib){
	if (!pMountHandle||!filename||!pFileLocation||!pFileEntry)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytes_per_cluster)!=0)
		return -1;
	uint64_t max_dir_entries = bytes_per_cluster/sizeof(struct fat32_file_entry);
	struct fat32_cluster_entry currentCluster = {0};
	currentCluster.cluster_value = dirCluster;
	uint64_t clusterDataPages = align_up(bytes_per_cluster, PAGE_SIZE)/PAGE_SIZE;
	struct fat32_file_entry* pClusterData = (struct fat32_file_entry*)0x0;
	if (virtualAllocPages((uint64_t*)&pClusterData, clusterDataPages, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	while (FAT32_CONTINUE_CLUSTER(currentCluster.cluster_value)){
		if (fat32_read_cluster_data(pMountHandle, currentCluster.cluster_value, (unsigned char*)pClusterData)!=0){
			break;
		}
		for (uint64_t file_index = 0;file_index<max_dir_entries;file_index++){
			struct fat32_file_entry* pCurrentFileEntry = pClusterData+file_index;
			if (fat32_file_entry_namecmp(*pCurrentFileEntry, filename)!=0)
				continue;
			if (file_attrib!=0&&!(pCurrentFileEntry->file_attribs&file_attrib)){
				continue;
			}
			if (pCurrentFileEntry->file_attribs&FAT32_FILE_ATTRIBUTE_LFN)
				continue;
			struct fat32_file_location fileLocation = {0};
			fileLocation.fileCluster = currentCluster.cluster_value;
			fileLocation.fileIndex = file_index;
			fileLocation.fileDataCluster = (((uint32_t)pCurrentFileEntry->entry_first_cluster_high)<<16)|(uint32_t)pCurrentFileEntry->entry_first_cluster_low;
			*pFileLocation = fileLocation;
			*pFileEntry = *pCurrentFileEntry;
			virtualFreePages((uint64_t)pClusterData, clusterDataPages);
			return 0;
		}
		if (fat32_readcluster(pMountHandle, currentCluster.cluster_value, &currentCluster)!=0){
			break;
		}
	}
	virtualFreePages((uint64_t)pClusterData, clusterDataPages);
	return -1;
}
int fat32_find_file_in_root(struct fat32_mount_handle* pMountHandle, unsigned char* filename, struct fat32_file_entry* pFileEntry,struct fat32_file_location* pFileLocation, uint8_t file_attribs){
	if (!pMountHandle||!filename||!pFileLocation||!pFileEntry)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint32_t rootDirCluster = bpb.ebr.root_cluster_number;
	if (fat32_find_file_in_dir(pMountHandle, rootDirCluster, filename, pFileEntry, pFileLocation, file_attribs)!=0)
		return -1;
	return 0;
}
int fat32_get_free_cluster(struct fat32_mount_handle* pMountHandle, uint32_t* pFreeCluster, uint64_t start_cluster){
	if (!pMountHandle||!pFreeCluster)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	if (!start_cluster){
		start_cluster = fsinfo.start_lookup_cluster;
	}
	for (uint32_t i = start_cluster;;){
		struct fat32_cluster_entry clusterEntry = {0};
		if (fat32_readcluster(pMountHandle, i, &clusterEntry)!=0)
			return -1;
		if (clusterEntry.cluster_value==FAT32_CLUSTER_FREE){
			*pFreeCluster = i;
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
int fat32_free_cluster(struct fat32_mount_handle* pMountHandle, uint32_t freeCluster){
	if (!pMountHandle)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	struct fat32_cluster_entry newClusterEntry = {0};
	newClusterEntry.cluster_value = FAT32_CLUSTER_FREE;
	if (fat32_writecluster(pMountHandle, freeCluster, newClusterEntry)!=0)
		return -1;
	return 0;
}
int fat32_allocate_cluster(struct fat32_mount_handle* pMountHandle, uint32_t* pNewCluster){
	if (!pMountHandle||!pNewCluster)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	uint32_t newCluster = 0;
	if (fat32_get_free_cluster(pMountHandle, &newCluster, 0)!=0){
		printf(L"failed to get free cluster\r\n");
		return -1;
	}
	struct fat32_cluster_entry newEntry = {0};
	newEntry.cluster_value = newCluster+1;
	if (fat32_writecluster(pMountHandle, newCluster, newEntry)!=0){
		printf(L"failed to write to cluster\r\n");
		return -1;
	}
	*pNewCluster = newCluster;
	return 0;
}
int fat32_readcluster(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, struct fat32_cluster_entry* pClusterEntry){
	if (!pMountHandle||!pClusterEntry)
		return -1;
	uint64_t fatSector = 0;
	if (fat32_get_fat(pMountHandle, &fatSector)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytes_per_cluster)!=0)
		return -1;
	uint64_t cluster_byte_offset = cluster_id*sizeof(uint32_t);
	uint64_t cluster_sector_offset = cluster_byte_offset/DRIVE_SECTOR_SIZE;
	uint64_t cluster_offset = cluster_byte_offset%DRIVE_SECTOR_SIZE;
	uint64_t cluster_sector = fatSector+cluster_sector_offset;
	unsigned char* pSectorData = (unsigned char*)pMountHandle->cacheInfo.lastSectorCache;
	uint64_t cached_last_sector = 0;
	fat32_get_cached_last_sector(pMountHandle, &cached_last_sector);
	if (cached_last_sector==cluster_sector){
		*(uint32_t*)pClusterEntry = *(uint32_t*)(pMountHandle->cacheInfo.lastSectorCache+cluster_offset);
		return 0;
	}
	if (fat32_cache_last_sector(pMountHandle, cluster_sector)!=0)
		return -1;
	if (partition_read_sectors(pMountHandle->drive_id, pMountHandle->partition_id, cluster_sector, 1, (unsigned char*)pSectorData)!=0)
		return -1;
	uint32_t cluster_value = *(uint32_t*)(pMountHandle->cacheInfo.lastSectorCache+cluster_offset);
	*(uint32_t*)pClusterEntry = cluster_value;
	return 0;
}
int fat32_writecluster(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, struct fat32_cluster_entry clusterEntry){
	if (!pMountHandle)
		return -1;
	uint64_t fatSector = 0;
	uint64_t backupFatSector = 0;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	if (fat32_get_fat(pMountHandle, &fatSector)!=0)
		return -1;
	if (fat32_get_backup_fat(pMountHandle, &backupFatSector)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytes_per_cluster)!=0)
		return -1;
	uint64_t cluster_byte_offset = cluster_id*sizeof(uint32_t);
	uint64_t cluster_sector_offset = cluster_byte_offset/DRIVE_SECTOR_SIZE;
	uint64_t cluster_offset = cluster_byte_offset%DRIVE_SECTOR_SIZE;
	uint64_t cluster_sector = fatSector+cluster_sector_offset;
	uint64_t cluster_backup_sector = backupFatSector+cluster_sector_offset;
	unsigned char sectorData[512] = {0};
	if (partition_read_sectors(pMountHandle->drive_id, pMountHandle->partition_id, cluster_sector, 1, (unsigned char*)sectorData)!=0)
		return -1;
	*(uint32_t*)(sectorData+cluster_offset) = *(uint32_t*)(&clusterEntry);
	if (partition_write_sectors(pMountHandle->drive_id, pMountHandle->partition_id, cluster_sector, 1, (unsigned char*)sectorData)!=0)
		return -1;
	if (partition_write_sectors(pMountHandle->drive_id, pMountHandle->partition_id, cluster_backup_sector, 1, (unsigned char*)sectorData)!=0)
		return -1;
	uint64_t cached_last_sector = 0;
	if (fat32_get_cached_last_sector(pMountHandle, &cached_last_sector)!=0)
		return 0;
	if (cluster_sector!=cached_last_sector)
		return 0;
	*(uint32_t*)(pMountHandle->cacheInfo.lastSectorCache+cluster_offset) = *(uint32_t*)(&clusterEntry);
	return 0;
}
int fat32_read_cluster_data(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, unsigned char* pClusterData){
	if (!pMountHandle||!pClusterData)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytes_per_cluster)!=0)
		return -1;
	uint64_t sectors_per_cluster = bytes_per_cluster/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_start_sector = 0;
	if (fat32_get_cluster_data_sector(pMountHandle, &cluster_data_start_sector)!=0)
		return -1;
	uint64_t cluster_sector_offset = ((cluster_id-2)*bytes_per_cluster)/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_sector = cluster_data_start_sector+cluster_sector_offset;
	if (partition_read_sectors(pMountHandle->drive_id, pMountHandle->partition_id, cluster_data_sector, sectors_per_cluster, pClusterData)!=0)
		return -1;
	return 0;
}
int fat32_write_cluster_data(struct fat32_mount_handle* pMountHandle, uint32_t cluster_id, unsigned char* pClusterData){
	if (!pMountHandle||!pClusterData)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	uint64_t bytes_per_cluster = 0;
	if (fat32_get_bytes_per_cluster(pMountHandle, &bytes_per_cluster)!=0)
		return -1;
	uint64_t sectors_per_cluster = bytes_per_cluster/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_start_sector = 0;
	if (fat32_get_cluster_data_sector(pMountHandle, &cluster_data_start_sector)!=0)
		return -1;
	uint64_t cluster_sector_offset = ((cluster_id-2)*bytes_per_cluster)/DRIVE_SECTOR_SIZE;
	uint64_t cluster_data_sector = cluster_data_start_sector+cluster_sector_offset;
	if (partition_write_sectors(pMountHandle->drive_id, pMountHandle->partition_id, cluster_data_sector, sectors_per_cluster, pClusterData)!=0)
		return -1;
	return 0;
}
int fat32_get_cluster_data_sector(struct fat32_mount_handle* pMountHandle, uint64_t* pClusterDataSector){
	if (!pMountHandle||!pClusterDataSector)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t sectors_per_fat = 0;
	if (fat32_get_sectors_per_fat(pMountHandle, &sectors_per_fat)!=0)
		return -1;
	uint64_t clusterDataSector = ((uint64_t)bpb.reserved_sectors)+sectors_per_fat*bpb.fat_count;
	*pClusterDataSector = clusterDataSector;
	return 0;
}
int fat32_get_bytes_per_cluster(struct fat32_mount_handle* pMountHandle, uint64_t* pBytesPerCluster){
	if (!pMountHandle||!pBytesPerCluster)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t bytesPerCluster = ((uint64_t)bpb.sectors_per_cluster)*DRIVE_SECTOR_SIZE;
	*pBytesPerCluster = bytesPerCluster;
	return 0;
}
int fat32_get_fat(struct fat32_mount_handle* pMountHandle, uint64_t* pFatSector){
	if (!pMountHandle||!pFatSector)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t fat_sector = (uint64_t)bpb.reserved_sectors;
	*pFatSector = fat_sector;
	return 0;
}
int fat32_get_backup_fat(struct fat32_mount_handle* pMountHandle, uint64_t* pFatSector){
	if (!pMountHandle||!pFatSector)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t sectors_per_fat = 0;
	if (fat32_get_sectors_per_fat(pMountHandle, &sectors_per_fat)!=0)
		return -1;
	uint64_t fat_sector = (uint64_t)(bpb.reserved_sectors+sectors_per_fat);
	*pFatSector = fat_sector;
	return 0;
}
int fat32_get_sectors_per_fat(struct fat32_mount_handle* pMountHandle, uint64_t* pSectorsPerFat){
	if (!pMountHandle||!pSectorsPerFat)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t sectors_per_fat = bpb.ebr.sectors_per_fat;
	if (!sectors_per_fat)
		return -1;
	*pSectorsPerFat = sectors_per_fat;
	return 0;
}
int fat32_get_sector_count(struct fat32_mount_handle* pMountHandle, uint64_t* pSectorCount){
	if (!pMountHandle||!pSectorCount)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	uint64_t sectorCount = 0;
	if (bpb.large_sector_count)
		sectorCount = bpb.large_sector_count;
	else
		sectorCount = bpb.total_sectors;
	*pSectorCount = sectorCount;
	return 0;
}
int fat32_verify(struct fat32_mount_handle* pMountHandle){
	if (!pMountHandle)
		return -1;
	struct fat32_bpb bpb = {0};
	struct fat32_ebr ebr = {0};
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	if (fat32_get_ebr(pMountHandle, &ebr)!=0)
		return -1;
	if (fat32_get_fsinfo(pMountHandle, &fsinfo)!=0)
		return -1;
	if (ebr.signature!=0x28&&ebr.signature!=0x29){
		return -1;
	}
	if (ebr.system_ident_string!=FAT32_SYSTEM_IDENT_STRING){
		return -1;
	}
	if (fsinfo.signature0!=FAT32_SIGNATURE0){
		return -1;
	}
	if (fsinfo.signature1!=FAT32_SIGNATURE1){
		return -1;
	}
	if (fsinfo.signature2!=FAT32_SIGNATURE2){
		return -1;
	}
	uint64_t fat_sectors = 0;
	if (fat32_get_sectors_per_fat(pMountHandle, &fat_sectors)!=0)
		return -1;
	return 0;
}
int fat32_get_bpb(struct fat32_mount_handle* pMountHandle, struct fat32_bpb* pBpb){
	if (!pMountHandle||!pBpb)
		return -1;
	if (fat32_get_bpb_cache(pMountHandle, pBpb)==0)
		return 0;
	if (partition_read_sectors(pMountHandle->drive_id, pMountHandle->partition_id, 0, 1, (unsigned char*)pBpb)!=0)
		return -1;
	fat32_cache_bpb(pMountHandle, *pBpb);
	return 0;
}
int fat32_get_ebr(struct fat32_mount_handle* pMountHandle, struct fat32_ebr* pEbr){
	if (!pMountHandle||!pEbr)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	*pEbr = bpb.ebr;
	return 0;
}
int fat32_get_fsinfo(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo* pFsinfo){
	if (!pMountHandle||!pFsinfo)
		return -1;
	if (fat32_get_fsinfo_cache(pMountHandle, pFsinfo)==0)
		return 0;
	struct fat32_ebr ebr = {0};
	if (fat32_get_ebr(pMountHandle, &ebr)!=0)
		return -1;
	if (partition_read_sectors(pMountHandle->drive_id, pMountHandle->partition_id, ebr.fsinfo_sector, 1,(unsigned char*)pFsinfo)!=0)
		return -1;
	return 0;
}
int fat32_set_bpb(struct fat32_mount_handle* pMountHandle, struct fat32_bpb newBpb){
	if (!pMountHandle)
		return -1;
	if (partition_write_sectors(pMountHandle->drive_id, pMountHandle->partition_id, 0, 1, (unsigned char*)&newBpb)!=0)
		return -1;
	pMountHandle->cacheInfo.bpb_cache = newBpb;
	return 0;
}
int fat32_set_ebr(struct fat32_mount_handle* pMountHandle, struct fat32_ebr newEbr){
	if (!pMountHandle)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(pMountHandle, &bpb)!=0)
		return -1;
	bpb.ebr = newEbr;
	if (fat32_set_bpb(pMountHandle, bpb)!=0)
		return -1;
	return 0;
}
int fat32_set_fsinfo(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo newFsinfo){
	if (!pMountHandle)
		return -1;
	struct fat32_ebr ebr = {0};
	if (fat32_get_ebr(pMountHandle, &ebr)!=0)
		return -1;
	if (partition_write_sectors(pMountHandle->drive_id, pMountHandle->partition_id, ebr.fsinfo_sector, 1, (unsigned char*)&newFsinfo)!=0)
		return -1;
	pMountHandle->cacheInfo.fsinfo_cache = newFsinfo;
	return 0;
}
int fat32_get_bpb_cache(struct fat32_mount_handle* pMountHandle, struct fat32_bpb* pBpb){
	if (!pMountHandle||!pBpb)
		return -1;
	if (!pMountHandle->cacheInfo.bpb_cached)
		return -1;
	*pBpb = pMountHandle->cacheInfo.bpb_cache;
	return 0;
}
int fat32_get_fsinfo_cache(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo* pFsinfo){
	if (!pMountHandle||!pFsinfo)
		return -1;
	if (!pMountHandle->cacheInfo.fsinfo_cached)
		return -1;
	*pFsinfo = pMountHandle->cacheInfo.fsinfo_cache;
	return 0;
}
int fat32_get_cached_last_sector(struct fat32_mount_handle* pMountHandle, uint64_t* pLastSector){
	if (!pMountHandle||!pLastSector)
		return -1;
	if (!pMountHandle->cacheInfo.last_sector_cached)
		return -1;
	*pLastSector = pMountHandle->cacheInfo.last_sector;
	return 0;
}
int fat32_cache_bpb(struct fat32_mount_handle* pMountHandle, struct fat32_bpb bpb){
	if (!pMountHandle)
		return -1;
	pMountHandle->cacheInfo.bpb_cache = bpb;
	pMountHandle->cacheInfo.bpb_cached = 1;
	return 0;
}
int fat32_cache_fsinfo(struct fat32_mount_handle* pMountHandle, struct fat32_fsinfo fsinfo){
	if (!pMountHandle)
		return -1;
	pMountHandle->cacheInfo.fsinfo_cache = fsinfo;
	pMountHandle->cacheInfo.fsinfo_cached = 1;
	return 0;
}
int fat32_cache_last_sector(struct fat32_mount_handle* pMountHandle, uint64_t last_sector){
	if (!pMountHandle)
		return -1;
	pMountHandle->cacheInfo.last_sector = last_sector;
	pMountHandle->cacheInfo.last_sector_cached = 1;
	return 0;
}
int fat32_subsystem_verify(struct fs_mount* pMount){
	if (!pMount)
		return -1;
	struct fat32_mount_handle* pMountHandle = (struct fat32_mount_handle*)pMount->pMountData;
	if (!pMountHandle)
		return -1;
	if (fat32_verify(pMountHandle)!=0)
		return -1;
	return 0;
}
int fat32_subsystem_mount(uint64_t drive_id, uint64_t partition_id, struct fs_mount* pMount){
	if (!pMount)
		return -1;
	struct fat32_mount_handle* pHandle = (struct fat32_mount_handle*)0x0;
	if (fat32_mount(drive_id, partition_id, &pHandle)!=0)
		return -1;
	pMount->pMountData = (void*)pHandle;
	return 0;
}
int fat32_subsystem_unmount(struct fs_mount* pMount){
	if (!pMount)
		return -1;
	struct fat32_mount_handle* pHandle = (struct fat32_mount_handle*)pMount->pMountData;
	if (!pHandle)
		return -1;
	if (fat32_unmount(pHandle)!=0)
		return -1;
	return 0;
}
int fat32_subsystem_open(struct fs_mount* pMount, uint16_t* filename, void** ppFileHandle){
	if (!pMount||!ppFileHandle){
		return -1;
	}
	struct fat32_mount_handle* pHandle = (struct fat32_mount_handle*)pMount->pMountData;
	if (!pHandle){
		return -1;
	}
	unsigned char filename_ascii[FAT32_FILENAME_LEN_MAX] = {0};
	for (uint64_t i = 0;i<FAT32_FILENAME_LEN_MAX;i++){
		filename_ascii[i] = (unsigned char)(filename[i]);
		if (!filename[i])
			break;
	}
	filename_ascii[FAT32_FILENAME_LEN_MAX-1] = 0;
	struct fat32_file_handle* pFileHandle = (struct fat32_file_handle*)0x0;
	if (fat32_openfile(pHandle, filename_ascii, &pFileHandle)!=0){
		return -1;
	}
	*ppFileHandle = (void*)pFileHandle;
	return 0;
}
int fat32_subsystem_close(void* pFileHandle){
	if (!pFileHandle)
		return -1;
	if (fat32_closefile((struct fat32_file_handle*)pFileHandle)!=0)
		return -1;
	return 0;
}
int fat32_subsystem_read(struct fs_mount* pMount, void* pFileHandle, unsigned char* pBuffer, uint64_t size){
	if (!pMount||!pFileHandle||!pBuffer||!size)
		return -1;
	if (fat32_readfile((struct fat32_file_handle*)pFileHandle, pBuffer, size)!=0)
		return -1;
	return 0;
}
int fat32_subsystem_write(struct fs_mount* pMount, void* pFileHandle, unsigned char* pBuffer, uint64_t size){
	if (!pMount||!pFileHandle||!pBuffer||!size)
		return -1;
	if (fat32_writefile((struct fat32_file_handle*)pFileHandle, pBuffer, size)!=0)
		return -1;
	return 0;
}
int fat32_subsystem_getFileInfo(struct fs_mount* pMount, void* pFileHandle, struct fs_file_info* pFileInfo){
	if (!pMount||!pFileHandle||!pFileInfo)
		return -1;
	struct fs_file_info fileInfo = {0};
	struct fat32_mount_handle* pMountHandle = (struct fat32_mount_handle*)pMount->pMountData;
	if (!pMountHandle)
		return -1;
	uint64_t fileSize = 0;
	uint64_t fatFileAttribs = 0;
	if (fat32_get_file_size((struct fat32_file_handle*)pFileHandle, (uint32_t*)&fileSize)!=0)
		return -1;
	if (fat32_get_file_attributes((struct fat32_file_handle*)pFileHandle, (uint8_t*)&fatFileAttribs)!=0)
		return -1;
	uint64_t fileAttribs = 0;
	fileAttribs|=fatFileAttribs&FAT32_FILE_ATTRIBUTE_HIDDEN ? FILE_ATTRIBUTE_HIDDEN : 0;
	fileAttribs|=fatFileAttribs&FAT32_FILE_ATTRIBUTE_SYSTEM ? FILE_ATTRIBUTE_SYSTEM : 0;
	fileAttribs|=fatFileAttribs&FAT32_FILE_ATTRIBUTE_READONLY ? FILE_ATTRIBUTE_READONLY : 0;
	fileAttribs|=fatFileAttribs&FAT32_FILE_ATTRIBUTE_DIRECTORY ? FILE_ATTRIBUTE_DIRECTORY : 0;
	fileInfo.fileSize = fileSize;
	fileInfo.fileAttributes = fileAttribs;
	*pFileInfo = fileInfo;
	return 0;
}
int fat32_subsystem_create_file(struct fs_mount* pMount, unsigned char* filename, uint64_t fileAttribs){
	if (!pMount||!filename)
		return -1;
	struct fat32_mount_handle* pMountHandle = (struct fat32_mount_handle*)pMount->pMountData;
	if (!pMountHandle)
		return -1;
	uint8_t fatAttribs = 0;
	fatAttribs|=fileAttribs&FILE_ATTRIBUTE_HIDDEN ? FAT32_FILE_ATTRIBUTE_HIDDEN : 0;
	fatAttribs|=fileAttribs&FILE_ATTRIBUTE_SYSTEM ? FAT32_FILE_ATTRIBUTE_SYSTEM : 0;
	fatAttribs|=fileAttribs&FILE_ATTRIBUTE_DIRECTORY ? FAT32_FILE_ATTRIBUTE_DIRECTORY : 0;
	fatAttribs|=fileAttribs&FILE_ATTRIBUTE_READONLY ? FAT32_FILE_ATTRIBUTE_READONLY : 0;
	if (fat32_createfile(pMountHandle, filename, (uint8_t)fatAttribs)!=0)
		return -1;
	return 0;
}
int fat32_subsystem_delete_file(struct fs_mount* pMount, void* pFileHandle){
	if (!pMount||!pFileHandle)
		return -1;
	if (fat32_deletefile((struct fat32_file_handle*)pFileHandle)!=0)
		return -1;
	return 0;
}
