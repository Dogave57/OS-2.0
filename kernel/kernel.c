#include "bootloader.h"
#include "graphics.h"
#include "interrupt.h"
#include "stdlib.h"
#include "gdt.h"
EFI_SYSTEM_TABLE* systab = (EFI_SYSTEM_TABLE*)0x0;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*)0x0;
struct bootloader_args* pbootargs = (struct bootloader_args*)0x0;
int kmain(unsigned char* pstack, struct bootloader_args* blargs){
	if (!blargs)
		return -1;
	if (!blargs->systable)
		return -1;
	systab = blargs->systable;
	conout = systab->ConOut;
	pbootargs = blargs;
	print(L"kernel loaded\r\n"); 
	if (gdt_init()!=0){
		print(L"failed to load gdt\r\n");
		while (1){};
		return -1;
	}
	print(L"gdt loaded\r\n");
	if (idt_init()!=0){
		print(L"failed to load idt\r\n");
		while (1){};
		return -1;
	}
	print(L"idt loaded\r\n");
	printf(L"test: %d\r\n", 0);
	while (1){};
	return 0;	
}
