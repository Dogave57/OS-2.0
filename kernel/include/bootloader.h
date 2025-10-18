#ifndef _BOOTLOADER
#define _BOOTLOADER
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
struct bootloader_graphicsInfo{
	unsigned int width;
	unsigned int height;
	unsigned char* physicalFrameBuffer;
	unsigned char* pfontbuffer;
	EFI_PIXEL_BITMASK pixelFormat;
};
struct bootloader_memoryInfo{
	EFI_MEMORY_DESCRIPTOR* pMemoryMap;
	unsigned int memoryMapSize;
};
struct bootloader_args{
	EFI_HANDLE bootloaderHandle;
	EFI_SYSTEM_TABLE* systable;
	void* ppagetables;
	struct bootloader_memoryInfo memoryInfo;	
	struct bootloader_graphicsInfo graphicsInfo;
};
typedef int(*kernelEntryType)(unsigned char* pstack, struct bootloader_args*);
extern struct bootloader_args* pbootargs;
#endif
