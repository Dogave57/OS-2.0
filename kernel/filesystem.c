#include "stdlib.h"
#include "filesystem.h"
int efi_readfile(EFI_FILE_PROTOCOL* pdir, CHAR16* filename, unsigned char* pfilebuffer, unsigned int* pfilesize){
	if (!pdir||!filename||!pfilebuffer||!pfilesize)
		return -1;
	EFI_FILE_PROTOCOL* pFileProtocol = (EFI_FILE_PROTOCOL*)0x0;
	UINTN fileSize = 0;
	EFI_FILE_INFO* pFileInfo = (EFI_FILE_INFO*)0x0;
	EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
	EFI_STATUS status = pdir->Open(pdir, &pFileProtocol, filename, EFI_FILE_MODE_READ, 0);
	if (status!=EFI_SUCCESS){
		printf(L"failed to open file %x\r\n", status);
		return -1;
	}
	return 0;
}
