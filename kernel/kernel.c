#include "bootloader.h"
#include "logo.h"
#include "stdlib/stdlib.h"
#include "cpu/interrupt.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
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
#include "subsystem/text.h"
#include "subsystem/gpu.h"
#include "drivers/gpt.h"
#include "drivers/gpu/virtio/virtio.h"
#include "drivers/filesystem/fat32.h"
#include "drivers/filesystem/exfat.h"
#include "drivers/filesystem/fluxfs.h"
#include "drivers/usb/xhci.h"
#include "drivers/dma-device.h"
#include "drivers/fonts/ttf.h"
#include "drivers/shaders/tgsi.h"
#include "drivers/shaders/ggsl.h"
#include "math/basic.h"
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
	BS->ExitBootServices(pbootargs->bootloaderHandle, pbootargs->memoryInfo.memoryMapKey);
	if (serial_init()!=0){
		printf("failed to intialize serial ports\r\n");
		while (1){};
		return -1;
	}
	if (text_subsystem_soft_init()!=0){
		serial_print(0, "failed to soft initialize graphical text subsystem\r\n");
		while (1){};
		return -1;
	}
	clear();
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
	if (gdt_init()!=0){
		printf("failed to initialize GDT\r\n");
		while (1){};
		return -1;
	}
	if (idt_init()!=0){
		printf("failed to initialize IDT\r\n");
		while (1){};
		return -1;
	}
	EFI_DEVICE_PATH_PROTOCOL* pCurrentPathNode = pbootargs->pDevicePathProtocol;
	if (virtualMapPages((uint64_t)pCurrentPathNode, (uint64_t)pCurrentPathNode, PTE_RW|PTE_NX, align_up(pbootargs->devicePathProtocolSize, PAGE_SIZE)/PAGE_SIZE, 0x01, 0x00)!=0){
		printf("failed to map device path protocol chain\r\n");
		while (1){};
		return -1;
	}	
	uint8_t currentBus = 0;
	struct pcie_location currentLocation = {0};
	memset((void*)&currentLocation, 0, sizeof(struct pcie_location));	
	while (pCurrentPathNode->Type!=0x7F){
		uint64_t nodeLength = (uint64_t)(*(uint16_t*)pCurrentPathNode->Length);
		if (pCurrentPathNode->Type==0x03&&pCurrentPathNode->SubType==0x12){
			struct efi_sata_dev_path* pSataDevicePath = (struct efi_sata_dev_path*)pCurrentPathNode;
			pbootargs->driveInfo.driveType = DRIVE_TYPE_SATA;
			pbootargs->driveInfo.port = pSataDevicePath->port;
		}
		if (pCurrentPathNode->Type==0x03&&pCurrentPathNode->SubType==0x05){
			struct efi_usb_dev_path* pUsbDevicePath = (struct efi_usb_dev_path*)pCurrentPathNode;
			pbootargs->driveInfo.driveType = DRIVE_TYPE_USB;
			pbootargs->driveInfo.port = pUsbDevicePath->portNumber;
			pbootargs->driveInfo.extra = pUsbDevicePath->interfaceNumber;
		}
		if (pCurrentPathNode->Type==0x03&&pCurrentPathNode->SubType==0x17){
			struct efi_nvme_dev_path* pNvmeDevicePath = (struct efi_nvme_dev_path*)pCurrentPathNode;
			struct pcie_location location = currentLocation;
			currentLocation.bus = currentBus;
			pbootargs->driveInfo.driveType = DRIVE_TYPE_NVME;
			pbootargs->driveInfo.port = pNvmeDevicePath->namespaceId;	
			*(uint32_t*)&pbootargs->driveInfo.extra = *(uint32_t*)&location;	
		}
		if (pCurrentPathNode->Type==0x01&&pCurrentPathNode->SubType==0x01){
			struct efi_pcie_path* pPciePath = (struct efi_pcie_path*)pCurrentPathNode;
			currentLocation.dev = pPciePath->device;
			currentLocation.func = pPciePath->function;	
			struct pcie_location location = {0};
			memset((void*)&location, 0, sizeof(struct pcie_location));
			location.bus = currentBus;
			location.dev = pPciePath->device;
			location.func = pPciePath->function;	
			uint8_t headerType = 0;
			pcie_read_byte(location, 0x0E, &headerType);
			uint8_t functionCount = (headerType&(1<<7)) ? 8 : 0;
			headerType&=0x3F;
			if (headerType==0x01){
				pcie_read_byte(location, 0x19, &currentBus);
			}
			for (uint64_t i = 1;i<functionCount;i++){
				location.func = i;	
				pcie_read_byte(location, 0x8E, &headerType);
				headerType&=0x3F;
				if (headerType==0x1)
					pcie_read_byte(location, 0x19, &currentBus);	
			}	
		}
		if (pCurrentPathNode->Type==0x04&&pCurrentPathNode->SubType==0x01){
			struct efi_gpt_partition_path* pGptPath = (struct efi_gpt_partition_path*)pCurrentPathNode;
			pbootargs->driveInfo.espNumber = pGptPath->partitionNumber-1;	
		}
		pCurrentPathNode = (EFI_DEVICE_PATH_PROTOCOL*)(((uint64_t)pCurrentPathNode)+nodeLength);	
	}
	if (pbootargs->driveInfo.driveType==DRIVE_TYPE_INVALID){
		printf("unsupported boot device\r\n");
		while (1){};
		return -1;
	}
	if (pbootargs->driveInfo.espNumber==0xFFFFFFFFFFFFFFFF){
		printf("failed to get boot device EFI system partition\r\n");
		while (1){};
		return -1;
	}	
	if (subsystem_mem_init()!=0){
		printf("failed to initialize subsystem allocator volatile memory\r\n");
		while (1){};
		return -1;
	}
	if (heap_mem_init()!=0){
		printf("failed to initialize heap volatile memory\r\n");
		while (1){};
		return -1;
	}
	uint64_t heapId = 0x00;
	if (heap_init(MEM_MB*0x04, 0x10, PAGE_SIZE, &heapId)!=0){
		printf("failed to initialize heap volatile memory descriptor\r\n");
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
		printf("failed to initialize NVMe protocol specification driver\r\n");
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
	uint64_t espMountId = 0x00;
	if (fs_mount(0x01, pbootargs->driveInfo.espNumber, &espMountId)!=0){
		printf("failed to mount non-volatile memory device ESP partition\r\n");
		while (1){};
		return -1;
	}
	if (kext_subsystem_init()!=0){
		printf("failed to initialize kext subsystem\r\n");
		while (1){};
		return -1;
	}
	if (gpu_subsystem_init()!=0){
		printf("failed to initialize GPU host controller subsystem\r\n");
		while (1){};
		return -1;
	}
	if (dma_device_driver_init()!=0){
		printf("failed to initialize custom bulk DMA device host controller driver\r\n");
	}
	if (tgsi_driver_init()!=0)
		printf("failed to initialize GPU host controller TGSI shader driver\r\n");
	if (ggsl_driver_init()!=0)
		printf("failed to initialize GPU host controller GGSL shader driver\r\n");
	if (virtio_gpu_init()!=0){
		printf("virtual I/O GPU host controller not available\r\n");
	}
	if (text_subsystem_init()!=0){
		printf("failed to fully initialize graphical text subsystem\r\n");
		while (1){};
		return -1;
	}
	if (font_driver_ttf_init()!=0){
		printf("failed to initialize GPU host controller .TTF font driver\r\n");
	}
	if (xhci_driver_init()!=0){
		printf("extensible host controller not available\r\n");
	}
	if (gpt_verify(0x01)!=0){
		printf("failed to verify GPT partition table\r\n");
		while (1){};
		return -1;
	}
	uint64_t startTime = get_time_us();
	unsigned char* pPageData = (unsigned char*)0x00;
	uint64_t pageDataSize = MEM_MB*0x20;
	if (virtualAlloc((uint64_t*)&pPageData, pageDataSize, PTE_RW|PTE_NX, 0x00)!=0){
		printf("failed to allocate physical volatile memory physical pages\r\n");
		while (1){};
		return -1;
	}
	uint64_t elapsedTime = get_time_us()-startTime;
	printf("%fGB/s physical volatile memory allocation\r\n", (floorf(((1000000.0/((double)elapsedTime))/(double)(MEM_GB/pageDataSize))*10.0))/10.0);
	virtualFree((uint64_t)pPageData, pageDataSize);
/*	uint64_t usedPages = 0x00;
	uint64_t freePages = 0x00;
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
	printf("--volatile physical page info--\r\n");
	struct uvec4_8 green = {0, 255, 0, 0};
	struct uvec4_8 red = {255, 0, 0, 0};
	struct uvec4_8 old_fg = {0,0,0,0};
	struct uvec4_8 old_bg = {0,0,0,0};
	get_text_color(&old_fg, &old_bg);
	set_text_color(red, old_bg);
	printf("used pages: %d\r\n", usedPages);
	set_text_color(green, old_bg);
	printf("free pages: %d\r\n", freePages);
	set_text_color(old_fg, old_bg);
	printf("--end of volatile physical page info--\r\n");
*/	struct gpt_header gptHeader = {0};
	if (gpt_get_header(1, &gptHeader)!=0){
		printf("failed to get GPT header\r\n");
		while (1){};
		return -1;
	}
	struct gpt_partition esp_partition = {0};
	uint64_t esp_partition_id = 0xFFFFFFFFFFFFFFFF;
	unsigned char esp_partition_guid[16] = GPT_ESP_GUID;
	uint64_t espNumber = pbootargs->driveInfo.espNumber;
	struct gpt_partition partition = {0};
	if (gpt_get_partition(1, espNumber, &partition)!=0){
		printf("failed to get ESP information\r\n");
		while (1){};
		return -1;
	}
	uint64_t size = (partition.end_lba-partition.start_lba)*DRIVE_SECTOR_SIZE;
	uint64_t dirHandle = 0x00;
	if (fs_opendir(espMountId, "FONTS", &dirHandle)!=0){
		printf("failed to open root\r\n");
		while (1){};
		return -1;
	}
	struct fs_file_info fileInfo = {0};
	while (fs_read_dir(espMountId, dirHandle, &fileInfo)==0){
		if (fileInfo.fileAttribs&FILE_ATTRIBUTE_HIDDEN)
			continue;
		if (fileInfo.fileAttribs&FILE_ATTRIBUTE_DIRECTORY){
			printf("directory: %s\r\n", fileInfo.fileName);
			continue;
		}
		printf("file: %s\r\n", fileInfo.fileName);
	}
	fs_closedir(espMountId, dirHandle);
	uint64_t rootPartition = 0xFFFFFFFFFFFFFFFF;
	uint64_t rootPartitionSector = 0;
	unsigned char rootPartitionType[] = GPT_BASIC_DATA_GUID;
	struct drive_info bootDriveInfo = {0};
	uint64_t bootDriveSize = 0;
	if (drive_get_info(1, &bootDriveInfo)!=0){
		printf("failed to get boot drive info\r\n");
		while (1){};
		return -1;
	}
	bootDriveSize = bootDriveInfo.sectorCount*DRIVE_SECTOR_SIZE;
	struct gpt_partition highestPartition = {0};
	for (uint64_t i = 0;i<gptHeader.partition_count;i++){
		struct gpt_partition partition = {0};
		if (gpt_get_partition(1, i, &partition)!=0){
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
	uint64_t pid = 0;
	if (kext_load(0x01, "KEXTS/TEST.ELF", &pid)!=0){
		printf("failed to load kext\r\n");
		while (1){};
		return -1;
	}
	while (1){};
	return 0;	
}
