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
#include "drivers/filesystem/fluxfs.h"
#include "crypto/guid.h"
#include "crypto/random.h"
#include "kexts/loader.h"
#include "panic.h"
#include "align.h"
#include "partition_conf.h"
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
	printf("VMM initialized\r\n");
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
		printf("failed to initialize PCIE\r\n");
		while (1){};
		return -1;
	}
	if (ahci_init()!=0){
		printf("no AHCI controller available\r\n");
	}
	printf("initialized AHCI controller\r\n");
	if (nvme_init()!=0){
		printf("failed to initialize NVME driver\r\n");
	}
	if (drive_subsystem_init()!=0){
		printf("failed to initialize drive subsystem\r\n");
		while (1){};
		return -1;
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
	if (gpt_verify(0)!=0){
		printf("invalid GPT partition table\r\n");
		while (1){};
		return -1;
	}
	uint64_t va = 0;
	uint64_t pagecnt = (MEM_MB*64)/PAGE_SIZE;
	uint64_t before_ms = get_time_ms();
	if (virtualAllocPages(&va, pagecnt, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate %d pages\r\n", pagecnt);	
		while (1){};
		return -1;
	}
	uint64_t elapsed_ms = get_time_ms()-before_ms;
	if (elapsed_ms)
		printf("%dGB/s allocation\r\n", (1000/((MEM_GB/PAGE_SIZE)/pagecnt))/elapsed_ms);
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
	struct vec3 green = {0, 255, 0};
	struct vec3 red = {255, 0, 0};
	struct vec3 old_fg = {0,0,0};
	struct vec3 old_bg = {0,0,0};
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
	bootDriveSize = bootDriveInfo.sector_count*DRIVE_SECTOR_SIZE;
	struct gpt_partition highestPartition = {0};
	for (uint64_t i = 0;i<gptHeader.partition_count;i++){
		struct gpt_partition partition = {0};
		if (gpt_get_partition(0, i, &partition)!=0){
			printf("failed to get partition %d\r\n", i);
			while (1){};
			continue;
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
	uint64_t fileId = 0;
	uint64_t mountId = 0;
	if (fs_mount(0, espNumber, &mountId)!=0){
		printf("failed to mount ESP\r\n");
		while (1){};
		return -1;
	}
	if (fs_open(mountId, "CONFIG\\PART.CFG", 0, &fileId)!=0){
		uint64_t rootPartitionStart = partition.end_lba*DRIVE_SECTOR_SIZE;
		uint64_t rootPartitionSize = bootDriveSize-rootPartitionStart;
		struct gpt_partition rootPartitionData = {0};
		rootPartitionStart+=DRIVE_SECTOR_SIZE;
		if (gpt_add_partition(0, "ROOTFS", rootPartitionStart, rootPartitionSize, 0, rootPartitionType, &rootPartition)!=0){
			printf("failed to create root partition\r\n");
			while (1){};
			return -1;
		}
		if (gpt_get_partition(0, rootPartition, &rootPartitionData)!=0){
			printf("failed to get root partition data\r\n");
			while (1){};
			return -1;
		}
		uint64_t time_ms = get_time_ms();
		printf("creating partition config\r\n");
		if (fs_create(mountId, "CONFIG\\PART.CFG", 0)!=0){
			printf("failed to create partition config\r\n");
			while (1){};
			return -1;
		}
		if (fs_open(mountId, "CONFIG\\PART.CFG", 0, &fileId)!=0){
			printf("failed to open newly created partition config\r\n");
			while (1){};
			return -1;
		}
		struct partition_conf partitionConf = {0};
		partitionConf.rootPartitionId = rootPartition;
		partitionConf.rootPartition = rootPartitionData;
		if (fs_write(mountId, fileId, (unsigned char*)&partitionConf, sizeof(struct partition_conf))!=0){
			printf("failed to write to partition config\r\n");
			while (1){};	
			return -1;
		}
	}
	struct fs_file_info confFileInfo = {0};
	if (fs_getFileInfo(mountId, fileId, &confFileInfo)!=0){
		printf("failed to get file information\r\n");
		fs_close(mountId, fileId);
		fs_unmount(mountId);
		while (1){};
		return -1;
	}
	unsigned char* pFileBuffer = (unsigned char*)kmalloc(confFileInfo.fileSize);
	if (!pFileBuffer){
		printf("failed to allocate memory for file buffer\r\n");
		fs_close(mountId, fileId);
		fs_unmount(mountId);
		while (1){};
		return -1;
	}
	struct partition_conf* pPartitionConf = (struct partition_conf*)pFileBuffer;
	if (fs_read(mountId, fileId, pFileBuffer, confFileInfo.fileSize)!=0){
		printf("failed to read config\r\n");
		fs_close(mountId, fileId);
		fs_unmount(mountId);
		while (1){};
		return -1;
	}
	struct gpt_partition rootPartitionData = pPartitionConf->rootPartition;
	uint64_t rootPartitionId = pPartitionConf->rootPartitionId;
	printf("root partition size: %dMB\r\n", ((rootPartitionData.end_lba-rootPartitionData.start_lba)*DRIVE_SECTOR_SIZE)/MEM_MB);
	kfree((void*)pFileBuffer);
	fs_close(mountId, fileId);
	uint64_t pid = 0;
	if (kext_load(mountId, "KEXTS/TEST.ELF", &pid)!=0){
		printf("failed to load kext\r\n");
		fs_unmount(mountId);
		while (1){};
		return -1;
	}
	printf("PID: %d\r\n", pid);
	fs_unmount(mountId);
	lprintf((uint16_t*)L"dev path: %s\r\n", pbootargs->driveInfo.devicePathStr);
	while (1){};
	return 0;	
}
