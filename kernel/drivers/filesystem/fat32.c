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
int fat32_opendir(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, struct fat32_dir_handle** ppDirHandle){
	if (!filename||!ppDirHandle)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint32_t firstDirCluster = bpb.ebr.root_cluster_number;
	if (filename[0]!='/'||filename[1]){
		struct fat32_file_entry fileEntry = {0};
		struct fat32_file_location fileLocation = {0};	
		if (fat32_find_file(drive_id, partition_id, filename, &fileEntry, &fileLocation, 0)!=0)
			return -1;
		firstDirCluster = ((uint32_t)fileEntry.entry_first_cluster_high)|((uint32_t)fileEntry.entry_first_cluster_low);
	}
	struct fat32_dir_handle* pDirHandle = (struct fat32_dir_handle*)kmalloc(sizeof(struct fat32_dir_handle));
	if (!pDirHandle)
		return -1;
	memset((void*)pDirHandle, 0, sizeof(struct fat32_dir_handle));
	pDirHandle->desc.firstDirCluster = firstDirCluster;
	pDirHandle->currentDirCluster.cluster_value = firstDirCluster;
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
	if (fat32_get_bytes_per_cluster(pDirHandle->drive_id, pDirHandle->partition_id, &bytesPerCluster)!=0)
		return -1;
	uint64_t maxFileEntries = bytesPerCluster/sizeof(struct fat32_file_entry);
	if (pDirHandle->currentDirIndex>=maxFileEntries){
		if (fat32_readcluster(pDirHandle->drive_id, pDirHandle->partition_id, pDirHandle->currentDirCluster.cluster_value, &pDirHandle->currentDirCluster)!=0)
			return -1;
		if (FAT32_CLUSTER_END_OF_CHAIN(pDirHandle->currentDirCluster.cluster_value))
			return -1;
		pDirHandle->currentDirIndex = 0;
	}
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pDirHandle->drive_id, pDirHandle->partition_id, pDirHandle->currentDirCluster.cluster_value, pClusterData)!=0){
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
int fat32_openfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, struct fat32_file_handle** ppFileHandle){
	if (!ppFileHandle)
		return -1;
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	struct fat32_file_location fileLocation = {0};
	struct fat32_file_entry fileEntry = {0};
	if (fat32_find_file(drive_id, partition_id, filename, &fileEntry, &fileLocation,0)!=0){
		printf(L"failed to find file\r\n");
		return -1;
	}
	struct fat32_file_handle* pFileHandle = (struct fat32_file_handle*)kmalloc(sizeof(struct fat32_file_handle));
	if (!pFileHandle)
		return -1;
	memset((void*)pFileHandle, 0, sizeof(struct fat32_file_handle));
	pFileHandle->fileLocation = fileLocation;
	pFileHandle->fileEntry = fileEntry;
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
	if (fat32_get_bytes_per_cluster(pFileHandle->drive_id, pFileHandle->partition_id, &bytesPerCluster)!=0)
		return -1;
	struct fat32_file_location* pFileLocation = &pFileHandle->fileLocation;
	struct fat32_file_entry* pFileEntry = &pFileHandle->fileEntry;
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	struct fat32_file_entry* pNewFileEntry = ((struct fat32_file_entry*)pClusterData)+pFileLocation->fileIndex;
	if (fat32_string_to_filename(pNewFileEntry->filename, filename)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	if (fat32_write_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, pFileLocation->fileCluster, pClusterData)!=0){
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
	if (!fileEntry.file_size)
		return -1;
	if (fat32_get_bytes_per_cluster(pFileHandle->drive_id, pFileHandle->partition_id, &bytesPerCluster)!=0)
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
			if (fat32_read_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, pClusterData)!=0){
				kfree((void*)pClusterData);
				return -1;
			}
			memcpy((unsigned char*)(pFileBuffer+(i*bytesPerCluster)), (unsigned char*)pClusterData, alignOffset);
			kfree((void*)pClusterData);
			if (fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, &currentCluster)!=0)
				return -1;
			continue;
		}
		unsigned char* pClusterData = pFileBuffer+(i*bytesPerCluster);
		if (fat32_read_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, pClusterData)!=0)
			return -1;
		if (fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, &currentCluster)!=0)
			return -1;
	}
	return 0;
}
int fat32_writefile(struct fat32_file_handle* pFileHandle, unsigned char* pFileBuffer, uint32_t size){
	if (!pFileHandle||!pFileBuffer)
		return -1;
	uint64_t bytesPerCluster = 0;
	if (fat32_get_bytes_per_cluster(pFileHandle->drive_id, pFileHandle->partition_id, &bytesPerCluster)!=0)
		return -1;
	struct fat32_file_entry* pFileEntry = &pFileHandle->fileEntry;
	struct fat32_file_location* pFileLocation = &pFileHandle->fileLocation;
	uint64_t clustersNeeded = align_up(size, bytesPerCluster)/bytesPerCluster;
	uint64_t fileClusters = 0;
	if (pFileEntry->file_size)
		fileClusters = align_up(pFileEntry->file_size, bytesPerCluster)/bytesPerCluster;
	if (!fileClusters){
		uint32_t newCluster = 0;
		if (fat32_allocate_cluster(pFileHandle->drive_id, pFileHandle->partition_id, &newCluster)!=0)
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
		if (fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, lastClusterEntry.cluster_value, &lastClusterEntry)!=0)
			break;
	}
	uint64_t clustersToAlloc = 0;
	if (fileClusters<clustersNeeded)
		clustersToAlloc = clustersNeeded-fileClusters;
	struct fat32_cluster_entry currentCluster = {0};
	currentCluster = lastClusterEntry;
	for (uint64_t i = 0;i<clustersToAlloc;i++){
		struct fat32_cluster_entry newCluster = {0};
		if (fat32_allocate_cluster(pFileHandle->drive_id, pFileHandle->partition_id, (uint32_t*)&newCluster)!=0)
			return -1;
		if (fat32_writecluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, newCluster)!=0)
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
			if (fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, &currentCluster)!=0){
				printf(L"failed to read next cluster\r\n");
				return -1;
			}
			continue;
		}
		struct fat32_cluster_entry nextCluster = {0};
		if (fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, &nextCluster)!=0){
			printf(L"failed to read next cluster\r\n");
			return -1;
		}
		if (fat32_free_cluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value)!=0){
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
			if (fat32_write_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, pClusterData)!=0){
				printf(L"failed to write last misaligned cluster\r\n");
				return -1;
			}
			kfree((void*)pClusterData);
			fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, &currentCluster);
			continue;
		}
		unsigned char* pClusterData = pFileBuffer+(i*bytesPerCluster);
		if (fat32_write_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, pClusterData)!=0){
			printf(L"failed to write cluster %d with id %d\r\n", i, currentCluster.cluster_value);
			return -1;
		}
		if (fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, &currentCluster)!=0&&i!=fileClusters-1){
			printf(L"failed to read next cluster\r\n");
			return -1;
		}
	}
	pFileEntry->file_size = size;
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	struct fat32_file_entry* pNewFileEntry = ((struct fat32_file_entry*)pClusterData)+pFileLocation->fileIndex;
	*pNewFileEntry = pFileHandle->fileEntry;
	uint32_t entry = ((uint32_t)pFileHandle->fileEntry.entry_first_cluster_high<<16)|((uint32_t)pFileHandle->fileEntry.entry_first_cluster_low);
	if (fat32_write_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	kfree((void*)pClusterData);	
	return 0;
}
int fat32_createfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, uint8_t file_attribs){
	if (!filename)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint64_t bytesPerCluster = 0;
	if (fat32_get_bytes_per_cluster(drive_id, partition_id, &bytesPerCluster)!=0)
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
		if (fat32_find_file_in_dir(drive_id, partition_id, dirLocation.fileCluster, filename+pathstart, &dirEntry, &dirLocation, FAT32_FILE_ATTRIBUTE_DIRECTORY)!=0){
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
		if (fat32_read_cluster_data(drive_id, partition_id, dirLocation.fileCluster, pClusterData)!=0){
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
		if (fat32_read_cluster_data(drive_id, partition_id, currentCluster.cluster_value, pClusterData)!=0)
			return -1;
		for (uint64_t i = 0;i<max_file_entries;i++){
			struct fat32_file_entry* pFileEntry = ((struct fat32_file_entry*)pClusterData)+i;
			if (pFileEntry->filename[0])
				continue;
			if (fat32_string_to_filename(pFileEntry->filename, filename+pathstart)!=0)
				continue;
			pFileEntry->file_size = 0;
			pFileEntry->file_attribs = file_attribs;
			if (fat32_write_cluster_data(drive_id, partition_id, currentCluster.cluster_value, pClusterData)!=0){
				kfree((void*)pClusterData);
				return -1;
			}
			kfree((void*)pClusterData);
			return 0;
		}
		if (fat32_readcluster(drive_id, partition_id, currentCluster.cluster_value, &currentCluster)!=0)
			return -1;
	}
	kfree((void*)pClusterData);
	return -1;
}
int fat32_deletefile(struct fat32_file_handle* pFileHandle){
	if (!pFileHandle)
		return -1;
	uint64_t bytesPerCluster = 0;
	if (fat32_get_bytes_per_cluster(pFileHandle->drive_id, pFileHandle->partition_id, &bytesPerCluster)!=0)
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
		if (fat32_readcluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value, &nextCluster)!=0)
			return -1;
		if (fat32_free_cluster(pFileHandle->drive_id, pFileHandle->partition_id, currentCluster.cluster_value)!=0)
			return -1;
		currentCluster = nextCluster;
	}
	unsigned char* pClusterData = (unsigned char*)kmalloc(bytesPerCluster);
	if (!pClusterData)
		return -1;
	if (fat32_read_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, pFileLocation->fileCluster, pClusterData)!=0){
		kfree((void*)pClusterData);
		return -1;
	}
	struct fat32_file_entry* pNewFileEntry = ((struct fat32_file_entry*)pClusterData)+pFileLocation->fileIndex;
	memset((void*)pNewFileEntry, 0, sizeof(struct fat32_file_entry));
	if (fat32_write_cluster_data(pFileHandle->drive_id, pFileHandle->partition_id, pFileLocation->fileCluster, pClusterData)!=0){
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
int fat32_find_file(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, struct fat32_file_entry* pFileEntry, struct fat32_file_location* pFileLocation, uint8_t file_attribs){
	if (!filename||!pFileLocation||!pFileEntry)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
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
		if (fat32_find_file_in_dir(drive_id, partition_id, fileLocation.fileDataCluster, filename+pathstart, &fileEntry, &fileLocation, mask)!=0){
			printf_ascii("failed to find %s\r\n", filename+pathstart);
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
int fat32_find_file_in_dir(uint64_t drive_id, uint64_t partition_id, uint32_t dirCluster, unsigned char* filename, struct fat32_file_entry* pFileEntry, struct fat32_file_location* pFileLocation, uint8_t file_attrib){
	if (!filename||!pFileLocation||!pFileEntry)
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
		if (fat32_readcluster(drive_id, partition_id, currentCluster.cluster_value, &currentCluster)!=0){
			break;
		}
	}
	virtualFreePages((uint64_t)pClusterData, clusterDataPages);
	return -1;
}
int fat32_find_file_in_root(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, struct fat32_file_entry* pFileEntry,struct fat32_file_location* pFileLocation, uint8_t file_attribs){
	if (!filename||!pFileLocation||!pFileEntry)
		return -1;
	struct fat32_bpb bpb = {0};
	if (fat32_get_bpb(drive_id, partition_id, &bpb)!=0)
		return -1;
	uint32_t rootDirCluster = bpb.ebr.root_cluster_number;
	if (fat32_find_file_in_dir(drive_id, partition_id, rootDirCluster, filename, pFileEntry, pFileLocation, file_attribs)!=0)
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
	for (uint32_t i = start_cluster;;){
		struct fat32_cluster_entry clusterEntry = {0};
		if (fat32_readcluster(drive_id, partition_id, i, &clusterEntry)!=0)
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
int fat32_free_cluster(uint64_t drive_id, uint64_t partition_id, uint32_t freeCluster){
	struct fat32_fsinfo fsinfo = {0};
	if (fat32_get_fsinfo(drive_id, partition_id, &fsinfo)!=0)
		return -1;
	struct fat32_cluster_entry newClusterEntry = {0};
	newClusterEntry.cluster_value = FAT32_CLUSTER_FREE;
	if (fat32_writecluster(drive_id, partition_id, freeCluster, newClusterEntry)!=0)
		return -1;
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
