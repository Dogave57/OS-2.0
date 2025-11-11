#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/ahci.h"
struct ahci_info ahciInfo = {0};
int ahci_init(void){
	if (ahci_get_info(&ahciInfo)!=0)
		return -1;
	if (virtualMapPages((uint64_t)ahciInfo.pBase, (uint64_t)ahciInfo.pBase, PTE_RW|PTE_NX, 9, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf(L"failed to map AHCI controller registers\r\n");
		return -1;
	}
	printf(L"AHCI base: %p\r\n", (void*)ahciInfo.pBase);
	printf(L"AHCI controller bus: %d, device: %d, function: %d\r\n", ahciInfo.bus, ahciInfo.dev, ahciInfo.func);
	uint32_t ahci_version = 0;
	if (ahci_read_dword(0x10, &ahci_version)!=0){
		printf(L"failed to read AHCI controller version register\r\n");
		return -1;
	}
	printf(L"AHCI controller version: 0x%x\r\n", ahci_version);
	for (uint32_t i = 0;i<AHCI_MAX_PORTS;i++){
		if (ahci_drive_exists(i)!=0)
			continue;
		printf(L"drive at port %d\r\n", i);	
	}
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
int ahci_write_byte(uint64_t offset, uint8_t value){
	if (!ahciInfo.pBase)
		return -1;
	uint8_t* pReg = (uint8_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_byte(uint64_t offset, uint8_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	uint8_t* pReg = (uint8_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_write_word(uint64_t offset, uint16_t value){
	if (!ahciInfo.pBase)
		return -1;
	uint16_t* pReg = (uint16_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_word(uint64_t offset, uint16_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	uint16_t* pReg = (uint16_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_write_dword(uint64_t offset, uint32_t value){
	if (!ahciInfo.pBase)
		return -1;
	uint32_t* pReg = (uint32_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_dword(uint64_t offset, uint32_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	uint32_t* pReg = (uint32_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_write_qword(uint64_t offset, uint64_t value){
	if (!ahciInfo.pBase)
		return -1;
	uint64_t* pReg = (uint64_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_qword(uint64_t offset, uint64_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	uint64_t* pReg = (uint64_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_drive_exists(uint8_t drive_port){
	if (drive_port>32)
		return -1;
	uint64_t drive_base = AHCI_DRIVE_MMIO_OFFSET+(drive_port*0x80);
	uint32_t drive_signature = 0;
	if (ahci_read_dword(drive_base+0x24, &drive_signature)!=0){
		printf(L"failed to read drive status\r\n");
		return -1;
	}
	if (drive_signature!=AHCI_DRIVE_SIGNATURE)
		return -1;
	return 0;
}
