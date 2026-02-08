#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Guid/Acpi.h>
#include <Guid/SmBios.h>
#include <Guid/GlobalVariable.h>
#include "align.h"
#include "elf.h"
#include "bootloader.h"
int uefi_execute_kernel(void* pfiledata, uint64_t fileDataSize);
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
int uefi_dprintf(const CHAR16* fmt, ...);
int getConfTableEntry(EFI_GUID guid, void** ppEntry);
int compareGuid(EFI_GUID guid1, EFI_GUID guid2);
int uefi_strlen(CHAR16* pStr, uint64_t* pLen);
EFI_HANDLE imgHandle = {0};
EFI_SYSTEM_TABLE* ST = (EFI_SYSTEM_TABLE*)0x0;
EFI_BOOT_SERVICES* BS = (EFI_BOOT_SERVICES*)0x0;
EFI_RUNTIME_SERVICES* RT = (EFI_RUNTIME_SERVICES*)0x0;
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
struct efi_sata_dev_path{
	EFI_DEVICE_PATH_PROTOCOL hdr;
	uint16_t port;
	uint16_t portMulti;
	uint64_t logicalUnitNumber;
};
struct efi_usb_dev_path{
	EFI_DEVICE_PATH_PROTOCOL hdr;
	uint8_t portNumber;
	uint8_t interfaceNumber;
};
struct efi_gpt_partition_path{
	EFI_DEVICE_PATH_PROTOCOL hdr;
	uint32_t partitionNumber;
	uint64_t partitionlba;
	uint64_t partitionSize;
	EFI_GUID partitionTypeGuid;
	uint8_t signature[16];
	uint8_t mbrType;
	uint8_t signatureType;
};
EFI_STATUS EFIAPI UefiEntry(IN EFI_HANDLE imgHandle, IN EFI_SYSTEM_TABLE* systab){
	EFI_STATUS status = {0};
	ST = systab;
	BS = ST->BootServices;
	RT = ST->RuntimeServices;
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
	if (uefi_readfile(kernelDir, L"kernel.elf", (void**)&pbuffer, &size)!=0){
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
	unsigned int prefered_modes[][2] = {{1280, 720},{1920, 1080}, {640,480}, {320,200}};
	unsigned int prefered_modecnt = sizeof(prefered_modes)/sizeof(prefered_modes[0]);
	unsigned int mode_set = 0;
	if (!modecnt){
		conout->OutputString(conout, L"NO UEFI GRAPHICS MODE AVAILABLE!\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	status = BS->AllocatePool(EfiReservedMemoryType, sizeof(struct bootloader_args), (void**)&blargs);
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
	blargs->graphicsInfo.physicalFrameBuffer = (uint64_t)gopProtocol->Mode->FrameBufferBase;
	blargs->graphicsInfo.width = gopProtocol->Mode->Info->HorizontalResolution;
	blargs->graphicsInfo.height = gopProtocol->Mode->Info->VerticalResolution;
	blargs->graphicsInfo.virtualFrameBuffer = (struct uvec4_8*)blargs->graphicsInfo.physicalFrameBuffer;
	blargs->systable = ST;
	status = BS->GetMemoryMap(&memoryMapSize, pMemoryMap, &memoryMapKey, &memoryMapDescSize, &memoryMapDescVersion);
	if (status!=EFI_BUFFER_TOO_SMALL){
		uefi_printf(L"failed to get memory map size %x\r\n", status);
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	memoryMapSize+=(EFI_PAGE_SIZE*4);
	status = BS->AllocatePool(EfiRuntimeServicesData, memoryMapSize, (void**)&pMemoryMap);
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
	uint64_t freeMem = 0;
	for (UINTN i = 0;i<memoryMapSize/memoryMapDescSize;i++){
		EFI_MEMORY_DESCRIPTOR* memDesc = (EFI_MEMORY_DESCRIPTOR*)(((uint64_t)pMemoryMap)+(i*memoryMapDescSize));
		if (memDesc->Type!=EfiConventionalMemory)
			continue;
		freeMem+=memDesc->NumberOfPages*EFI_PAGE_SIZE;
	}
	conout->ClearScreen(conout);
	blargs->memoryInfo.pMemoryMap = pMemoryMap;
	blargs->memoryInfo.memoryMapKey = memoryMapKey;
	blargs->memoryInfo.memoryMapSize = memoryMapSize;
	blargs->memoryInfo.memoryDescSize = memoryMapDescSize;
	struct acpi_xsdp* pXsdp = (struct acpi_xsdp*)0x0;
	EFI_GUID xsdpGuid = EFI_ACPI_TABLE_GUID;
	EFI_GUID smbiosGuid = SMBIOS_TABLE_GUID;
	if (getConfTableEntry(xsdpGuid, (void**)(&pXsdp))!=0){
		conout->OutputString(conout, L"failed to get XSDP entry\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}	
	if (getConfTableEntry(smbiosGuid, (void**)(&blargs->smbiosInfo.pSmbios))!=0){
		conout->OutputString(conout, L"failed to get SMBIOS table\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	blargs->acpiInfo.pXsdp = (struct acpi_xsdp*)pXsdp;
	uint64_t devicePathStrLen = 0;
	if (uefi_strlen(devicePathStr, &devicePathStrLen)!=0){
		conout->OutputString(conout, L"failed to get boot device path length\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	CHAR16* devicePathStrCopy = (CHAR16*)0x0;
	status = BS->AllocatePool(EfiRuntimeServicesData, devicePathStrLen, (void**)&devicePathStrCopy);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for device path string copy (0x%x)\r\n", status);
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	uefi_memcpy((void*)devicePathStrCopy, (void*)devicePathStr, devicePathStrLen);
	blargs->driveInfo.devicePathStr = devicePathStr;
	conout->ClearScreen(conout);
	EFI_DEVICE_PATH_PROTOCOL* pCurrentNode = devicePathProtocol;
	blargs->driveInfo.espNumber = 0xFFFFFFFFFFFFFFFF;
	while (pCurrentNode->Type!=0x7F){
		uint64_t len = (uint64_t)((*(uint16_t*)pCurrentNode->Length));
		if (pCurrentNode->Type==0x3&&pCurrentNode->SubType==0x12){
			struct efi_sata_dev_path* pSataPath = (struct efi_sata_dev_path*)pCurrentNode;
			blargs->driveInfo.driveType = DRIVE_TYPE_SATA;
			blargs->driveInfo.port = pSataPath->port;
		}
		if (pCurrentNode->Type==0x3&&pCurrentNode->SubType==0x05){
			struct efi_usb_dev_path* pUsbPath = (struct efi_usb_dev_path*)pCurrentNode;
			blargs->driveInfo.driveType = DRIVE_TYPE_USB;
			blargs->driveInfo.port = pUsbPath->portNumber;
			blargs->driveInfo.interfaceNumber = pUsbPath->interfaceNumber;
		}
		if (pCurrentNode->Type==0x4&&pCurrentNode->SubType==0x1){
			struct efi_gpt_partition_path* pGptPath = (struct efi_gpt_partition_path*)pCurrentNode;
			blargs->driveInfo.espNumber = pGptPath->partitionNumber-1;
		}
		pCurrentNode = (EFI_DEVICE_PATH_PROTOCOL*)(((unsigned char*)pCurrentNode)+len);
	}
	if (blargs->driveInfo.driveType==DRIVE_TYPE_INVALID){
		conout->OutputString(conout, L"unsupported boot device\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	if (blargs->driveInfo.espNumber==0xFFFFFFFFFFFFFFFF){
		conout->OutputString(conout, L"failed to get ESP partiton number\r\n");
		BS->FreePool((void*)pbuffer);
		while (1){};
		return EFI_ABORTED;
	}
	if (uefi_execute_kernel((void*)pbuffer, (uint64_t)size)!=0){
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
int uefi_execute_kernel(void* pFileData, uint64_t fileDataSize){
	if (!pFileData||!fileDataSize)
		return -1;
	struct elf64_header* pHeader = (struct elf64_header*)pFileData;
	if (!ELF_VALID_SIGNATURE(pHeader))
		return -1;
	if (pHeader->e_machine!=ELF_MACHINE_X64)
		return -1;
	if (pHeader->e_type!=ELF_TYPE_DYNAMIC)
		return -1;
	uefi_dprintf(L"version: 0x%x\r\n", pHeader->e_version);
	uefi_dprintf(L"program header offset: 0x%x\r\n", pHeader->e_phoff);
	uefi_dprintf(L"section header offset: 0x%x\r\n", pHeader->e_shoff);
	uefi_dprintf(L"section header count: %d\r\n", pHeader->e_shnum);
	struct elf64_pheader* pFirstPh = (struct elf64_pheader*)(pFileData+pHeader->e_phoff);
	struct elf64_sheader* pFirstSh = (struct elf64_sheader*)(pFileData+pHeader->e_shoff);
	struct elf64_pheader* pCurrentPh = pFirstPh;
	struct elf64_sheader* pCurrentSh = pFirstSh;
	uint64_t imageSize = 0;
	uint64_t virtualBase = 0xFFFFFFFFFFFFFFFF;
	unsigned char* pImage = (unsigned char*)0x0;	
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		if (pCurrentPh->p_va<virtualBase)
			virtualBase = pCurrentPh->p_va;
		imageSize+=pCurrentPh->p_memorySize;
	}
	imageSize = align_up(imageSize, 0x1000);
	EFI_STATUS status = BS->AllocatePool(EfiReservedMemoryType, imageSize, (void**)&pImage);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate main image buffer 0x%x\r\n", status);
		return -1;
	}
	pCurrentPh = pFirstPh;
	for (uint16_t i = 0;i<pHeader->e_phnum;i++,pCurrentPh++){
		if (pCurrentPh->p_type!=ELF_PT_LOAD)
			continue;
		if (pCurrentPh->p_fileSize>pCurrentPh->p_memorySize){
			BS->FreePool((void*)pImage);
			return -1;
		}
		uint64_t virtualOffset = pCurrentPh->p_va;
		uefi_memcpy((void*)(pImage+virtualOffset), (void*)(pFileData+pCurrentPh->p_offset), pCurrentPh->p_fileSize);
		uint64_t extraBytes = pCurrentPh->p_memorySize-pCurrentPh->p_fileSize;
		for (uint64_t byte = 0;byte<extraBytes;byte++){
			uint64_t offset = virtualOffset+pCurrentPh->p_fileSize+byte;
			*(pImage+offset)=  0;
		}
	}
	int64_t imageDelta = ((int64_t)pImage)-virtualBase;
	uefi_dprintf(L"section header count: %d\r\n", pHeader->e_shnum);
	struct elf64_sym* pSymtab = (struct elf64_sym*)0x0;
	for (uint64_t i = 0;i<pHeader->e_shnum;i++,pCurrentSh++){
		uint64_t sectionData = (uint64_t)(pFileData+pCurrentSh->sh_offset);
		switch (pCurrentSh->sh_type){
			case ELF_SHT_SYMTAB:
			pSymtab = (struct elf64_sym*)sectionData;
			break;
		}
	
	}
	pCurrentSh = pFirstSh;
	for (uint16_t i = 0;i<pHeader->e_shnum;i++,pCurrentSh++){
		switch (pCurrentSh->sh_type){
		case ELF_SHT_REL:
		case ELF_SHT_RELA:
		uint64_t entrySize = pCurrentSh->sh_entrySize;
		uint64_t entryCount = pCurrentSh->sh_size/entrySize;
		unsigned char* pFirstEntry = pFileData+pCurrentSh->sh_offset;
		unsigned char* pCurrentEntry = pFirstEntry;
		uefi_dprintf(L"reloc section at va: %p\r\n", (uint64_t)pFirstEntry);
		for (uint64_t reloc = 0;reloc<entryCount;reloc++,pCurrentEntry+=entrySize){
			if (!pSymtab){
				conout->OutputString(conout, L"kernel has no symbol table!\r\n");
				return -1;
			}
			struct elf64_rela* pEntry = (struct elf64_rela*)pCurrentEntry;
			uint64_t patchOffset = pEntry->r_offset;
			uint64_t type = ELF64_R_TYPE(pEntry->r_info);
			uefi_dprintf(L"type: %d\r\n", type);
			uefi_dprintf(L"relocation: %d\r\n", reloc);
			switch (type){
			case ELF_RELOC_GLOB_DAT_X64:{	
			case ELF_RELOC_ABS_X64:
				struct elf64_sym* pSymbol = pSymtab+ELF64_R_INDEX(pEntry->r_info);
				uint64_t symbolAddress = (uint64_t)(pImage+pSymbol->st_value);
				int64_t addend = pCurrentSh->sh_type==ELF_SHT_RELA ? pEntry->r_addend : *(uint64_t*)(pImage+pEntry->r_offset);
				uefi_dprintf(L"symbol address: %d\r\n", symbolAddress);
				uint64_t patch = (type==ELF_RELOC_ABS_X64) ? symbolAddress+addend : symbolAddress;
				*(uint64_t*)(pImage+pEntry->r_offset) = patch;
				break;
			}
			case ELF_RELOC_RELATIVE_X64:{
				uint64_t* pPatch = (uint64_t*)(pImage+patchOffset);
				uint64_t addend = pCurrentSh->sh_type==ELF_SHT_RELA ? pEntry->r_addend : 0;
				*pPatch = (uint64_t)pImage+addend;
				uefi_dprintf(L"applied x64 relocation relocation with patch %d\r\n", *pPatch);
				break;
			}	    
			}
		}
		break;
		}
	}
	unsigned char* pStack = (unsigned char*)0x0;
	uint64_t stackSize = 4096*64;
	status = BS->AllocatePool(EfiReservedMemoryType, stackSize, (void**)&pStack);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate kernel stack\r\n");
		BS->FreePool((void*)pImage);
		return -1;
	}
	uint64_t entryOffset = pHeader->e_entry;
	kernelEntryType entry = (kernelEntryType)(pImage+entryOffset);
	blargs->kernelInfo.pKernel = (uint64_t)pImage;
	blargs->kernelInfo.pKernelStack = (uint64_t)pStack;
	blargs->kernelInfo.kernelSize = imageSize;
	blargs->kernelInfo.kernelStackSize = stackSize;
	blargs->kernelInfo.pKernelFileData = (uint64_t)pFileData;
	blargs->kernelInfo.kernelFileDataSize = fileDataSize;	
	uefi_dprintf(L"kernel entry offset: %d\r\n", entryOffset);
	if (entry(pStack+stackSize, blargs)!=0)
		return -1;
	while (1){};
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
	status = BS->AllocatePool(EfiLoaderData, fileInfoSize, (void**)&pFileInfo);
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
	status = BS->AllocatePool(EfiReservedMemoryType, fileSize, (void**)&pbuffer);
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
int uefi_dprintf(const CHAR16* fmt, ...){
#ifdef _BOOTLOADER_DEBUG
	uefi_printf(fmt);
#endif
	return 0;
}
int getConfTableEntry(EFI_GUID guid, void** ppEntry){
	EFI_CONFIGURATION_TABLE* pConfigTable = ST->ConfigurationTable;
	UINTN configEntryCnt = ST->NumberOfTableEntries;
	for (UINTN i = 0;i<configEntryCnt;i++){
		EFI_CONFIGURATION_TABLE* pentry = pConfigTable+i;
		EFI_GUID vendorGuid = pentry->VendorGuid;
		if (compareGuid(vendorGuid, guid)!=0)
			continue;
		*ppEntry = (void*)(pentry->VendorTable);
		return 0;
	}
	return -1;
}
int compareGuid(EFI_GUID guid1, EFI_GUID guid2){
	if (guid1.Data1!=guid2.Data1)
		return 1;
	if (guid1.Data2!=guid2.Data2)
		return 1;
	if (guid1.Data3!=guid2.Data3)
		return 1;
	if (*((uint64_t*)guid1.Data4)!=*((uint64_t*)guid2.Data4))
		return 1;
	return 0;
}
int uefi_strlen(CHAR16* pStr, uint64_t* pLen){
	if (!pStr||!pLen)
		return -1;
	uint64_t len = 0;
	for (;pStr[len];len++){};
	*pLen = len;
	return 0;
}
