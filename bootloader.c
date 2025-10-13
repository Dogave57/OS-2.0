#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
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
	uefi_printf(L"filesystem protocol: %p\r\n", (void*)filesystemProtocol);
	uefi_printf(L"drive id: %s\r\n", devicePathStr);
	status = filesystemProtocol->OpenVolume(filesystemProtocol, &rootfsProtocol);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get root volume protocol %x\r\n", status);
		while (1){};
		return EFI_ABORTED;
	}	
	EFI_FILE_PROTOCOL* testFileProtocol = (EFI_FILE_PROTOCOL*)0x0;
	status = rootfsProtocol->Open(rootfsProtocol, &testFileProtocol,  L"test.txt", EFI_FILE_MODE_READ, 0);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to open test file %x\r\n", status);
		rootfsProtocol->Close(rootfsProtocol);
		while (1){};
		return EFI_ABORTED;
	}
	uefi_printf(L"root volume protocol: %p\r\n", rootfsProtocol);
	uefi_printf(L"test file protocol: %p\r\n", testFileProtocol);
	UINTN fileInfoSize = 0;
	EFI_FILE_INFO* fileInfo = (EFI_FILE_INFO*)0x0;
	EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
	status = testFileProtocol->GetInfo(testFileProtocol, &fileInfoGuid, &fileInfoSize, NULL);
	if (status!=EFI_BUFFER_TOO_SMALL){
		uefi_printf(L"failed to get file info size: %x\n", status);
		while (1){};
		return EFI_ABORTED;
	}
	status = BS->AllocatePool(EfiBootServicesData, fileInfoSize, (void**)&fileInfo);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for file info %x\r\n", status);
		testFileProtocol->Close(testFileProtocol);
		rootfsProtocol->Close(rootfsProtocol);
		while (1){};
		return EFI_ABORTED;
	}
	status = testFileProtocol->GetInfo(testFileProtocol, &fileInfoGuid, &fileInfoSize, fileInfo);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to get file info %x\n", status);
		testFileProtocol->Close(testFileProtocol);
		rootfsProtocol->Close(rootfsProtocol);
		while (1){};
		return EFI_ABORTED;
	}
	UINTN fileSize = (UINTN)fileInfo->FileSize;
	BS->FreePool((void*)fileInfo);
	char* filedata = (char*)0x0;
	status = BS->AllocatePool(EfiBootServicesData, fileSize, (void**)&filedata);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to allocate memory for file data %x\r\n", status);
		testFileProtocol->Close(testFileProtocol);
		rootfsProtocol->Close(rootfsProtocol);
		while (1){};
		return EFI_ABORTED;
	}
	status = testFileProtocol->Read(testFileProtocol, &fileSize, (void*)filedata);
	if (status!=EFI_SUCCESS){
		uefi_printf(L"failed to read file into memory %x\r\n", status);
		testFileProtocol->Close(testFileProtocol);
		rootfsProtocol->Close(rootfsProtocol);
		while (1){};
		return EFI_ABORTED;
	}
	for (unsigned int i = 0;i<fileSize;i++){
		char ch = filedata[i];
		uefi_putchar((CHAR16)ch);
	}
	testFileProtocol->Close(testFileProtocol);
	rootfsProtocol->Close(rootfsProtocol);
	while (1){};
	return EFI_SUCCESS;
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
		case L'c':
		uefi_putchar(*((CHAR16*)parg));
		parg++;
		i++;
		break;	
		case L's':
		conout->OutputString(conout, *((CHAR16**)parg));
		parg++;
		i++;
		break;
		case L'p':
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
		break;
		default:
		uefi_putchar(ch);
		break;
		}
	}
	return 0;	
}
