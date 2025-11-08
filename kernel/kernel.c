#include "bootloader.h"
#include "graphics.h"
#include "interrupt.h"
#include "stdlib.h"
#include "filesystem.h"
#include "logo.h"
#include "timer.h"
#include "pmm.h"
#include "vmm.h"
#include "smbios.h"
#include "smp.h"
#include "serial.h"
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
	if (vmm_init()!=0){
		printf(L"failed to initialize vmm\r\n");
		while (1){};
		return -1;
	}
	printf(L"virtual memory management initialized\r\n");
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
	printf(L"Welcome to SlickOS\r\n");
	struct smbios_hdr* pCpuInfo = (struct smbios_hdr*)0x0;
	if (smbios_get_entry(SMBIOS_CPU_INFO, &pCpuInfo)!=0){
		printf(L"failed to get SMBIOS CPU info entry\r\n");
		while (1){};
		return -1;
	}
	unsigned char* cpu_manufacturer = (unsigned char*)0x0;
	if (smbios_get_string(pCpuInfo, 1, &cpu_manufacturer)!=0){
		printf(L"failed to get CPU manufacturer\r\n");
		while (1){};
		return -1;
	}
	printf_ascii("CPU manufacturer: %s\r\n", cpu_manufacturer);
	unsigned char* cpu_family = (unsigned char*)0x0;
	if (smbios_get_string(pCpuInfo, 2, &cpu_family)!=0){
		printf(L"failed to get CPU family\r\n");
		while (1){};
		return -1;
	}
	printf_ascii("CPU family: %s\r\n", cpu_family);
	unsigned int coreCnt = 0;
	if (getCpuCoreCount(&coreCnt)!=0){
		printf(L"failed to get CPU core count\r\n");
		while (1){};
		return -1;
	}
	printf(L"core count: %d\r\n", coreCnt);
	uint64_t va = 0;
	uint64_t pagecnt = (MEM_MB*512)/PAGE_SIZE;
	uint64_t before_ms = get_time_ms();
	uint64_t elapsed_ms = 0;
	if (virtualAllocPages(&va, pagecnt, PTE_RW|PTE_NX, 0)!=0){
		printf(L"failed to allocate %d pages\r\n", pagecnt);	
		while (1){};
		return -1;
	}
	elapsed_ms = get_time_ms()-before_ms;
	printf(L"took %dms to allocate %d 4KB pages\r\n", elapsed_ms, pagecnt);
	printf(L"%dGB/s allocation\r\n", 500/elapsed_ms);
	if (virtualFreePages(va, pagecnt)!=0){
		printf(L"failed to free %d pages\r\n", pagecnt);
		while (1){};
		return -1;
	}
	printf(L"took %dms to allocate and free %d pages\r\n", get_time_ms()-before_ms, pagecnt);
	while (1){};
	return 0;	
}
