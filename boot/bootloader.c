#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Guid/Acpi.h>
#include "bootloader.h"
#include "align.h"
#include "pe.h"
#include "mz.h"
int uefi_execute_elf(void* pfiledata);
int uefi_memset(void* mem, unsigned long long value, unsigned long long size);
int uefi_memcmp(void* mem1, void* mem2, uint64_t size);
int uefi_memcpy(void* dst, void* src, uint64_t size);
int uefi_readfile(EFI_FILE_PROTOCOL* pdir, CHAR16* filename, void** ppbuffer, UINTN* psize);
int uefi_scan(CHAR16* buf, unsigned int bufmax, CHAR16 terminator);
int uefi_atoi(long long num, CHAR16* buf, unsigned int bufmax);
long long uefi_stoi(CHAR16* buf);
int uefi_putchar(CHAR16 ch);
int uefi_puthex(unsigned char hex, unsigned char upper);
int uefi_printf(const CHAR16* fmt, ...);
EFI_HANDLE imgHandle = {0};
EFI_SYSTEM_TABLE* ST = (EFI_SYSTEM_TABLE*)0x0;
EFI_BOOT_SERVICES* BS = (EFI_BOOT_SERVICES*)0x0;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*)0x0;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL* conin = (EFI_SIMPLE_TEXT_INPUT_PROTOCOL*)0x0;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* stderr = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*)0x0;
EFI_LOADED_IMAGE_PROTOCOL* loadedImageProtocol = (EFI_LOADED_IMAGE_PROTOCOL*)0x0;
EFI_GUID loadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_DEVICE_PATH_PROTOCOL* devicePathProtocol = (EFI_DEVICE_PATH_PROTOCOL*)0x0;
EFI_GUID devicePathProtocolGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* devicePathToTextProtocol = (EFI_DEVICE_PATH_TO_TEXT_PROTOCOL*)0x0;
EFI_GUID devicePathToTextProtocolGuid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystemProtocol = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*)0x0;
EFI_GUID filesystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_FILE_PROTOCOL* rootfsProtocol = (EFI_FILE_PROTOCOL*)0x0;
EFI_GRAPHICS_OUTPUT_PROTOCOL* gopProtocol = (EFI_GRAPHICS_OUTPUT_PROTOCOL*)0x0;
EFI_GUID gopProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL* simpleTextInputProtocol = (EFI_SIMPLE_TEXT_INPUT_PROTOCOL*)0x0;
EFI_GUID simpleTextInputProtocolGuid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
EFI_MEMORY_DESCRIPTOR* pMemoryMap = (EFI_MEMORY_DESCRIPTOR*)0x0;
UINTN memoryMapSize = 0;
UINTN memoryMapKey = 0;
UINTN memoryMapDescSize = 0;
uint32_t memoryMapDescVersion = 0;
struct bootloader_args* blargs = (struct bootloader_args*)0x0;
EFI_STATUS EFIAPI UefiEntry(IN EFI_HANDLE imgHandle, IN EFI_SYSTEM_TABLE* systab){
	EFI_STATUS status = {0};
	ST = systab;
	BS = ST->BootServices;
	imgHandle = imgHandle;
	conout = systab->ConOut;
	stderr = systab->StdErr;
	conin = systab->ConIn;
	systab->BootServices->SetWatchdogTimer(0,0,0,NULL);
	conout->ClearScreen(conout);
	conout->OutputString(conout, L"booting kernel...\r\n");
	uefi_printf(L"image handle: %p\r\n", (void*)imgHandle);
	status = BS->HandleProtocol(imgHandle, &loadedImageProtocolGuid, (void**)&loadedImageProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get loaded image protocol %x %s %c\r\n", status, L"skibap", L'c');
		while (1){};
		return EFI_ABORTED;
	}
	uefi_printf(L"image protocol: %p\r\n", (void*)loadedImageProtocol);
	uefi_printf(L"boot drive: %x\r\n", loadedImageProtocol->DeviceHandle);
	status = BS->HandleProtocol(loadedImageProtocol->DeviceHandle, &devicePathProtocolGuid, (void**)&devicePathProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get UEFI device path protocol %x\r\n", status);
		while (1){};
		return EFI_ABORTED;	
	}
	status = BS->LocateProtocol(&devicePathToTextProtocolGuid, NULL, (void**)&devicePathToTextProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get UEFI device path to text protocol %x\r\n", status);
		while (1){};
		return EFI_ABORTED;
	}
	CHAR16* devicePathStr = devicePathToTextProtocol->ConvertDevicePathToText(devicePathProtocol, 0, 0);
	if (!devicePathStr){
		uefi_printf(L"failed to get device path in string form\r\n");
		while (1){};
		return EFI_ABORTED;
	}
	status = BS->HandleProtocol(loadedImageProtocol->DeviceHandle, &filesystemProtocolGuid, (void**)&filesystemProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get filesystem protocol %x\r\n", status);
		while (1){};
		return EFI_ABORTED;
	}
	status = BS->LocateProtocol(&simpleTextInputProtocolGuid, NULL, (void**)&simpleTextInputProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get simple text input protocol %x\r\n", status);
		while (1){};
		return EFI_ABORTED;
	}
	uefi_printf(L"filesystem protocol: %p\r\n", (void*)filesystemProtocol);
	uefi_printf(L"drive id: %s\r\n", devicePathStr);
	status = filesystemProtocol->OpenVolume(filesystemProtocol, &rootfsProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get root volume protocol %x\r\n", status);
		while (1){};
		return EFI_ABORTED;
	}	
	EFI_FILE_PROTOCOL* kernelDir = (EFI_FILE_PROTOCOL*)0x0;
	status = rootfsProtocol->Open(rootfsProtocol, &kernelDir, L"KERNEL", EFI_FILE_MODE_READ, 0);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to open kernel dir %x\r\n", status);
		while (1){};
		return EFI_ABORTED;
	}
	char* pbuffer = (char*)0x0;
	UINTN size = 0;
	if (uefi_readfile(kernelDir, L"kernel.exe", (void**)&pbuffer, &size)!=0){
		conout->OutputString(conout, L"failed to read test file\n");
		kernelDir->Close(kernelDir);
		rootfsProtocol->Close(rootfsProtocol);
		while (1){};
		return EFI_ABORTED;
	}
	kernelDir->Close(kernelDir);
	rootfsProtocol->Close(rootfsProtocol);
	status = BS->LocateProtocol(&gopProtocolGuid, NULL, (void**)&gopProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get GOP %x\r\n", status);
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	UINTN modecnt = gopProtocol->Mode->MaxMode;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* pmodeinfo = (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION*)0x0;
	UINTN infoSize = 0;
	unsigned int prefered_modes[][2] = {{1920, 1080}, {640,480},{320,200}};
	unsigned int prefered_modecnt = sizeof(prefered_modes)/sizeof(prefered_modes[0]);
	unsigned int mode_set = 0;
	if (!modecnt){
		conout->OutputString(conout, L"NO UEFI GRAPHICS MODE AVAILABLE!\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	status = BS->AllocatePool(EfiLoaderData, sizeof(struct bootloader_args), (void**)&blargs);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocatea memory for bootloader args %x\r\n", status);
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	uefi_memset((void*)blargs, 0, sizeof(struct bootloader_args));
	blargs->filesystemProtocol = filesystemProtocol;
	unsigned int graphicsWidth = 0;
	unsigned int graphicsHeight = 0;
	for (unsigned int i = 0;i<prefered_modecnt;i++){
		for (UINTN mode = 0;mode<modecnt;mode++){
			status = gopProtocol->QueryMode(gopProtocol, mode, &infoSize, &pmodeinfo);
			if (status!=EFI_SUCCESS){
				uefi_printf(L"failed to query mode %d\r\n", mode);
				BS->FreePool((void*)pmodeinfo);
				continue;
			}
			if (pmodeinfo->HorizontalResolution!=prefered_modes[i][0]||pmodeinfo->VerticalResolution!=prefered_modes[i][1]){
				BS->FreePool((void*)pmodeinfo);
				continue;
			}
			gopProtocol->SetMode(gopProtocol, mode);
			BS->FreePool((void*)pmodeinfo);
			mode_set = 1;
			break;		
		}
		if (mode_set)
			break;
	}
	if (!mode_set){
		conout->ClearScreen(conout);
		conout->OutputString(conout, L"GRAPHICS MODE NOT SET!\r\n");
		for (UINTN mode = 0;mode<modecnt;mode++){
			status = gopProtocol->QueryMode(gopProtocol, mode, &infoSize, &pmodeinfo);
			if (status!=EFI_SUCCESS){
				uefi_printf(L"failed to query mode %d\r\n", mode);
				BS->FreePool((void*)pmodeinfo);
				continue;
			}
			uefi_printf(L"%d: width: %d, height: %d\r\n", mode, pmodeinfo->HorizontalResolution, pmodeinfo->VerticalResolution);
		}
		while(1){
			conout->OutputString(conout, L"pick a graphics mode from the list: ");
			CHAR16 input[32];
			uefi_scan(input, sizeof(input), '\n');
			long long mode = uefi_stoi(input);
			if (mode==-1||mode>modecnt){
				conout->OutputString(conout, L"Invalid mode!\r\n");
				continue;
			}
			gopProtocol->SetMode(gopProtocol, mode);
			mode_set = 1;
			break;
		}
	}
	blargs->graphicsInfo.physicalFrameBuffer = (struct vec4*)gopProtocol->Mode->FrameBufferBase;
	blargs->graphicsInfo.width = gopProtocol->Mode->Info->HorizontalResolution;
	blargs->graphicsInfo.height = gopProtocol->Mode->Info->VerticalResolution;
	blargs->systable = ST;
	status = BS->GetMemoryMap(&memoryMapSize, pMemoryMap, &memoryMapKey, &memoryMapDescSize, &memoryMapDescVersion);
	if (status!=EFI_BUFFER_TOO_SMALL){
		uefi_printf(L"failed to get memory map size %x\r\n", status);
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	memoryMapSize+=EFI_PAGE_SIZE;
	status = BS->AllocatePool(EfiLoaderData, memoryMapSize, (void**)&pMemoryMap);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for memory map %x\r\n", status);
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	status = BS->GetMemoryMap(&memoryMapSize, pMemoryMap, &memoryMapKey, &memoryMapDescSize, &memoryMapDescVersion);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get memory map %x\r\n", status);
		BS->FreePool((void*)pMemoryMap);
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	conout->ClearScreen(conout);
	blargs->memoryInfo.pMemoryMap = pMemoryMap;
	blargs->memoryInfo.memoryMapKey = memoryMapKey;
	struct acpi_xsdp* pXsdp = (struct acpi_xsdp*)0x0;
	EFI_GUID acpiTableGuid = EFI_ACPI_TABLE_GUID;
	EFI_CONFIGURATION_TABLE* pConfTable = systab->ConfigurationTable;
	UINTN tableCnt = systab->NumberOfTableEntries;
	for (UINTN i = 0;i<tableCnt;i++){
		EFI_GUID guid = pConfTable[i].VendorGuid;
		if (guid.Data1!=acpiTableGuid.Data1||guid.Data2!=acpiTableGuid.Data2||guid.Data3!=acpiTableGuid.Data3)
			continue;
		if (uefi_memcmp((void*)guid.Data4, acpiTableGuid.Data4, 8)!=0)
			continue;
		
		struct acpi_xsdp* hdr = (struct acpi_xsdp*)pConfTable[i].VendorTable;
		if (hdr->signature!=XSDP_SIGNATURE)
			continue;
		pXsdp = hdr;
		break;
	}
	blargs->acpiInfo.pXsdp = (struct acpi_xsdp*)pXsdp;
	conout->ClearScreen(conout);
	if (uefi_execute_elf((void*)pbuffer)!=0){
		conout->OutputString(conout, L"failed to execute kernel!\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;		
	}
	BS->FreePool((void*)pbuffer);
	while (1){
		CHAR16 buf[16] = {0};
		uefi_scan(buf, sizeof(buf), 0);
	};
	return EFI_SUCCESS;
}
int uefi_execute_elf(void* pfiledata){
	if (!pfiledata)
		return -1;
	struct IMAGE_DOS_HEADER* pDosHeader = (struct IMAGE_DOS_HEADER*)pfiledata;
	if (pDosHeader->magic!=MZ_MAGIC){
		uefi_printf(L"unknown MZ magic %x\r\n", pDosHeader->magic);
		return -1;
	}
	struct PE_HDR* pPeHeader = (struct PE_HDR*)(pfiledata+pDosHeader->exeOffset);
	if (pPeHeader->magic!=PE_MAGIC){
		uefi_printf(L"unknown PE magic %x\r\n", pPeHeader->magic);
		return -1;
	}
	struct PE64_OPTHDR* pOptHeader = (struct PE64_OPTHDR*)(pPeHeader+1);
	if (pOptHeader->magic!=PE_OPT64_MAGIC){
		uefi_printf(L"unknown PE opt header x64 magic %x\r\n", pOptHeader->magic);
		return -1;
	}
	unsigned char* pimage = (unsigned char*)0x0;
	EFI_STATUS status = BS->AllocatePool(EfiLoaderData, pOptHeader->sizeOfImage, (void**)&pimage);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for portable executable binary image %x\r\n", status);
		return -1;
	}
	struct IMAGE_SECTION_HEADER* pFirstSection = (struct IMAGE_SECTION_HEADER*)(pOptHeader+1);
	struct IMAGE_SECTION_HEADER* pSectionHeader = pFirstSection;
	for (unsigned int i = 0;i<pPeHeader->section_cnt;i++,pSectionHeader++){
		uefi_memcpy((void*)(pimage+pSectionHeader->virtualAddress), (void*)(pfiledata+pSectionHeader->pointerToRawData), pSectionHeader->sizeOfRawData);
	}
	unsigned long long imagedt = (unsigned long long)pimage - pOptHeader->imageBase;
	struct IMAGE_DATA_DIRECTORY* pRelocDir = pOptHeader->dataDirectory+DATA_DIRECTORY_BASE_RELOC_TABLE;
	if (imagedt&&pRelocDir&&pRelocDir->size){
		unsigned char* pRelocData = (unsigned char*)(pimage+pRelocDir->virtualAddress);
		unsigned int relocOffset = 0;
		while (relocOffset<pRelocDir->size){
			struct IMAGE_BASE_RELOCATION* pBaseReloc = (struct IMAGE_BASE_RELOCATION*)(pRelocData+relocOffset);
			if (!pBaseReloc->sizeOfBlock)
				break;
			unsigned int relocCnt = (pBaseReloc->sizeOfBlock-sizeof(struct IMAGE_BASE_RELOCATION))/sizeof(uint16_t);
			uint16_t* pRelocData = pBaseReloc->relocData;
			unsigned char* patchTable = pimage+pBaseReloc->virtualAddress;
			for (unsigned int i = 0;i<relocCnt;i++){
				uint16_t* pReloc = pRelocData+i;
				unsigned int offset = (*pReloc)&0x0FFF;
				unsigned int relocType = (*pReloc)>>12;
				switch (relocType){
					case IMAGE_REL_BASED_DIR64:
					*(unsigned long long*)(patchTable+offset)+=imagedt;
					break;
				
				}
			}
			relocOffset+=pBaseReloc->sizeOfBlock;
		}
	}
	unsigned char* pbase_stack = (unsigned char*)0x0;
	unsigned int stack_size = 8192;
	status = BS->AllocatePool(EfiLoaderData, stack_size, (void**)&pbase_stack);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for stack %x\r\n", status);
		BS->FreePool((void*)pimage);
		return -1;
	}
	unsigned char* pstack = pbase_stack+stack_size;
	unsigned char* pentry = (unsigned char*)(pimage+pOptHeader->addressOfEntryPoint);
	kernelEntryType entry = (kernelEntryType)pentry;
	int kstatus = entry(pstack, blargs);
	uefi_printf(L"kernel returned with status 0x%x\r\n", kstatus);
	BS->FreePool((void*)pimage);
	return 0;
}
int uefi_memset(void* mem, unsigned long long value, unsigned long long size){
	if (!mem||!size)
		return -1;
	for (unsigned int i = 0;i<size/8;i++){
		*((unsigned long long*)mem+i) = value;
	}
	return 0;
}
int uefi_memcmp(void* mem1, void* mem2, uint64_t size){
	if (!mem1||!mem2)
		return 1;
	for (uint64_t i = 0;i<size;i++){
		if (*((unsigned char*)mem1+i)!=*((unsigned char*)mem2+i))
			return 1;
	}
	return 0;
}
int uefi_memcpy(void* dst, void* src, uint64_t size){
	if (!dst||!src)
		return -1;
	for (uint64_t i = 0;i<size;){
		if (!(size%8)){
			*((uint64_t*)((unsigned char*)dst+i)) = *((uint64_t*)((unsigned char*)src+i));
			i+=8;
			continue;
		}
		*((unsigned char*)((unsigned char*)dst+i)) = *((unsigned char*)((unsigned char*)src+i));
		i++;
	}
	return 0;	
}
int uefi_readfile(EFI_FILE_PROTOCOL* pdir, CHAR16* filename, void** ppbuffer, UINTN* psize){
	if (!pdir||!filename||!ppbuffer||!psize)
		return -1;
	EFI_FILE_PROTOCOL* pFileProtocol = (EFI_FILE_PROTOCOL*)0x0;
	UINTN fileInfoSize = 0;
	UINTN fileSize = 0;
	EFI_FILE_INFO* pFileInfo = (EFI_FILE_INFO*)0x0;
	EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
	EFI_STATUS status = pdir->Open(pdir, &pFileProtocol, filename, EFI_FILE_MODE_READ, 0);	
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to open file %x\r\n", status);
		return -1;
	}
	status = pFileProtocol->GetInfo(pFileProtocol, &fileInfoGuid, &fileInfoSize, NULL);
	if (status!=EFI_BUFFER_TOO_SMALL){
		uefi_printf(L"failed to get file info %x\r\n", status);
		return -1;
	}	
	uefi_printf(L"file info size: %x\r\n", fileInfoSize);
	status = BS->AllocatePool(EfiBootServicesData, fileInfoSize, (void**)&pFileInfo);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for file info %x\r\n", status);
		pFileProtocol->Close(pFileProtocol);
		return -1;
	}
	status = pFileProtocol->GetInfo(pFileProtocol, &fileInfoGuid, &fileInfoSize, pFileInfo);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get file info %x\r\n", status);
		pFileProtocol->Close(pFileProtocol);
		return -1;
	}
	fileSize = (UINTN)pFileInfo->FileSize;
	BS->FreePool((void*)pFileInfo);
	void* pbuffer = (void*)0x0;
	status = BS->AllocatePool(EfiBootServicesData, fileSize, (void**)&pbuffer);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for file buffer %x\r\n", status);
		pFileProtocol->Close(pFileProtocol);
		return -1;
	}
	uefi_printf(L"buffer size: %x\r\n", fileSize);
	status = pFileProtocol->Read(pFileProtocol, &fileSize, (void*)pbuffer);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to read file %x\r\n", status);
		BS->FreePool((void*)pbuffer);
		pFileProtocol->Close(pFileProtocol);
		return -1;
	}
	*ppbuffer = pbuffer;
	*psize = fileSize;
	return 0;
}
int uefi_scan(CHAR16* buf, unsigned int bufmax, CHAR16 terminator){
	if (!buf||!bufmax)
		return -1;
	unsigned int index = 0;
	EFI_INPUT_KEY key = {0};
	while (conin->ReadKeyStroke(conin, &key)==EFI_NOT_READY){};
	conin->Reset(conin, 0);
	while (1){
		EFI_EVENT event = conin->WaitForKey;
		UINTN index = 0;
		EFI_STATUS status = BS->WaitForEvent(1, &event, &index);
		status = conin->ReadKeyStroke(conin, &key);
		if (status!=EFI_SUCCESS)
			continue;
		if (index>bufmax-1)
			break;
		CHAR16 ch = key.UnicodeChar;
		if (ch=='\r'){
			uefi_putchar(ch);
			ch = '\n';
		}
		uefi_putchar(ch);
		if (ch!=terminator){
			*buf = ch;
			buf++;
			continue;
		}
		break;
	}
	*buf = 0;
	return 0;
}
int uefi_atoi(long long num, CHAR16* buf, unsigned int bufmax){
	if (!buf||bufmax<2)
		return -1;
	if (num<0){
		num*=-1;
		buf[0] = '-';
		buf++;
		bufmax--;
	}
	if (!num){
		buf[0] = L'0';
		return 0;
	}
	unsigned int digitcnt = 0;
	unsigned int tmp = num;
	while (tmp){
		digitcnt++;
		tmp/=10;
	}
	if (digitcnt>bufmax-2)
		digitcnt = bufmax-2;
	unsigned int index = 0;
	while (num){
		if (index>bufmax-2)
			break;
		unsigned int digit = num%10;
		index++;
		buf[digitcnt-index] = L'0'+digit;
		num/=10;
	}
	buf[digitcnt+1] = 0;
	return 0;
}
long long uefi_stoi(CHAR16* str){
	if (!str)
		return -1;
	long long res = 0; 
	unsigned int isNegative = 0;
	if (str[0]==L'-'){
		str++;
		isNegative = 1;
	}	
	if (str[0]==L'0'){
		return 0;	
	}
	for (unsigned int i = 0;str[i];i++){
		if (str[i]>'9'||str[i]<'0')
			return -1;
		long long digit = (long long)str[i]-L'0';
		res = res*10+digit;
	}
	if (isNegative)
		res = res*-1;
	return res;
}
int uefi_putchar(CHAR16 ch){
	CHAR16 str[2] = {ch,0};
	conout->OutputString(conout, str);
	return 0;	
}
int uefi_puthex(unsigned char hex, unsigned char upper){
	if (hex>16)
		return -1;
	if (hex<10){
		uefi_putchar(L'0'+hex);
		return 0;
	}
	if (!upper){
		uefi_putchar(L'a'+hex-10);
		return 0;
	}
	uefi_putchar(L'A'+hex-10);
	return 0;
}
int uefi_printf(const CHAR16* fmt, ...){
	if (!fmt)
		return -1;
	unsigned long long* parg = (unsigned long long*)(&fmt);
	parg++;
	for (unsigned int i = 0;fmt[i];i++){
		CHAR16 ch = fmt[i];
		if (ch!=L'%'){
			uefi_putchar(ch);
			continue;
		}
		switch(fmt[i+1]){
		case L'c':{
		uefi_putchar(*((CHAR16*)parg));
		parg++;
		i++;
		break;}	
		case L's':{
		conout->OutputString(conout, *((CHAR16**)parg));
		parg++;
		i++;
		break;}
		case L'd':{
		CHAR16 str[64] = {0};
		long long num = *((long long*)parg);
		uefi_atoi(num, str, 32);	
		conout->OutputString(conout, (CHAR16*)str);
		parg++;
		i++;
		break;}
		case L'p':{
		case L'x':
		case L'X':
		if (fmt[i+1]==L'p'){
			uefi_putchar(L'0');
			uefi_putchar(L'x');
		}
		unsigned long long hex = *parg;
		for (int s = 15;s>-1;s--){
			unsigned char byte = hex>>((s)*4);
			byte&=0xF;
			if (fmt[i+1]==L'X')
				uefi_puthex(byte, 1);
			else
				uefi_puthex(byte, 0);
		}
		parg++;
		i++;
		break;}
		default:{
		uefi_putchar(ch);
		break;
		}
		case L'a':{
		if (fmt[i+2]!=L's')
			break;
		char* str = (char*)(*(char**)parg);
		if (!str)
			break;
		for (unsigned int i = 0;str[i];i++){
			uefi_putchar((CHAR16)str[i]);
		}
		parg++;
		i+=2;
		break;
		}
		}
	}
	return 0;	
}
