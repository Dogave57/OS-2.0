#include "bootloader.h"
#include "logo.h"
#include "stdlib/stdlib.h"
#include "cpu/interrupt.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "drivers/graphics.h"
#include "drivers/smbios.h"
#include "drivers/smp.h"
#include "drivers/serial.h"
#include "drivers/apic.h"
#include "drivers/acpi.h"
#include "drivers/pcie.h"
#include "drivers/filesystem.h"
#include "drivers/timer.h"
#include "cpu/gdt.h"
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
	if (serial_init()!=0){
		printf(L"failed to intialize serial ports\r\n");
		while (1){};
		return -1;
	}
	if (gdt_init()!=0){
		print(L"failed to load gdt\r\n");
		while (1){};
		return -1;
	}
	if (idt_init()!=0){
		print(L"failed to load idt\r\n");
		while (1){};
		return -1;
	}
	if (pmm_init()!=0){
		printf(L"failed to initiallize pmm\r\n");
		while (1){};
		return -1;
	}
	printf(L"physical memory management initialized\r\n");
	if (vmm_init()!=0){
		printf(L"failed to initialize vmm\r\n");
		while (1){};
		return -1;
	}
	printf(L"virtual memory management initialized\r\n");
	if (heap_init()!=0){
		printf(L"failed to initialize heap\r\n");
		while (1){};
		return -1;
	}
	printf(L"heap initialized\r\n");
	if (acpi_init()!=0){
		printf(L"failed to initialize ACPI\r\n");
		while (1){};
		return -1;
	}
	printf(L"ACPI initialized\r\n");
	if (smbios_init()!=0){
		printf(L"failed to initialize SMBIOS\r\n");
		while (1){};
		return -1;
	}
	printf(L"SMBIOS initialized\r\n");
	if (apic_init()!=0){
		printf(L"failed to initialize the APIC\r\n");
		while (1){};
		return -1;
	}
	printf(L"APIC initialized\r\n");
	if (pcie_init()!=0){
		printf(L"failed to initialize PCIE\r\n");
		while (1){};
		return -1;
	}
	printf(L"Welcome to SlickOS\r\n");
	uint64_t va = 0;
	uint64_t pagecnt = (MEM_MB*256)/PAGE_SIZE;
	uint64_t before_ms = get_time_ms();
	uint64_t elapsed_ms = 0;
	if (virtualAllocPages(&va, pagecnt, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf(L"failed to allocate %d pages\r\n", pagecnt);	
		while (1){};
		return -1;
	}
	elapsed_ms = get_time_ms()-before_ms;
	printf(L"took %dms to allocate %d 4KB pages\r\n", elapsed_ms, pagecnt);
	printf(L"%dGB/s allocation\r\n", 250/elapsed_ms);
	if (virtualFreePages(va, pagecnt)!=0){
		printf(L"failed to free %d pages\r\n", pagecnt);
		while (1){};
		return -1;
	}
	printf(L"took %dms to allocate and free %d pages\r\n", get_time_ms()-before_ms, pagecnt);
	uint64_t usedPages = 0;
	uint64_t freePages = 0;
	if (getUsedPhysicalPages(&usedPages)!=0){
		printf(L"failed to get used physical pages\r\n");
		while (1){};
		return -1;
	}
	if (getFreePhysicalPages(&freePages)!=0){
		printf(L"failed to get free physical pages\r\n");
		while (1){};
		return -1;
	}
	printf(L"--page info--\r\n");
	struct vec3 green = {0, 255, 0};
	struct vec3 red = {255, 0, 0};
	struct vec3 old_fg = {0,0,0};
	struct vec3 old_bg = {0,0,0};
	get_text_color(&old_fg, &old_bg);
	set_text_color(red, old_bg);
	printf(L"used pages: %d\r\n", usedPages);
	set_text_color(green, old_bg);
	printf(L"free pages: %d\r\n", freePages);
	set_text_color(old_fg, old_bg);
	printf(L"--end of page info---\r\n");
	while (1){};
	return 0;	
}
