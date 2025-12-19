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
	fs_close(mount_id, fileId);
	struct elf64_header* pHeader = (struct elf64_header*)pFileBuffer;
	if (!ELF_VALID_SIGNATURE(pFileBuffer)){
		printf("invalid signature\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	if (pHeader->e_machine!=ELF_MACHINE_X64){
		printf("invalid architecture\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	if (pHeader->e_type!=ELF_TYPE_DYNAMIC){
		printf("not a valid dynamic ELF binary\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	struct elf64_sheader* pFirstSh = (struct elf64_sheader*)(pFileBuffer+pHeader->e_shoff);
	struct elf64_pheader* pFirstPh = (struct elf64_pheader*)(pFileBuffer+pHeader->e_phoff);
	struct elf64_sheader* pCurrentSh = pFirstSh;
	struct elf64_pheader* pCurrentPh = pFirstPh;
	uint64_t imageSize = 0;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		imageSize+=align_up(pCurrentPh->p_memorySize, pCurrentPh->p_align);
	}
	imageSize = align_up(imageSize, PAGE_SIZE);
	unsigned char* pImage = (unsigned char*)0x0;
	if (virtualAlloc((uint64_t*)&pImage, imageSize, PTE_RW, 0, PAGE_TYPE_NORMAL)!=0){
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		return -1;
	}
	uint64_t imageDelta = (uint64_t)pImage-(uint64_t)pFileBuffer;
	pCurrentPh = pFirstPh;
	unsigned char* pStringTable = (unsigned char*)0x0;
	unsigned char* pDynamicStringTable = (unsigned char*)0x0;
	struct elf64_sym* pSymTable = (struct elf64_sym*)0x0;
	struct elf64_sym* pDynamicSymTable = (struct elf64_sym*)0x0;
	uint64_t dynamicSymbolCount = 0;
	uint64_t dynamicSymbolEntrySize = 0;
	uint64_t symbolCount = 0;
	uint64_t symbolEntrySize = 0;
	struct elf64_dyn* pDynTable = (struct elf64_dyn*)0x0;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type==ELF_PT_DYNAMIC){
			pDynTable = (struct elf64_dyn*)(pImage+pCurrentPh->p_va);
			continue;
		}
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		if (pCurrentPh->p_memorySize<pCurrentPh->p_fileSize){
			virtualFree((uint64_t)pImage, imageSize);
			virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
			return -1;
		}
		uint64_t virtualOffset = pCurrentPh->p_va;
		if (virtualOffset>imageSize){
			virtualFree((uint64_t)pImage, imageSize);
			virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
			return -1;
		}
		if (pCurrentPh->p_offset>fileInfo.fileSize){
			virtualFree((uint64_t)pImage, imageSize);
			virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
			return -1;
		}
		memcpy(pImage+virtualOffset, pFileBuffer+pCurrentPh->p_offset, pCurrentPh->p_fileSize);
		uint64_t extraBytes = 0;
		if (pCurrentPh->p_memorySize>pCurrentPh->p_fileSize)
			extraBytes = pCurrentPh->p_memorySize-pCurrentPh->p_fileSize;
		for (uint64_t i = 0;i<extraBytes;i++){
			uint64_t offset = pCurrentPh->p_fileSize+i;
			*(pImage+offset) = *(pFileBuffer+pCurrentPh->p_offset+offset);	
		}
	}
	if (!pDynTable){
		printf("dynamic table is mandatory!\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
		return -1;
	}
	struct elf64_dyn* pCurrentDtEntry = (struct elf64_dyn*)pDynTable;
	struct elf64_rel* pRelTable = (struct elf64_rel*)0x0;
	struct elf64_rela* pRelaTable = (struct elf64_rela*)0x0;
	uint64_t relSize = 0;
	uint64_t relaSize = 0;
	uint64_t relEntrySize = 0;
	uint64_t relaEntrySize = 0;
	while (pCurrentDtEntry->d_tag){
		switch (pCurrentDtEntry->d_tag){
			case ELF_DT_REL:
			pRelTable = (struct elf64_rel*)pCurrentDtEntry->d_ptr;
			break;
			case ELF_DT_RELA:
			pRelaTable = (struct elf64_rela*)pCurrentDtEntry->d_ptr;
			break;
			case ELF_DT_REL_SIZE:
			relSize = pCurrentDtEntry->d_value;
			break;
			case ELF_DT_RELA_SIZE:
			relaSize = pCurrentDtEntry->d_value;
			break;
			case ELF_DT_REL_ENTRY_SIZE:
			relEntrySize = pCurrentDtEntry->d_value;
			break;
			case ELF_DT_RELA_ENTRY_SIZE:
			relaEntrySize = pCurrentDtEntry->d_value;
			break;
			case ELF_DT_STRTAB:
			pDynamicStringTable = (unsigned char*)pCurrentDtEntry->d_ptr;
			break;
			case ELF_DT_SYMTAB:
			pSymTable = (struct elf64_sym*)pCurrentDtEntry->d_ptr;
			break;
		}
		pCurrentDtEntry++;
	}
	if (pRelTable&&relSize&&relEntrySize){
		uint64_t relEntries = relSize/relEntrySize;
		struct elf64_rel* pCurrentRel = (struct elf64_rel*)pRelTable;
		for (uint64_t i = 0;i<relEntries;i++,pCurrentRel = (struct elf64_rel*)((uint64_t)pCurrentRel+relEntrySize)){
			uint64_t relocType = ELF64_R_TYPE(pCurrentRel->r_info);
			switch (relocType){
				case ELF_RELOC_RELATIVE_X64:{
				uint64_t* pPatch = (uint64_t*)(pImage+pCurrentRel->r_offset);			    
				*pPatch+=imageDelta;
				break;			    
				}
			}
		}
	}
	if (pRelaTable&&relaSize&&relaEntrySize){
		uint64_t relaEntries = relaSize/relaEntrySize;
		struct elf64_rela* pCurrentRela = (struct elf64_rela*)pRelaTable;
		for (uint64_t i = 0;i<relaEntries;i++,pCurrentRela = (struct elf64_rela*)((uint64_t)pCurrentRela+relaEntrySize)){
			uint64_t relocType = ELF64_R_TYPE(pCurrentRela->r_info);
			switch (relocType){
				case ELF_RELOC_RELATIVE_X64:{
				uint64_t* pPatch = (uint64_t*)(pImage+pCurrentRela->r_offset);			    
				uint64_t patch = ((uint64_t)pImage)+pCurrentRela->r_addend;
				*pPatch = patch;
				}
			}
		}
	}
	if (!pStringTable){
		printf("string table is mandatory\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
		return -1;
	}
	pCurrentSh = pFirstSh;
	struct elf64_sym* pCurrentSymbol = (struct elf64_sym*)pDynamicSymTable;
	for (uint64_t i = 0;i<symbolCount&&pCurrentSymbol;i++,pCurrentSymbol = (struct elf64_sym*)(((uint64_t)pCurrentSymbol)+symbolEntrySize)){
		uint32_t symbolType = ELF64_SYMBOL_TYPE(pCurrentSymbol->st_info);
		uint32_t stv = ELF64_SYMBOL_STV(pCurrentSymbol->st_misc);
		unsigned char* pSymbolName = pDynamicStringTable+pCurrentSymbol->st_name;
		switch (symbolType){
			case ELF_STT_ROUTINE:
			printf("routine %s found with visibility 0x%x\r\n", pSymbolName, stv);
			switch (stv){
				case ELF_STV_DEFAULT:
				break;
				case ELF_STV_HIDDEN:
				break;
			}
			break;
			default:

			break;
		}
	}
	uint64_t entryOffset = pHeader->e_entry;
	kextEntry entry = (kextEntry)(pImage+entryOffset);
	virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
	while (1){};
	unsigned int status = entry();
	if (status!=0){
		printf("program failed with status: 0x%x\r\n", status);
		virtualFree((uint64_t)pImage, imageSize);
		return -1;
	}
	virtualFree((uint64_t)pImage, imageSize);
	return 0;
}
