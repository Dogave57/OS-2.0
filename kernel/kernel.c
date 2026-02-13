#include "bootloader.h"
#include "logo.h"
#include "stdlib/stdlib.h"
#include "cpu/interrupt.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "drivers/gpu/framebuffer.h"
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
#include "subsystem/pcie.h"
#include "drivers/gpt.h"
#include "drivers/gpu/virtio.h"
#include "drivers/filesystem/fat32.h"
#include "drivers/filesystem/exfat.h"
#include "drivers/filesystem/fluxfs.h"
#include "drivers/usb/xhci.h"
#include "crypto/guid.h"
#include "crypto/random.h"
#include "kexts/loader.h"
#include "panic.h"
#include "align.h"
#include "partition_conf.h"
#include "cpu/gdt.h"
#include "cpu/thread.h"
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
		print("failed to initiailize fonts\r\n");
		while (1){};
		return -1;
	}
	BS->ExitBootServices(pbootargs->bootloaderHandle, pbootargs->memoryInfo.memoryMapKey);
	clear();
	if (serial_init()!=0){
		printf("failed to intialize serial ports\r\n");
		while (1){};
		return -1;
	}
	if (gdt_init()!=0){
		print("failed to load gdt\r\n");
		while (1){};
		return -1;
	}
	if (idt_init()!=0){
		print("failed to load idt\r\n");
		while (1){};
		return -1;
	}
	if (pmm_init()!=0){
		printf("failed to initiallize pmm\r\n");
		while (1){};
		return -1;
	}
	if (vmm_init()!=0){
		printf("failed to initialize vmm\r\n");
		while (1){};
		return -1;
	}
	if (heap_init()!=0){
		printf("failed to initialize heap\r\n");
		while (1){};
		return -1;
	}
	if (acpi_init()!=0){
		printf("failed to initialize ACPI\r\n");
		while (1){};
		return -1;
	}
	if (smbios_init()!=0){
		printf("failed to initialize SMBIOS\r\n");
		while (1){};
		return -1;
	}
	if (apic_init()!=0){
		printf("failed to initialize the APIC\r\n");
		while (1){};
		return -1;
	}
	if (pcie_init()!=0){
		printf("failed to initialize PCIe\r\n");
		while (1){};
		return -1;
	}
	if (pcie_subsystem_init()!=0){
		printf("failed to initialize PCIe subsystem\r\n");
		while (1){};
		return -1;
	}
	if (drive_subsystem_init()!=0){
		printf("failed to initialize drive subsystem\r\n");
		while (1){};
		return -1;
	}
	if (ahci_driver_init()!=0){
		printf("no AHC available\r\n");
	}
	if (nvme_driver_init()!=0){
		printf("failed to initialize NVME driver\r\n");
	}
	if (fs_subsystem_init()!=0){
		printf("failed to initialize filesystem subsystem\r\n");
		while (1){};
		return -1;
	}
	if (fat32_init()!=0){
		printf("failed to initialize FAT32 drivers\r\n");
		while (1){};
		return -1;
	}
	if (fluxfs_init()!=0){
		printf("failed to initialize fluxFS drivers\r\n");
		while (1){};
		return -1;
	}
	if (kext_subsystem_init()!=0){
		printf("failed to initialize kext subsystem\r\n");
		while (1){};
		return -1;
	}
	if (virtio_gpu_init()!=0){
		printf("failed to initialize virtual I/O GPU driver\r\n");
	}
	uint64_t usedPhysPages = 0;
	uint64_t freePhysPages = 0;
	if (getUsedPhysicalPages(&usedPhysPages)!=0){
		printf("failed to get used physical pages\r\n");
		while (1){};
		return -1;
	}
	if (getFreePhysicalPages(&freePhysPages)!=0){
		printf("failed to get free physical pages\r\n");
		while (1){};
		return -1;
	}
	if (xhci_driver_init()!=0){
		printf("failed to initialize XHCI controller\r\n");
	}
	if (gpt_verify(0)!=0){
		printf("failed to verify GPT partition table\r\n");
		while (1){};
		return -1;
	}
	uint64_t va = 0;
	uint64_t pagecnt = (MEM_MB*64)/PAGE_SIZE;
	uint64_t before_us = get_time_us();
	if (virtualAllocPages(&va, pagecnt, PTE_RW|PTE_NX, 0x0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate %d pages\r\n", pagecnt);	
		while (1){};
		return -1;
	}
	uint64_t elapsed_us = get_time_us()-before_us;
	if (elapsed_us){
		printf("%dGB/s allocation\r\n", (1000000/((MEM_GB/PAGE_SIZE)/pagecnt))/elapsed_us);
	}
	if (virtualFreePages(va, pagecnt)!=0){
		printf("failed to free %d pages\r\n", pagecnt);
		while (1){};
		return -1;
	}
	uint64_t usedPages = 0;
	uint64_t freePages = 0;
	if (getUsedPhysicalPages(&usedPages)!=0){
		printf("failed to get used physical pages\r\n");
		while (1){};
		return -1;
	}
	if (getFreePhysicalPages(&freePages)!=0){
		printf("failed to get free physical pages\r\n");
		while (1){};
		return -1;
	}
	printf("--page info--\r\n");
	struct uvec3_8 green = {0, 255, 0};
	struct uvec3_8 red = {255, 0, 0};
	struct uvec3_8 old_fg = {0,0,0};
	struct uvec3_8 old_bg = {0,0,0};
	get_text_color(&old_fg, &old_bg);
	set_text_color(red, old_bg);
	printf("used pages: %d\r\n", usedPages);
	set_text_color(green, old_bg);
	printf("free pages: %d\r\n", freePages);
	set_text_color(old_fg, old_bg);
	printf("--end of page info---\r\n");
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(0, &gptHeader)!=0){
		printf("failed to get GPT header\r\n");
		while (1){};
		return -1;
	}
	struct gpt_partition esp_partition = {0};
	uint64_t esp_partition_id = 0xFFFFFFFFFFFFFFFF;
	unsigned char esp_partition_guid[16] = GPT_ESP_GUID;
	uint64_t espNumber = pbootargs->driveInfo.espNumber;
	struct gpt_partition partition = {0};
	if (gpt_get_partition(0, espNumber, &partition)!=0){
		printf("failed to get ESP information\r\n");
		while (1){};
		return -1;
	}
	uint64_t size = (partition.end_lba-partition.start_lba)*DRIVE_SECTOR_SIZE;
	struct fat32_mount_handle* pEspHandle = (struct fat32_mount_handle*)0x0;
	if (fat32_mount(0, espNumber, &pEspHandle)!=0){
		printf("failed to mount ESP\r\n");
		while (1){};
		return -1;
	}
	struct fat32_dir_handle* pDirHandle = (struct fat32_dir_handle*)0x0;
	if (fat32_opendir(pEspHandle, "EFI/BOOT", &pDirHandle)!=0){
		printf("failed to open root\r\n");
		while (1){};
		return -1;
	}
	struct fat32_simple_file_entry fileEntry = {0};
	while (fat32_read_dir(pDirHandle, &fileEntry)==0){
		if (fileEntry.fileAttribs&FAT32_FILE_ATTRIBUTE_HIDDEN)
			continue;
		if (fileEntry.fileAttribs&FAT32_FILE_ATTRIBUTE_DIRECTORY){
			printf("directory: %s\r\n", fileEntry.filename);
			continue;
		}	
		printf("file: %s\r\n", fileEntry.filename);
	}
	fat32_closedir(pDirHandle);
	fat32_unmount(pEspHandle);
	uint64_t rootPartition = 0xFFFFFFFFFFFFFFFF;
	uint64_t rootPartitionSector = 0;
	unsigned char rootPartitionType[] = GPT_BASIC_DATA_GUID;
	struct drive_info bootDriveInfo = {0};
	uint64_t bootDriveSize = 0;
	if (drive_get_info(0, &bootDriveInfo)!=0){
		printf("failed to get boot drive info\r\n");
		while (1){};
		return -1;
	}
	bootDriveSize = bootDriveInfo.sectorCount*DRIVE_SECTOR_SIZE;
	struct gpt_partition highestPartition = {0};
	for (uint64_t i = 0;i<gptHeader.partition_count;i++){
		struct gpt_partition partition = {0};
		if (gpt_get_partition(0, i, &partition)!=0){
			break;
		}
		if (!partition.end_lba){
			continue;
		}
		if (partition.end_lba>highestPartition.end_lba){
			highestPartition = partition;
		}
		uint64_t partitionSize = (partition.end_lba-partition.start_lba)*DRIVE_SECTOR_SIZE;
		printf("partition size: %dMB\r\n", partitionSize/MEM_MB);
	}
	if (threads_init()!=0){
		printf("failed to initialize threads\r\n");
		while (1){};
		return -1;
	}
	uint64_t mountId = 0;
	if (fs_mount(0, espNumber, &mountId)!=0){
		printf("failed to mount ESP\r\n");
		while (1){};
		return -1;
	}
	uint64_t pid = 0;
	if (kext_load(mountId, "KEXTS/TEST.ELF", &pid)!=0){
		printf("failed to load kext\r\n");
		fs_unmount(mountId);
		while (1){};
		return -1;
	}
	fs_unmount(mountId);
	while (1){};
	return 0;	
}
