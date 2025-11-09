#ifndef _BOOTLOADER
#define _BOOTLOADER
#include <Uefi.h>
#include <Uefi/UefiMultiPhase.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include "drivers/graphics.h"
#include "drivers/acpi.h"
#include "drivers/smbios.h"
struct bootloader_graphicsInfo{
	unsigned int width;
	unsigned int height;
	struct vec4* physicalFrameBuffer;
	unsigned int font_initialized;
	unsigned char* fontData;
	unsigned int fontDataSize;
	EFI_PIXEL_BITMASK pixelFormat;
};
struct bootloader_memoryInfo{
	EFI_MEMORY_DESCRIPTOR* pMemoryMap;
	UINTN memoryMapKey;
	UINTN memoryDescSize;
	unsigned int memoryMapSize;
};
struct bootloader_acpiInfo{
	struct acpi_xsdp* pXsdp;	
};
struct bootloader_smbiosInfo{
	struct smbios_eps* pSmbios;
};
struct bootloader_kernelInfo{
	uint64_t pKernel;
	uint64_t pKernelStack;
	uint64_t kernelSize;
	uint64_t kernelStackSize;
};
struct bootloader_args{
	EFI_HANDLE bootloaderHandle;
	EFI_SYSTEM_TABLE* systable;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystemProtocol;
	struct bootloader_memoryInfo memoryInfo;	
	struct bootloader_graphicsInfo graphicsInfo;
	struct bootloader_acpiInfo acpiInfo;
	struct bootloader_smbiosInfo smbiosInfo;
	struct bootloader_kernelInfo kernelInfo;
};
typedef int(*kernelEntryType)(unsigned char* pstack, struct bootloader_args*);
extern EFI_SYSTEM_TABLE* systab;
extern EFI_BOOT_SERVICES* BS;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout;
extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystemProtocol;
extern struct bootloader_args* pbootargs;
#endif
