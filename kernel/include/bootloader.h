#ifndef _BOOTLOADER
#define _BOOTLOADER
#include <Uefi.h>
#include <Uefi/UefiMultiPhase.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include "subsystem/drive.h"
#include "drivers/gpu/framebuffer.h"
#include "drivers/acpi.h"
#include "drivers/smbios.h"
struct bootloader_graphicsInfo{
	unsigned int width;
	unsigned int height;
	uint64_t physicalFrameBuffer;
	struct uvec4_8* virtualFrameBuffer;
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
	uint64_t pKernelFileData;
	uint64_t kernelFileDataSize;
};
struct bootloader_driveInfo{
	CHAR16* devicePathStr;
	uint64_t driveType;
	uint64_t espNumber;
	uint64_t port;
	uint64_t extra;
};
struct efi_sata_dev_path{
	EFI_DEVICE_PATH_PROTOCOL header;
	uint16_t port;
	uint16_t portMultiplier;
	uint64_t logicalUnitNumber;
}__attribute__((packed));
struct efi_usb_dev_path{
	EFI_DEVICE_PATH_PROTOCOL header;
	uint8_t portNumber;
	uint8_t interfaceNumber;
}__attribute__((packed));
struct efi_nvme_dev_path{
	EFI_DEVICE_PATH_PROTOCOL header;
	uint32_t namespaceId;
	uint8_t ieee[8];
}__attribute__((packed));
struct efi_pcie_path{
	EFI_DEVICE_PATH_PROTOCOL header;
	uint8_t function;
	uint8_t device;
}__attribute__((packed));
struct efi_gpt_partition_path{
	EFI_DEVICE_PATH_PROTOCOL header;
	uint32_t partitionNumber;
	uint64_t partitionLba;
	uint64_t partitionSize;
	EFI_GUID partitionTypeGuid;
	uint8_t signature[16];
	uint8_t mbrType;
	uint8_t signatureType;
}__attribute__((packed));
struct bootloader_args{
	EFI_HANDLE bootloaderHandle;
	EFI_SYSTEM_TABLE* systable;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystemProtocol;
	EFI_DEVICE_PATH_PROTOCOL* pDevicePathProtocol;
	uint64_t devicePathProtocolSize;
	struct bootloader_memoryInfo memoryInfo;	
	struct bootloader_graphicsInfo graphicsInfo;
	struct bootloader_acpiInfo acpiInfo;
	struct bootloader_smbiosInfo smbiosInfo;
	struct bootloader_kernelInfo kernelInfo;
	struct bootloader_driveInfo driveInfo;
};
typedef int(*kernelEntryType)(unsigned char* pstack, struct bootloader_args*);
extern EFI_SYSTEM_TABLE* systab;
extern EFI_BOOT_SERVICES* BS;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout;
extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystemProtocol;
extern struct bootloader_args* pbootargs;
#endif
