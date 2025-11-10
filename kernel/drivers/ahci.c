#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/ahci.h"
struct ahci_info ahciInfo = {0};
int ahci_init(void){
	if (ahci_get_info(&ahciInfo)!=0)
		return -1;
	if (virtualMapPage(ahciInfo.pBase, ahciInfo.pBase, PTE_RW, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf(L"failed to map AHCI base\r\n");
		return -1;
	}
	printf(L"AHCI base: %p\r\n", (void*)ahciInfo.pBase);
	return 0;
}
int ahci_get_info(struct ahci_info* pInfo){
	if (!pInfo)
		return -1;
	if (ahciInfo.pBase){
		*pInfo = ahciInfo;
		return 0;
	}
	uint8_t bus = 0;
	uint8_t dev = 0;
	uint8_t func = 0;
	uint64_t pBase = 0;
	if (pcie_get_device_by_class(0x01, 0x06, &bus, &dev, &func)!=0){
		printf(L"failed to get AHCI controller\r\n");
		return -1;
	}
	if (pcie_get_bar(bus, dev, func, 5, &pBase)!=0){
		printf(L"failed to get AHCI base\r\n");
		return -1;
	}
	if (!pBase){
		printf(L"invalid AHCI base\r\n");
		return -1;
	}
	ahciInfo.bus = bus;
	ahciInfo.dev = dev;
	ahciInfo.func = func;
	ahciInfo.pBase = pBase;
	return 0;
}
