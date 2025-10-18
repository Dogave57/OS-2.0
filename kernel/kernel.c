#include "bootloader.h"
#include "graphics.h"
struct bootloader_args* pbootargs = (struct bootloader_args*)0x0;
int kmain(unsigned char* pstack, struct bootloader_args* blargs){
	if (!blargs)
		return -1;
	if (!blargs->systable)
		return -1;
	pbootargs = blargs;
	EFI_SYSTEM_TABLE* systab = blargs->systable;
	if (!systab)
		return -1;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout = systab->ConOut;
	if (!conout)
		return -1;
	struct bootloader_graphicsInfo graphicsInfo = blargs->graphicsInfo;
	conout->OutputString(conout, L"kernel loaded\r\n");
	while (1){};
	return 0;	
}
