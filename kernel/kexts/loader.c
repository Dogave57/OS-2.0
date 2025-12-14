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
		fs_close(mount_id, fileId);
		return -1;
	}
	if (fileInfo.fileSize<ELF64_MIN_SIZE){
		fs_close(mount_id, fileId);
		return -1;
	}
	unsigned char* pFileBuffer = (unsigned char*)0x0;
	if (virtualAlloc((uint64_t*)&pFileBuffer, fileInfo.fileSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failled to allocate file buffer\r\n");
		fs_close(mount_id, fileId);
		return -1;
	}
	if (fs_read(mount_id, fileId, pFileBuffer, fileInfo.fileSize)!=0){
		printf("failed to read file\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		fs_close(mount_id, fileId);
		return -1;
	}
	fs_close(mount_id, fileId);
	struct elf64_header* pHeader = (struct elf64_header*)pFileBuffer;
	if (!ELF_VALID_SIGNATURE(pFileBuffer)){
		printf("invalid ELF binary signature\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}	
	if (pHeader->e_machine!=ELF_MACHINE_X64){
		printf("ELF binary is not supposed to operate in long mode\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	uint64_t imageSize = 0;
	struct elf64_sheader* pFirstSh = (struct elf64_sheader*)(pFileBuffer+pHeader->e_shoff);
	struct elf64_pheader* pFirstPh = (struct elf64_pheader*)(pFileBuffer+pHeader->e_phoff);
	struct elf64_sheader* pCurrentSh = pFirstSh;
	struct elf64_pheader* pCurrentPh = pFirstPh;
	uint64_t virtualBase = 0xFFFFFFFFFFFFFFFF;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		uint64_t virtualSize = pCurrentPh->p_memorySize;
		if (pCurrentPh->p_align)
			virtualSize = align_up(virtualSize, pCurrentPh->p_align);
		printf("loadable segment va: %p, size: %d\r\n", (void*)pCurrentPh->p_va, virtualSize);
		imageSize+=virtualSize;
		if (pCurrentPh->p_va<virtualBase)
			virtualBase = pCurrentPh->p_va;
	}
	unsigned char* pImage = (unsigned char*)0x0;
	if (virtualAlloc((uint64_t*)&pImage, imageSize, PTE_RW, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate actual image\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	pCurrentPh = pFirstPh;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		uint64_t virtualOffset = pCurrentPh->p_va-virtualBase;
		unsigned char* pFileSegment = pFileBuffer+pCurrentPh->p_offset;
		unsigned char* pVirtualSegment = pImage+virtualOffset;
		if (virtualOffset>imageSize){
			virtualFree((uint64_t)pImage, fileInfo.fileSize);
			virtualFree((uint64_t)pImage, imageSize);
			return -1;
		}
		memcpy(pVirtualSegment, pFileSegment, pCurrentPh->p_fileSize);
		uint64_t nullBytes = pCurrentPh->p_memorySize-pCurrentPh->p_fileSize;
		if (!nullBytes)
			continue;
		for (uint64_t byte = 0;byte<nullBytes;byte++){
			uint64_t offset = pCurrentPh->p_fileSize+byte;
			if (offset>imageSize){
				virtualFree((uint64_t)pImage, fileInfo.fileSize);
				virtualFree((uint64_t)pImage, imageSize);
				return -1;
			}
			*(pVirtualSegment+offset) = 0;
		}
	}
	uint64_t entryOffset = pHeader->e_entry-virtualBase;
	printf("entry offset: %d\r\n", entryOffset);
	kextEntry entry = (kextEntry)(pImage+entryOffset);
	virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
	unsigned int status = 0;
	status = entry();
	printf("program exited with status 0x%x\r\n", status);
	virtualFree((uint64_t)pImage, imageSize);
	return 0;
}
