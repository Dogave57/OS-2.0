#include "drivers/acpi.h"
#include "drivers/timer.h"
#include "mem/vmm.h"
#include "stdlib/stdlib.h"
#include "align.h"
#include "drivers/pcie.h"
struct pcie_info pcie_info = {0};
int pcie_init(void){
	if (pcie_get_info(&pcie_info)!=0){
		printf("failed to get PCIE controller info\r\n");
		return -1;
	}
	if (virtualMapPages(pcie_info.pBase, pcie_info.pBase, PTE_RW, 16, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map PCIE controller registers\r\n");
		return -1;
	}
	for (uint8_t bus = 0;bus<255;bus++){
		for (uint8_t dev = 0;dev<32;dev++){
			for (uint8_t func = 0;func<8;func++){
				uint16_t vendor_id = 0;
				uint16_t dev_id = 0;
				uint8_t class = 0;
				uint8_t subclass = 0;
				if (pcie_get_vendor_id(bus, dev, func, &vendor_id)!=0){
					printf("failed to get vendor id of device at bus %d, dev %d, func %d\r\n", bus, dev, func);
					return -1;		
				}
				if (vendor_id==0xffff)
					continue;
				if (pcie_get_device_id(bus, dev, func, &dev_id)!=0){
					printf("failed to get device id of device at bus %d, dev %d, func %d\r\n", bus, dev, func);
					return -1;
				}
				if (pcie_get_class(bus, dev, func, &class)!=0){
					printf("failed to get class of device at bus %d, dev %d, func %d\r\n", bus, dev, func);
					return -1;
				}
				if (pcie_get_subclass(bus, dev, func, &subclass)!=0){
					printf("failed to get subclass of device at bus %d, dev %d, func %d\r\n", bus, dev, func);
					return -1;
				}
				printf("vendor id: 0x%x | device id: 0x%x | class: 0x%x | subclass: 0x%x\r\n", vendor_id, dev_id, class, subclass);
			}
		}
	}
	return 0;
}
int pcie_get_info(struct pcie_info* pInfo){
	if (!pInfo)
		return -1;
	struct acpi_mcfgHdr* pHdr = (struct acpi_mcfgHdr*)0x0;
	if (acpi_find_table((unsigned int)'GFCM', (struct acpi_sdt_hdr**)&pHdr)!=0){
		printf("failed to get MCFG table\r\n");
		return -1;
	}
	struct acpi_mcfgEntry* pFirstEntry = (struct acpi_mcfgEntry*)(((unsigned char*)pHdr)+0x2C);
	uint64_t entryCnt = (pHdr->hdr.len-sizeof(struct acpi_sdt_hdr))/sizeof(struct acpi_mcfgEntry);
	if (!entryCnt){
		printf("no PCIE controller!\r\n");
		return -1;
	}
	pInfo->pBase = pFirstEntry->pBase;
	pInfo->startBus = pFirstEntry->startBus;
	pInfo->endBus = pFirstEntry->endBus;
	return 0;
}
int pcie_get_ecam_base(uint8_t bus, uint8_t dev, uint8_t func, uint64_t* pBase){
	if (!pBase)
		return -1;
	uint64_t ecam_offset = (bus<<20)+(dev<<15)+(func<<12);
	uint64_t base = pcie_info.pBase+ecam_offset;
	uint64_t base_page = align_down(base, PAGE_SIZE);
	if (virtualMapPages(base_page, base_page, PTE_RW, 8, 1, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	*pBase = base;
	return 0;
}
int pcie_read_byte(uint8_t bus, uint8_t dev, uint8_t func, uint64_t byte_offset, uint8_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint8_t* pReg = (volatile uint8_t*)(ecam_base+byte_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_byte(uint8_t bus, uint8_t dev, uint8_t func, uint64_t byte_offset, uint8_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint8_t* pReg = (volatile uint8_t*)(ecam_base+byte_offset);
	*pReg = value;
	return 0;
}
int pcie_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint64_t word_offset, uint16_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint16_t* pReg = (volatile uint16_t*)(ecam_base+word_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_word(uint8_t bus, uint8_t dev, uint8_t func, uint64_t word_offset, uint16_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint16_t* pReg = (volatile uint16_t*)(ecam_base+word_offset);
	*pReg = value;
	return 0;
}
int pcie_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t dword_offset, uint32_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint32_t* pReg = (volatile uint32_t*)(ecam_base+dword_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t dword_offset, uint32_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint32_t* pReg = (volatile uint32_t*)(ecam_base+dword_offset);
	*pReg = value;
	return 0;
}
int pcie_read_qword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t qword_offset, uint64_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint64_t* pReg = (volatile uint64_t*)(ecam_base+qword_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_qword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t qword_offset, uint64_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(bus, dev, func, &ecam_base)!=0)
		return -1;
	volatile uint64_t* pReg = (volatile uint64_t*)(ecam_base+qword_offset);
	*pReg = value;
	return 0;
}
int pcie_get_vendor_id(uint8_t bus, uint8_t dev, uint8_t func, uint16_t* pVendorId){
	if (!pVendorId)
		return -1;
	return pcie_read_word(bus, dev, func, 0x0, pVendorId);
}
int pcie_get_device_id(uint8_t bus, uint8_t dev, uint8_t func, uint16_t* pDeviceId){
	if (!pDeviceId)
		return -1;
	return pcie_read_word(bus, dev, func, 0x2, pDeviceId);
}
int pcie_get_bar(uint8_t bus, uint8_t dev, uint8_t func, uint64_t barType, uint64_t* pBar){
	if (!pBar)
		return -1;
	uint64_t bar_reg = 0x10+(0x4*barType);
	return pcie_read_qword(bus, dev, func, bar_reg,  pBar);
}
int pcie_get_class(uint8_t bus, uint8_t dev, uint8_t func, uint8_t* pClass){
	if (!pClass)
		return -1;
	return pcie_read_byte(bus, dev, func, 0x0B, pClass);
}
int pcie_get_subclass(uint8_t bus, uint8_t dev, uint8_t func, uint8_t* pSubclass){
	if (!pSubclass)
		return -1;
	return pcie_read_byte(bus, dev, func, 0x0A, pSubclass);
}
int pcie_device_exists(uint8_t bus, uint8_t dev, uint8_t func){
	uint16_t vendor_id = 0;
	if (pcie_get_vendor_id(bus, dev, func, &vendor_id)!=0)
		return -1;
	if (vendor_id!=0xffff)
		return 0;
	return -1;
}
int pcie_get_device_by_class(uint8_t class, uint8_t subclass, uint8_t* pBus, uint8_t* pDev, uint8_t* pFunc){
	if (!pBus||!pDev||!pFunc)
		return -1;
	for (uint8_t bus = pcie_info.startBus;bus<pcie_info.endBus;bus++){
		for (uint8_t dev = 0;dev<32;dev++){
			for (uint8_t func = 0;func<8;func++){
				uint8_t dev_class = 0;
				uint8_t dev_subclass = 0;
				if (pcie_device_exists(bus, dev, func)!=0)
					continue;
				if (pcie_get_class(bus, dev, func, &dev_class)!=0)
					continue;
				if (pcie_get_subclass(bus, dev, func, &dev_subclass)!=0)
					continue;
				if (dev_class!=class||dev_subclass!=subclass)
					continue;
				*pBus = bus;
				*pDev = dev;
				*pFunc = func;
				return 0;
			}
		}
	}
	return -1;
}
