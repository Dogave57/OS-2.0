#include "kernel_include.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "drivers/graphics.h"
#include "stdlib/stdlib.h"
#include "align.h"
#include "kexts/loader.h"
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
	struct elf64_dyn* pDynamicTable = (struct elf64_dyn*)0x0;
	uint64_t dynamicTableEntries = 0;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type==ELF_PT_DYNAMIC){
			pDynamicTable = (struct elf64_dyn*)(pImage+pCurrentPh->p_va);
			dynamicTableEntries = pCurrentPh->p_memorySize/sizeof(struct elf64_dyn);
			continue;
		}
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
	if (!pDynamicTable){
		printf("dynamic table is mandatory\r\n");
		virtualFree((uint64_t)pImage, imageSize);
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	struct elf64_rel* pRelTable = (struct elf64_rel*)0x0;
	struct elf64_rela* pRelaTable = (struct elf64_rela*)0x0;
	struct elf64_sym* pSymbolTable = (struct elf64_sym*)0x0;
	unsigned char* pStringTable = (unsigned char*)0x0;
	uint64_t stringTableSize = 0;
	uint64_t relTableEntries = 0;
	uint64_t relaTableEntries = 0;
	for (uint64_t i = 0;i<dynamicTableEntries;i++){
		struct elf64_dyn* pCurrentEntry = pDynamicTable+i;
		switch (pCurrentEntry->d_tag){
			case ELF_DT_REL:{
			pRelTable = (struct elf64_rel*)(pImage+pCurrentEntry->d_ptr);
			break;
			}
			case ELF_DT_RELA:{
			pRelaTable = (struct elf64_rela*)(pImage+pCurrentEntry->d_ptr);
			break;		 
			}
			case ELF_DT_REL_SIZE:{
	   		relTableEntries = pCurrentEntry->d_value/sizeof(struct elf64_rel);		
			break;		     
			}
			case ELF_DT_RELA_SIZE:{
			relaTableEntries = pCurrentEntry->d_value/sizeof(struct elf64_rela);
			break;		      
			}
			case ELF_DT_SYMTAB:{
			pSymbolTable = (struct elf64_sym*)(pImage+pCurrentEntry->d_ptr);		   
			break;		   
			}
			case ELF_DT_STRTAB:{
			pStringTable = (unsigned char*)(pImage+pCurrentEntry->d_ptr);		   
			break;
			}
			case ELF_DT_STRTAB_SIZE:{
			stringTableSize = pCurrentEntry->d_value;			
			break;			
			}
		}
	}
	if (!pStringTable){
		printf("no string table\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
		return -1;
	}
	if (!pSymbolTable){
		printf("no symbol table\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
		return -1;
	}
	for (uint64_t i = 0;pRelTable&&i<relTableEntries;i++){
		struct elf64_rel* pCurrentEntry = (struct elf64_rel*)(pRelTable+i);
		uint64_t relocType = ELF64_R_TYPE(pCurrentEntry->r_info);
		uint64_t relocSymIndex = ELF64_R_INDEX(pCurrentEntry->r_info);
		printf("rel entry type: %d\r\n", relocType);
		printf("rel entry symbol table index: %d\r\n", relocSymIndex);
		switch (relocType){
			case ELF_RELOC_GLOB_DAT_X64:{
			struct elf64_sym* pSymbol = pSymbolTable+relocSymIndex;
			unsigned char* pSymbolName = pStringTable+pSymbol->st_name;
			printf("symbol name: %s\r\n", pSymbolName);
			break;
		        }

		}
	}
	for (uint64_t i = 0;pRelaTable&&i<relaTableEntries;i++){
		struct elf64_rela* pCurrentEntry = (struct elf64_rela*)(pRelaTable+i);
		uint64_t relocType = ELF64_R_TYPE(pCurrentEntry->r_info);
		uint64_t relocSymIndex = ELF64_R_INDEX(pCurrentEntry->r_info);
		printf("rela entry type: %d\r\n", relocType);
		switch (relocType){
			case ELF_RELOC_RELATIVE_X64:{
			uint64_t* pPatch = (uint64_t*)(pImage+pCurrentEntry->r_offset);
			*pPatch = (uint64_t)(pImage+pCurrentEntry->r_addend);
			break;
			}
			case ELF_RELOC_ABS_X64:{
			uint64_t* pPatch = (uint64_t*)(pImage+pCurrentEntry->r_offset);
			uint64_t symbolIndex = ELF64_R_INDEX(pCurrentEntry->r_info);
 			struct elf64_sym* pSymbol = pSymbolTable+symbolIndex;
			uint64_t symbolAddress = pSymbol->st_value;
			*pPatch = symbolAddress+pCurrentEntry->r_addend;
			break;
		        }
			case ELF_RELOC_PC32:{
			uint64_t* pPatch = (uint64_t*)(pImage+pCurrentEntry->r_offset);
			uint64_t symbolIndex = ELF64_R_INDEX(pCurrentEntry->r_info);
			struct elf64_sym* pSymbol = pSymbolTable+symbolIndex;
			uint64_t symbolAddress = pSymbol->st_value;
			*pPatch = symbolAddress+pCurrentEntry->r_addend-pCurrentEntry->r_offset;
			break;
			}
			case ELF_RELOC_GLOB_DAT_X64:{
			uint64_t* pPatch = (uint64_t*)(pImage+pCurrentEntry->r_offset);
			uint64_t relocSymIndex = ELF64_R_INDEX(pCurrentEntry->r_info);
			struct elf64_sym* pSymbol = pSymbolTable+relocSymIndex;
			unsigned char* pSymbolName = pStringTable+pSymbol->st_name;
			printf("symbol name: %s\r\n", pSymbolName);			
			break;			    
			}
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
int elf_execute(struct elf_handle* pHandle, int* pStatus){
	if (!pHandle)
		return -1;
	if (!pHandle->pFileBuffer||!pHandle->pImage)
		return -1;
	struct elf64_header* pHeader = (struct elf64_header*)pHandle->pFileBuffer;
	uint64_t entryOffset = pHeader->e_entry;
	kextEntry entry = (kextEntry)(pHandle->pImage+entryOffset);
	int status = entry();
	if (pStatus)
		*pStatus = status;
	return 0;
}
int elf_unload(struct elf_handle* pHandle){
	if (!pHandle)
		return -1;
	if (pHandle->pFileBuffer)
		virtualFree((uint64_t)pHandle->pFileBuffer, pHandle->fileInfo.fileSize);
	if (pHandle->pImage)
		virtualFree((uint64_t)pHandle->pImage, pHandle->imageSize);
	kfree((void*)pHandle);
	return 0;
}
