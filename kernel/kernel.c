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
#include "drivers/ahci.h"
#include "drivers/nvme.h"
#include "subsystem/drive.h"
#include "subsystem/subsystem.h"
#include "subsystem/filesystem.h"
#include "drivers/gpt.h"
#include "drivers/filesystem/fat32.h"
#include "drivers/filesystem/exfat.h"
#include "crypto/guid.h"
#include "crypto/random.h"
#include "align.h"
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
	if (vmm_init()!=0){
		printf(L"failed to initialize vmm\r\n");
		while (1){};
		return -1;
	}
	if (heap_init()!=0){
		printf(L"failed to initialize heap\r\n");
		while (1){};
		return -1;
	}
	if (acpi_init()!=0){
		printf(L"failed to initialize ACPI\r\n");
		while (1){};
		return -1;
	}
	if (smbios_init()!=0){
		printf(L"failed to initialize SMBIOS\r\n");
		while (1){};
		return -1;
	}
	if (apic_init()!=0){
		printf(L"failed to initialize the APIC\r\n");
		while (1){};
		return -1;
	}
	if (pcie_init()!=0){
		printf(L"failed to initialize PCIE\r\n");
		while (1){};
		return -1;
	}
	if (ahci_init()!=0){
		printf(L"no AHCI controller available\r\n");
	}
	if (nvme_init()!=0){
		printf(L"failed to initialize NVME driver\r\n");
	}
	if (drive_subsystem_init()!=0){
		printf(L"failed to initialize drive subsystem\r\n");
		while (1){};
		return -1;
	}
	if (fs_subsystem_init()!=0){
		printf(L"failed to initialize filesystem subsystem\r\n");
		while (1){};
		return -1;
	}
	if (fat32_init()!=0){
		printf(L"failed to initialize FAT32 drivers\r\n");
		while (1){};
		return -1;
	}
	if (gpt_verify(0)!=0){
		printf(L"invalid GPT partition table\r\n");
		while (1){};
		return -1;
	}
	uint64_t va = 0;
	uint64_t pagecnt = (MEM_MB*64)/PAGE_SIZE;
	uint64_t before_ms = get_time_ms();
	if (virtualAllocPages(&va, pagecnt, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf(L"failed to allocate %d pages\r\n", pagecnt);	
		while (1){};
		return -1;
	}
	uint64_t elapsed_ms = get_time_ms()-before_ms;
	printf(L"%dGB/s allocation\r\n", (1000/((MEM_GB/PAGE_SIZE)/pagecnt))/elapsed_ms);
	if (virtualFreePages(va, pagecnt)!=0){
		printf(L"failed to free %d pages\r\n", pagecnt);
		while (1){};
		return -1;
	}
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
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(0, &gptHeader)!=0){
		printf(L"failed to get GPT header\r\n");
		while (1){};
		return -1;
	}
	struct gpt_partition esp_partition = {0};
	uint64_t esp_partition_id = 0xFFFFFFFFFFFFFFFF;
	unsigned char esp_partition_guid[16] = GPT_ESP_GUID;
	uint64_t espNumber = pbootargs->driveInfo.espNumber;
	struct gpt_partition partition = {0};
	if (gpt_get_partition(0, espNumber, &partition)!=0){
		printf(L"failed to get ESP information\r\n");
		while (1){};
		return -1;
	}
	uint64_t size = (partition.end_lba-partition.start_lba)*DRIVE_SECTOR_SIZE;
	struct fat32_mount_handle* pEspHandle = (struct fat32_mount_handle*)0x0;
	if (fat32_mount(0, espNumber, &pEspHandle)!=0){
		printf(L"failed to mount ESP\r\n");
		while (1){};
		return -1;
	}
	struct fat32_dir_handle* pDirHandle = (struct fat32_dir_handle*)0x0;
	if (fat32_opendir(pEspHandle, "EFI/BOOT/", &pDirHandle)!=0){
		printf(L"failed to open root\r\n");
		while (1){};
		return -1;
	}
	struct fat32_simple_file_entry fileEntry = {0};
	while (fat32_read_dir(pDirHandle, &fileEntry)==0){
		if (fileEntry.fileAttribs&FAT32_FILE_ATTRIBUTE_HIDDEN)
			continue;
		if (fileEntry.fileAttribs&FAT32_FILE_ATTRIBUTE_DIRECTORY){
			printf_ascii("directory: %s\r\n", fileEntry.filename);
			continue;
		}	
		printf_ascii("file: %s\r\n", fileEntry.filename);
	}
	fat32_closedir(pDirHandle);
	fat32_unmount(pEspHandle);
	for (uint64_t i = 0;i<gptHeader.partition_count;i++){
		struct gpt_partition partition = {0};
		if (gpt_get_partition(0, i, &partition)!=0)
			return -1;
		if (!partition.end_lba)
			continue;
		uint64_t id = 0;
		if (fs_mount(0, i, &id)!=0){
			printf(L"failed to mount partition %d of boot drive\r\n", i);
			continue;
		}
		printf(L"partition %d mounted with id %d\r\n", i, id);
		uint64_t file_id = 0;
		if (fs_open(id, L"TEST.TXT", 0, &file_id)!=0){
			printf(L"failed to open file with fs subsystem\r\n");
			continue;
		}
		struct fs_file_info fileInfo = {0};
		if (fs_getFileInfo(id, file_id, &fileInfo)!=0){
			printf(L"failed to get file info\r\n");
			fs_close(id, file_id);
			continue;
		}
		printf(L"file size: %d\r\n", fileInfo.fileSize);
		unsigned char* pFileBuffer = (unsigned char*)kmalloc(fileInfo.fileSize);
		if (!pFileBuffer){
			printf(L"failed to allocate file buffer\r\n");
			fs_close(id, file_id);
			continue;
		}
		if (fs_read(id, file_id, pFileBuffer, fileInfo.fileSize)!=0){
			printf(L"failed to read file\r\n");
			fs_close(id, file_id);
			kfree((void*)pFileBuffer);
			continue;
		}
		if (fs_close(id, file_id)!=0){
			printf(L"failed to close file with fs subsystem\r\n");
			continue;
		}
	}
	printf(L"dev path: %s\r\n", pbootargs->driveInfo.devicePathStr);
	while (1){};
	return 0;	
}
