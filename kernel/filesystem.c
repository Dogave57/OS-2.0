#include "stdlib.h"
#include "filesystem.h"
int efi_readfile(EFI_FILE_PROTOCOL* pdir, CHAR16* filename, void** pPfilebuffer, unsigned int* pfilesize){
	if (!pdir||!filename||!pPfilebuffer||!pfilesize)
		return -1;
	EFI_FILE_PROTOCOL* pFileProtocol = (EFI_FILE_PROTOCOL*)0x0;
	UINTN fileSize = 0;
	EFI_FILE_INFO* pFileInfo = (EFI_FILE_INFO*)0x0;
	EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
	UINTN fileInfoSize = 0;
	EFI_STATUS status = pdir->Open(pdir, &pFileProtocol, filename, EFI_FILE_MODE_READ, 0);
	if (status!=EFI_SUCCESS){
		printf(L"failed to open file %x\r\n", status);
		return -1;
	}
	status = pFileProtocol->GetInfo(pFileProtocol, &fileInfoGuid, &fileInfoSize, NULL);
	if (status!=EFI_BUFFER_TOO_SMALL){
		printf(L"failed to get file info size %x\r\n", status);
		return -1;
	}
	status = BS->AllocatePool(EfiLoaderData, fileInfoSize, (void**)&pFileInfo);
	if (status!=EFI_SUCCESS){
		printf(L"failed to allocate memory for file data %x\r\n", status);
		return -1;
	}
	status = pFileProtocol->GetInfo(pFileProtocol, &fileInfoGuid, &fileInfoSize, pFileInfo);
	if (status!=EFI_SUCCESS){
		printf(L"failed to get file info %x\r\n", status);
		BS->FreePool((void*)pFileInfo);
		return -1;
	}
	fileSize = pFileInfo->FileSize;
	BS->FreePool((void*)pFileInfo);
	unsigned char* pFileBuffer = (unsigned char*)0x0;
	status = BS->AllocatePool(EfiLoaderData, fileSize, (void**)&pFileBuffer);
	if (status!=EFI_SUCCESS){
		printf(L"failed to allocate memory for file data %x\r\n", status);
		return -1;
	}
	status = pFileProtocol->Read(pFileProtocol, &fileSize, (void*)pFileBuffer);
	if (status!=EFI_SUCCESS){
		printf(L"failed to read file into memory %x\r\n", status);
		BS->FreePool((void*)pFileBuffer);
		return -1;
	}
	*pPfilebuffer = (void*)pFileBuffer;
	*pfilesize = fileSize;
	return 0;
}
