#ifndef _BOOTLOADER
#define _BOOTLOADER
#include <Uefi.h>
struct bootloader_graphicsInfo{
	unsigned int width;
	unsigned int height;
	unsigned char* pfontbuffer;
};
struct bootloader_args{
	EFI_HANDLE bootloaderHandle;
	EFI_SYSTEM_TABLE* systable;		
	void* ppagetables;
	struct bootloader_graphicsInfo graphicsInfo;
};
#endif
