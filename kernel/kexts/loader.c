#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/filesystem.h"
#include "align.h"
#include "elf.h"
#include "kexts/loader.h"
int kext_load(uint64_t mount_id, unsigned char* filename, uint64_t* pPid){
	if (!filename||!pPid)
		return -1;
	uint64_t fileId = 0;
	if (fs_open(mount_id, filename, 0, &fileId)!=0)
		return -1;
	struct fs_file_info fileInfo = {0};
	if (fs_getFileInfo(mount_id, fileId, &fileInfo)!=0){
		printf("failed to get file info\r\n");
		return -1;
	}
	unsigned char* pFileBuffer = (unsigned char*)0x0;
	if (virtualAlloc((uint64_t*)&pFileBuffer, fileInfo.fileSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate file buffer\r\n");
		fs_close(mount_id, fileId);
		return -1;
	}
	if (fs_read(mount_id, fileId, pFileBuffer, fileInfo.fileSize)!=0){
		printf("failed to read file\r\n");
		fs_close(mount_id, fileId);
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	struct elf64_header* pHeader = (struct elf64_header*)pFileBuffer;
	if (!ELF_VALID_SIGNATURE(pFileBuffer)){
		printf("invalid signature\r\n");
		fs_close(mount_id, fileId);
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	if (pHeader->e_machine!=ELF_MACHINE_X64){
		printf("invalid architecture\r\n");
		fs_close(mount_id, fileId);
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	if (pHeader->e_type!=ELF_TYPE_DYNAMIC){
		printf("not a valid dynamic ELF binary\r\n");
		fs_close(mount_id, fileId);
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	fs_close(mount_id, fileId);
	virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
	return 0;
}
