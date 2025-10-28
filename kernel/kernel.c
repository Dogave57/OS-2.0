#include "bootloader.h"
#include "graphics.h"
#include "interrupt.h"
#include "stdlib.h"
#include "filesystem.h"
#include "logo.h"
#include "timer.h"
#include "pmm.h"
#include "apic.h"
#include "acpi.h"
#include "gdt.h"
EFI_SYSTEM_TABLE* systab = (EFI_SYSTEM_TABLE*)0x0;
EFI_BOOT_SERVICES* BS = (EFI_BOOT_SERVICES*)0x0;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*)0x0;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystemProtocol = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*)0x0;
struct bootloader_args* pbootargs = (struct bootloader_args*)0x0;
int kmain(unsigned char* pstack, struct bootloader_args* blargs){
	if (!blargs)
		return -1;
	if (!blargs->systable)
		return -1;
	systab = blargs->systable;
	BS = systab->BootServices;
	conout = systab->ConOut;
	pbootargs = blargs;
	filesystemProtocol = pbootargs->filesystemProtocol;
	if (init_fonts()!=0){
		print(L"failed to initiailize fonts\r\n");
		while (1){};
		return -1;
	}
	BS->ExitBootServices(pbootargs->bootloaderHandle, pbootargs->memoryInfo.memoryMapKey);
	clear();
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
	if (pmm_init()!=0){
		printf(L"failed to initiallize pmm\r\n");
		while (1){};
		return -1;
	}
	printf(L"physical memory management initialized\r\n");
	if (acpi_init()!=0){
		printf(L"failed to initialize ACPI\r\n");
		while (1){};
		return -1;
	}
	printf(L"ACPI initialized\r\n");
	if (apic_init()!=0){
		printf(L"failed to initialize the APIC\r\n");
		while (1){};
		return -1;
	}
	printf(L"APIC initialized\r\n");
	uint64_t start_ms = get_time_ms();
	uint64_t mem = 0;
	if (physicalAllocPage(&mem)!=0){
		printf(L"failed to allocate page\r\n");
		while (1){};
		return -1;
	}
	uint64_t elapsed_ms = get_time_ms()-start_ms;
	printf(L"took %dms to allocate page\r\n", elapsed_ms);
	printf(L"Welcome to SlickOS\r\n");
	printf(L"%s\r\n", logo);
	while (1){};
	return 0;	
}
