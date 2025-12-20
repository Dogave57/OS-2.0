#include "kernel_include.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "drivers/graphics.h"
#include "stdlib/stdlib.h"
#include "align.h"
#include "elf.h"
int elf_load(uint64_t mount_id, unsigned char* filename, struct elf_handle** ppHandle){
	if (!filename||!ppHandle)
		return -1;
	uint64_t fileId = 0;
	if (fs_open(mount_id, filename, 0, &fileId)!=0)
		return -1;
	struct fs_file_info fileInfo = {0};
	if (fs_getFileInfo(mount_id, fileId, &fileInfo)!=0){
		fs_close(mount_id, fileId);
		return -1;
	}
	unsigned char* pFileBuffer = (unsigned char*)0x0;
	if (virtualAlloc((uint64_t*)&pFileBuffer, fileInfo.fileSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		fs_close(mount_id, fileId);
		return -1;
	}
	if (fs_read(mount_id, fileId, pFileBuffer, fileInfo.fileSize)!=0){
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);	
		fs_close(mount_id, fileId);
		return -1;
	}
	fs_close(mount_id, fileId);
	struct elf64_header* pHeader = (struct elf64_header*)pFileBuffer;
	if (!ELF_VALID_SIGNATURE(pFileBuffer)){
		printf("invalid ELF signature\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	if (pHeader->e_machine!=ELF_MACHINE_X64){
		printf("unsupported architecture!\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	struct elf64_pheader* pFirstPh = (struct elf64_pheader*)(pFileBuffer+pHeader->e_phoff);
	struct elf64_pheader* pCurrentPh = pFirstPh;
	uint64_t imageSize = 0;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		imageSize+=align_up(pCurrentPh->p_memorySize, pCurrentPh->p_align);
	}
	pCurrentPh = pFirstPh;
	unsigned char* pImage = (unsigned char*)0x0;
	if (virtualAlloc((uint64_t*)&pImage, imageSize, PTE_RW, 0, PAGE_TYPE_NORMAL)!=0){
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		uint64_t virtualAddress = pCurrentPh->p_va;
		memcpy((unsigned char*)(pImage+virtualAddress), (unsigned char*)(pFileBuffer+pCurrentPh->p_offset), pCurrentPh->p_fileSize);
		uint64_t extraBytes = pCurrentPh->p_memorySize-pCurrentPh->p_fileSize;
		for (uint64_t i = 0;i<extraBytes;i++){
			uint64_t offset = virtualAddress+pCurrentPh->p_fileSize+i;
			*(pImage+offset) = 0;
		}
	}
	struct elf_handle* pHandle = (struct elf_handle*)kmalloc(sizeof(struct elf_handle));
	if (!pHandle)
		return -1;
	memset((void*)pHandle, 0, sizeof(struct elf_handle));
	pHandle->fileInfo = fileInfo;
	pHandle->pFileBuffer = pFileBuffer;
	pHandle->pImage = pImage;
	pHandle->imageSize = imageSize;
	*ppHandle = pHandle;
	return 0;
}
int elf_unload(struct elf_handle* pHandle){
	if (!pHandle)
		return -1;
	return 0;
}
