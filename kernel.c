#include "bootloader.h"
CHAR16* test = L"asdfasdfas\n";
int kmain(struct bootloader_args* blargs){
	if (!blargs)
		return -1;
	EFI_SYSTEM_TABLE* systab = blargs->systable;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout = systab->ConOut;
	conout->OutputString(conout, test);
	return 0;	
}
