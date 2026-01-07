#include "kernel_include.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "drivers/timer.h"
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
	uint64_t time_us = get_time_us();
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
	struct elf_handle* pHandle = (struct elf_handle*)kmalloc(sizeof(struct elf_handle));
	if (!pHandle){
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);	
		return -1;
	}
	memset((void*)pHandle, 0, sizeof(struct elf_handle));
	pHandle->fileInfo = fileInfo;
	pHandle->pFileBuffer = pFileBuffer;
	pHandle->pImage = pImage;
	pHandle->imageSize = imageSize;
	struct elf_handle* pBaseKernelHandle = (struct elf_handle*)kmalloc(sizeof(struct elf_handle));
	if (!pBaseKernelHandle){
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
		return -1;
	}
	memset((void*)pBaseKernelHandle, 0, sizeof(struct elf_handle));
	pBaseKernelHandle->pFileBuffer = (unsigned char*)pbootargs->kernelInfo.pKernelFileData;
	pBaseKernelHandle->pImage = (unsigned char*)pbootargs->kernelInfo.pKernel;
	pBaseKernelHandle->imageSize = pbootargs->kernelInfo.kernelSize;
	pHandle->pDynamicLibs = pBaseKernelHandle;
	if (elf_cache_dt(pBaseKernelHandle)!=0){
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
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
		kfree((void*)pHandle);
		return -1;
	}
	struct elf64_rel* pRelTable = (struct elf64_rel*)0x0;
	struct elf64_rela* pRelaTable = (struct elf64_rela*)0x0;
	struct elf64_sym* pSymbolTable = (struct elf64_sym*)0x0;
	unsigned char* pStringTable = (unsigned char*)0x0;
	uint64_t stringTableSize = 0;
	uint64_t relTableEntries = 0;
	uint64_t relaTableEntries = 0;
	struct elf64_hash* pHashData = (struct elf64_hash*)0x0;
	uint32_t* pHashChain = (uint32_t*)0x0;
	for (uint64_t i = 0;i<dynamicTableEntries;i++){
		struct elf64_dyn* pCurrentEntry = pDynamicTable+i;
		if (pCurrentEntry->d_tag>(sizeof(pHandle->dynEntries)/sizeof(struct elf64_dyn))-1)
			continue;
		pHandle->dynEntries[pCurrentEntry->d_tag] = *pCurrentEntry;
	}
	elf_get_dt_ptr(pHandle, ELF_DT_REL, (uint64_t*)&pRelTable);
	elf_get_dt_ptr(pHandle, ELF_DT_RELA, (uint64_t*)&pRelaTable);
	elf_get_dt_ptr(pHandle, ELF_DT_SYMTAB, (uint64_t*)&pSymbolTable);
	elf_get_dt_ptr(pHandle, ELF_DT_STRTAB, (uint64_t*)&pStringTable);
	elf_get_dt_value(pHandle, ELF_DT_STRTAB_SIZE, &stringTableSize);
	elf_get_dt_value(pHandle, ELF_DT_REL_SIZE, &relTableEntries);
	relTableEntries/=sizeof(struct elf64_rel);
	elf_get_dt_value(pHandle, ELF_DT_RELA_SIZE, &relaTableEntries);
	relaTableEntries/=sizeof(struct elf64_rela);
	elf_get_dt_ptr(pHandle, ELF_DT_HASH, (uint64_t*)&pHashData);
	if (!pStringTable){
		printf("no string table\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
		kfree((void*)pHandle);
		return -1;
	}
	if (!pSymbolTable){
		printf("no symbol table\r\n");
		virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
		virtualFree((uint64_t)pImage, imageSize);
		kfree((void*)pHandle);
		return -1;
	}
	if (pHashData)
		pHashChain = pHashData->buckets+pHashData->nBuckets;
	for (uint64_t i = 0;i<dynamicTableEntries;i++){
		struct elf64_dyn* pCurrentEntry = pDynamicTable+i;
		switch (pCurrentEntry->d_tag){
			case ELF_DT_NEEDED:{
			unsigned char* pDlName = pStringTable+pCurrentEntry->d_value;
			if (strcmp(pDlName, "kernel.elf")==0){
				continue;
			}
			struct elf_handle* pDlHandle = (struct elf_handle*)0x0;
			if (elf_load_dl(pHandle, pDlName, &pDlHandle)!=0){
				printf("failed to load %s\r\n", pDlName);
				kfree((void*)pHandle);
				return -1;
			}
			break;
			}
		}
	}
	for (uint64_t i = 0;pRelTable&&i<relTableEntries;i++){
		struct elf64_rel* pCurrentEntry = (struct elf64_rel*)(pRelTable+i);
		uint64_t relocType = ELF64_R_TYPE(pCurrentEntry->r_info);
		uint64_t relocSymIndex = ELF64_R_INDEX(pCurrentEntry->r_info);
		switch (relocType){
			case ELF_RELOC_GLOB_DAT_X64:{
			struct elf64_sym* pSymbol = pSymbolTable+relocSymIndex;
			unsigned char* pSymbolName = pStringTable+pSymbol->st_name;
			uint64_t* pPatch = (uint64_t*)(pImage+pCurrentEntry->r_offset);
			*pPatch = (uint64_t)print;
			break;
		        }
		}
	}
	for (uint64_t i = 0;pRelaTable&&i<relaTableEntries;i++){
		struct elf64_rela* pCurrentEntry = (struct elf64_rela*)(pRelaTable+i);
		uint64_t relocType = ELF64_R_TYPE(pCurrentEntry->r_info);
		uint64_t relocSymIndex = ELF64_R_INDEX(pCurrentEntry->r_info);
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
			struct elf_handle* pCurrentHandle = pHandle->pDynamicLibs;
			uint64_t symbolIndex = 0;
			struct elf64_sym* pLibSymbolTable = (struct elf64_sym*)0x0;
			while (1){
				if (!pCurrentHandle){
					printf("failed to resolve dynamic symbol %s\r\n", pSymbolName);
					virtualFree((uint64_t)pFileBuffer, fileInfo.fileSize);
					virtualFree((uint64_t)pImage, imageSize);
					return -1;
				}
				if (elf_find_symbol(pCurrentHandle, pSymbolName, &symbolIndex)!=0){
					pCurrentHandle = pCurrentHandle->pFlink;
					continue;
				}
				if (elf_get_dt_ptr(pCurrentHandle, ELF_DT_SYMTAB, (uint64_t*)&pLibSymbolTable)!=0){
					pCurrentHandle = pCurrentHandle->pFlink;
					continue;
				}
				break;
			}
			struct elf64_sym* pExportSymbol = pLibSymbolTable+symbolIndex;
			uint64_t symbolAddress = (uint64_t)(pCurrentHandle->pImage+pExportSymbol->st_value);
			*pPatch = symbolAddress;
			break;
			}
		}
	}
	*ppHandle = pHandle;
	return 0;
}
int elf_get_entry(struct elf_handle* pHandle, uint64_t* ppEntry){
	if (!pHandle||!ppEntry)
		return -1;
	if (!pHandle->pFileBuffer||!pHandle->pImage)
		return -1;
	struct elf64_header* pHeader = (struct elf64_header*)pHandle->pFileBuffer;
	uint64_t entryOffset = pHeader->e_entry;
	uint64_t pEntry = ((uint64_t)pHandle->pImage)+entryOffset;
	*ppEntry = pEntry;
	return 0;
}
int elf_load_dl(struct elf_handle* pHandle, unsigned char* dlname, struct elf_handle** ppDlHandle){
	if (!pHandle||!dlname||!ppDlHandle)
		return -1;
	struct elf_handle* pCurrentDl = pHandle->pDynamicLibs;
	struct elf_handle* pLastDlEntry = pCurrentDl;
	while (pCurrentDl){
		if (strcmp(pCurrentDl->fileInfo.filename, dlname)==0)
			return -1;
		pLastDlEntry = pCurrentDl;
		pCurrentDl = pCurrentDl->pFlink;
	}
	struct elf_handle* pDlHandle = (struct elf_handle*)0x0;
	if (elf_load(pHandle->fileInfo.mount_id, dlname, &pDlHandle)!=0)
		return -1;
	pLastDlEntry->pFlink = pDlHandle;
	pDlHandle->pBlink = pLastDlEntry;
	pHandle->dl_cnt++;
	*ppDlHandle = pDlHandle;
	return 0;
}
int elf_unload_dl(struct elf_handle* pHandle, struct elf_handle* pDlHandle){
	if (!pDlHandle)
		return -1;
	if (pDlHandle->pFlink)
		pDlHandle->pFlink->pBlink = pDlHandle->pBlink;
	if (pDlHandle->pBlink)
		pDlHandle->pBlink->pFlink = pDlHandle->pFlink;
	if (pHandle)
		pHandle->dl_cnt--;
	if (!pDlHandle->fileInfo.fileSize)
		return 0;
	if (elf_unload(pDlHandle)!=0)
		return -1;
	return 0;
}
int elf_unload(struct elf_handle* pHandle){
	if (!pHandle)
		return -1;
	if (pHandle->pFileBuffer)
		virtualFree((uint64_t)pHandle->pFileBuffer, pHandle->fileInfo.fileSize);
	if (pHandle->pImage)
		virtualFree((uint64_t)pHandle->pImage, pHandle->imageSize);
	struct elf_handle* pCurrentDlHandle = pHandle->pDynamicLibs;
	while (pCurrentDlHandle){
		struct elf_handle* pNext = pCurrentDlHandle->pFlink;
		if (elf_unload_dl(pHandle, pCurrentDlHandle)!=0){
			kfree((void*)pHandle);
			return -1;
		}
		pCurrentDlHandle = pNext;	
	}
	kfree((void*)pHandle);
	return 0;
}
int elf_cache_dt(struct elf_handle* pHandle){
	if (!pHandle)
		return -1;
	struct elf64_header* pHeader = (struct elf64_header*)pHandle->pFileBuffer;
	if (!ELF_VALID_SIGNATURE(pHandle->pFileBuffer))
		return -1;
	struct elf64_pheader* pFirstPh = (struct elf64_pheader*)(pHandle->pFileBuffer+pHeader->e_phoff);
	struct elf64_pheader* pCurrentPh = pFirstPh;
	struct elf64_dyn* pDynamicTable = (struct elf64_dyn*)0x0;
	uint64_t dynamicTableEntries = 0;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_DYNAMIC)
			continue;
		pDynamicTable = (struct elf64_dyn*)(pHandle->pImage+pCurrentPh->p_va);
		dynamicTableEntries = pCurrentPh->p_memorySize/sizeof(struct elf64_dyn);
	}
	if (!pDynamicTable)
		return -1;
	for (uint64_t i = 0;i<dynamicTableEntries;i++){
		struct elf64_dyn* pCurrentEntry = pDynamicTable+i;
		if (pCurrentEntry->d_tag>(sizeof(pHandle->dynEntries)/sizeof(struct elf64_dyn))-1)
			continue;
		pHandle->dynEntries[pCurrentEntry->d_tag] = *pCurrentEntry;
	}
	return 0;
}
int elf_find_symbol(struct elf_handle* pHandle, unsigned char* symbolName, uint64_t* pSymbolIndex){
	if (!pHandle||!symbolName||!pSymbolIndex)
		return -1;
	if (elf_find_symbol_by_hash(pHandle, symbolName, pSymbolIndex)==0)
		return 0;
	return -1;
}
int elf_find_symbol_by_hash(struct elf_handle* pHandle, unsigned char* symbolName, uint64_t* pSymbolIndex){
	if (!pHandle||!symbolName||!pSymbolIndex)
		return -1;
	uint32_t hash = 0;
	if (elf_hash(symbolName, &hash)!=0){
		printf("failed to get hash\r\n");
		return -1;
	}
	struct elf64_hash* pHashData = (struct elf64_hash*)0x0;
	struct elf64_sym* pSymbolTable = (struct elf64_sym*)0x0;
	unsigned char* pStringTable = (unsigned char*)0x0;
	if (elf_get_dt_ptr(pHandle, ELF_DT_HASH, (uint64_t*)&pHashData)!=0){
		return -1;
	}
	if (elf_get_dt_ptr(pHandle, ELF_DT_SYMTAB, (uint64_t*)&pSymbolTable)!=0)
		return -1;
	if (elf_get_dt_ptr(pHandle, ELF_DT_STRTAB, (uint64_t*)&pStringTable)!=0)
		return -1;
	if (!pHashData||!pSymbolTable||!pStringTable){
		return -1;
	}
	uint32_t* pChainData = pHashData->buckets+pHashData->nBuckets;
	uint64_t hashBucket = hash%pHashData->nBuckets;
	uint64_t chainStart = (uint64_t)pHashData->buckets[hashBucket];
	if (!chainStart){
		return -1;
	}
	for (uint64_t i = chainStart;i!=0;i = pChainData[i]){
		uint64_t symbolIndex = (uint64_t)pChainData[i];
		struct elf64_sym* pSymbol = pSymbolTable+i;
		unsigned char* pName = pStringTable+pSymbol->st_name;
		if (strcmp(symbolName, pName)!=0)
			continue;
		*pSymbolIndex = i;
		return 0;
	}
	return -1;
}
int elf_get_dt_value(struct elf_handle* pHandle, uint64_t type, uint64_t* pValue){
	if (!pHandle||!pValue)
		return -1;
	if (type>(sizeof(pHandle->dynEntries)/sizeof(struct elf64_dyn))-1)
		return -1;
	*pValue = pHandle->dynEntries[type].d_value;
	return 0;
}
int elf_get_dt_ptr(struct elf_handle* pHandle, uint64_t type, uint64_t* pPtr){
	if (!pHandle||!type||!pPtr)
		return -1;
	if (type>(sizeof(pHandle->dynEntries)/sizeof(struct elf64_dyn))-1)
		return -1;
	uint64_t rva = pHandle->dynEntries[type].d_ptr;
	if (!rva)
		return -1;
	*pPtr = (uint64_t)(pHandle->pImage+rva);
	return 0;
}
int elf_hash(unsigned char* data, uint32_t* pHash){
	if (!data||!pHash)
		return -1;
	uint32_t hash = 0;
	uint32_t entropy = 0;
	while (*data){
		hash = (hash<<4) + *data++;
		entropy = hash&0xF0000000;
		if (!entropy)
			continue;
		hash^=entropy>>24;
		hash&=~entropy;
	}
	*pHash = hash;
	return 0;
}
