#ifndef _FILESYSTEM
#define _FILESYSTEM
#include "bootloader.h"
int efi_readfile(EFI_FILE_PROTOCOL* pdir, CHAR16* filename, void** pPfilebuffer, unsigned int* pfilesize);
#endif
