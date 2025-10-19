#ifndef _FILESYSTEM
#define _FILESYSTEM
#include "bootloader.h"
int efi_readfile(EFI_FILE_PROTOCOL* pdir, CHAR16* filename, unsigned char* pfilebuffer, unsigned int* pfilesize);
#endif
