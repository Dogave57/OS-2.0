#include <stdint.h>
#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/nvme.h"
int nvme_init(void){
	for (uint8_t bus = 0;bus<255;bus++){
	for (uint8_t dev = 0;dev<32;dev++){
		for (uint8_t func = 0;func<8;func++){
			struct pcie_location location = {0};
			location.bus = bus;
			location.dev = dev;
			location.func = func;
			if (nvme_drive_exists(location)!=0)
				continue;
			printf(L"NVME drive at bus %d, dev %d, func %d\r\n", bus, dev, func);
			struct nvme_driveinfo driveInfo = {0};
			if (nvme_get_drive_info(location, &driveInfo)!=0){
				printf(L"failed to get NVME drive info\r\n");
				continue;
			}
			printf(L"drive MMIO base: %p\r\n", (void*)driveInfo.mmio_base);
			continue;
		}
	}
	}
	return 0;
}
int nvme_drive_exists(struct pcie_location location){
	if (pcie_device_exists(location.bus, location.dev, location.func)!=0)
		return -1;
	uint8_t class = 0;
	uint8_t subclass = 0;
	if (pcie_get_class(location.bus, location.dev, location.func, &class)!=0)
		return -1;
	if (class!=PCIE_CLASS_DRIVE_CONTROLLER)
		return -1;
	if (pcie_get_subclass(location.bus, location.dev, location.func, &subclass)!=0)
		return -1;
	if (subclass!=PCIE_SUBCLASS_NVME_CONTROLLER)
		return -1;
	return 0;
}
int nvme_get_drive_info(struct pcie_location location, struct nvme_driveinfo* pInfo){
	if (!pInfo)
		return -1;
	if (nvme_drive_exists(location)!=0)
		return -1;
	uint64_t mmio_base = 0;
	if (pcie_get_bar(location.bus, location.dev, location.func, 0, &mmio_base)!=0){
		printf(L"failed to get BAR0\r\n");
		return -1;
	}
	pInfo->location = location;
	pInfo->mmio_base = mmio_base;
	return 0;
}
