#include "drivers/acpi.h"
#include "drivers/timer.h"
#include "drivers/apic.h"
#include "mem/vmm.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "stdlib/stdlib.h"
#include "align.h"
#include "drivers/pcie.h"
struct pcie_info pcie_info = {0};
int pcie_init(void){
	if (pcie_get_info(&pcie_info)!=0){
		printf("failed to get PCIE controller info\r\n");
		return -1;
	}
	uint64_t virtualSpace = pcie_info.pBase_phys;
	uint64_t mmioBasePages = 16;
	if (virtualMapPages(pcie_info.pBase_phys, virtualSpace, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, mmioBasePages, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map PCIE controller registers\r\n");
		return -1;
	}
	pcie_info.pBase = virtualSpace;
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
	pInfo->pBase_phys = (uint64_t)pFirstEntry->pBase;
	pInfo->startBus = pFirstEntry->startBus;
	pInfo->endBus = pFirstEntry->endBus;
	return 0;
}
int pcie_get_ecam_base(struct pcie_location location, uint64_t* pBase){
	if (!pBase)
		return -1;
	uint8_t bus = location.bus;
	uint8_t dev = location.dev;
	uint8_t func = location.func;
	uint64_t ecam_offset = (bus<<20)+(dev<<15)+(func<<12);
	uint64_t base_phys = pcie_info.pBase_phys+ecam_offset;
	uint64_t base_virtual = pcie_info.pBase+ecam_offset;
	if (virtualMapPages(base_phys, base_virtual, PTE_RW|PTE_PCD|PTE_PWT|PTE_NX, 32, 1, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	*pBase = base_virtual;
	return 0;
}
int pcie_read_byte(struct pcie_location location, uint64_t byte_offset, uint8_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint8_t* pReg = (volatile uint8_t*)(ecam_base+byte_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_byte(struct pcie_location location, uint64_t byte_offset, uint8_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint8_t* pReg = (volatile uint8_t*)(ecam_base+byte_offset);
	*pReg = value;
	return 0;
}
int pcie_read_word(struct pcie_location location, uint64_t word_offset, uint16_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint16_t* pReg = (volatile uint16_t*)(ecam_base+word_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_word(struct pcie_location location, uint64_t word_offset, uint16_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint16_t* pReg = (volatile uint16_t*)(ecam_base+word_offset);
	*pReg = value;
	return 0;
}
int pcie_read_dword(struct pcie_location location, uint64_t dword_offset, uint32_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint32_t* pReg = (volatile uint32_t*)(ecam_base+dword_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_dword(struct pcie_location location, uint64_t dword_offset, uint32_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint32_t* pReg = (volatile uint32_t*)(ecam_base+dword_offset);
	*pReg = value;
	return 0;
}
int pcie_read_qword(struct pcie_location location, uint64_t qword_offset, uint64_t* pValue){
	if (!pValue)
		return -1;
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint64_t* pReg = (volatile uint64_t*)(ecam_base+qword_offset);
	*pValue = *pReg;
	return 0;
}
int pcie_write_qword(struct pcie_location location, uint64_t qword_offset, uint64_t value){
	uint64_t ecam_base = 0;
	if (pcie_get_ecam_base(location, &ecam_base)!=0)
		return -1;
	volatile uint64_t* pReg = (volatile uint64_t*)(ecam_base+qword_offset);
	*pReg = value;
	return 0;
}
int pcie_get_cap_ptr(struct pcie_location location, uint8_t cap_id, struct pcie_cap_link** ppCapLink){
	if (!ppCapLink)
		return -1;
	uint8_t bus = location.bus;
	uint8_t dev = location.dev;
	uint8_t func = location.func;
	struct pcie_cap_link currentLink = {0};
	uint64_t currentOffset = 0x34;
	uint8_t newOffset = 0;
	pcie_read_byte(location, currentOffset, &newOffset);
	currentOffset = (uint64_t)newOffset;
	while (currentOffset&&currentOffset<0xFF){
		pcie_read_word(location, currentOffset, (uint16_t*)&currentLink);
		if (currentLink.cap_id!=cap_id){
			currentOffset = (uint64_t)currentLink.next_offset;
			continue;
		}
		uint64_t ecam_base = 0;
		if (pcie_get_ecam_base(location, &ecam_base)!=0){
			return -1;
		}
		struct pcie_cap_link* pCapLink = (struct pcie_cap_link*)(ecam_base+currentOffset);
		*ppCapLink = pCapLink;
		return 0;
	}
	return -1;
}
int pcie_get_vendor_id(struct pcie_location location, uint16_t* pVendorId){
	if (!pVendorId)
		return -1;
	return pcie_read_word(location, 0x0, pVendorId);
}
int pcie_get_device_id(struct pcie_location location, uint16_t* pDeviceId){
	if (!pDeviceId)
		return -1;
	return pcie_read_word(location, 0x2, pDeviceId);
}
int pcie_get_progif(struct pcie_location location, uint8_t* pProgIf){
	if (!pProgIf)
		return -1;
	return pcie_read_byte(location, 0x9, pProgIf);
}
int pcie_get_bar(struct pcie_location location, uint64_t barType, uint64_t* pBar){
	if (!pBar)
		return -1;
	uint64_t bar_reg = 0x10+(0x4*barType);
	return pcie_read_qword(location, bar_reg,  pBar);
}
int pcie_get_class(struct pcie_location location, uint8_t* pClass){
	if (!pClass)
		return -1;
	return pcie_read_byte(location, 0x0B, pClass);
}
int pcie_get_subclass(struct pcie_location location, uint8_t* pSubclass){
	if (!pSubclass)
		return -1;
	return pcie_read_byte(location, 0x0A, pSubclass);
}
int pcie_device_exists(struct pcie_location location){
	uint16_t vendor_id = 0;
	if (pcie_get_vendor_id(location, &vendor_id)!=0)
		return -1;
	if (vendor_id!=0xffff)
		return 0;
	return -1;
}
int pcie_get_device_by_class(uint8_t class, uint8_t subclass, struct pcie_location* pLocation){
	if (!pLocation)
		return -1;
	for (uint8_t bus = pcie_info.startBus;bus<pcie_info.endBus;bus++){
		for (uint8_t dev = 0;dev<32;dev++){
			for (uint8_t func = 0;func<8;func++){
				uint8_t dev_class = 0;
				uint8_t dev_subclass = 0;
				struct pcie_location location = {0};
				location.bus = bus;
				location.dev = dev;
				location.func = func;
				if (pcie_device_exists(location)!=0)
					continue;
				if (pcie_get_class(location, &dev_class)!=0)
					continue;
				if (pcie_get_subclass(location, &dev_subclass)!=0)
					continue;
				if (dev_class!=class||dev_subclass!=subclass)
					continue;
				*pLocation = location;
				return 0;
			}
		}
	}
	return -1;
}
int pcie_get_device_by_id(uint16_t vendor_id, uint16_t device_id, struct pcie_location* pLocation){
	if (!pLocation)
		return -1;
	for (uint8_t bus = pcie_info.startBus;bus<pcie_info.endBus;bus++){
		for (uint8_t dev = 0;dev<32;dev++){
			for (uint8_t func = 0;func<8;func++){
				uint16_t dev_vendor_id = 0;
				uint16_t dev_device_id = 0;
				struct pcie_location location = {0};
				location.bus = bus;
				location.dev = dev;
				location.func = func;
				if (pcie_device_exists(location)!=0)
					continue;
				if (pcie_get_vendor_id(location, &dev_vendor_id)!=0)
					continue;
				if (dev_vendor_id!=vendor_id)
					continue;
				if (pcie_get_device_id(location, &dev_device_id)!=0)
					continue;
				if (dev_device_id!=device_id)
					continue;
				*pLocation = location;
				return 0;
			}
		}
	}
	return -1;
}
int pcie_msix_get_msg_ctrl(struct pcie_location location, volatile struct pcie_msix_msg_ctrl** ppMsgControl){
	if (!ppMsgControl)
		return -1;
	if (pcie_get_cap_ptr(location, 0x11, (struct pcie_cap_link**)ppMsgControl)!=0)
		return -1;
	return 0;
}
int pcie_msi_get_msg_ctrl(struct pcie_location location, volatile struct pcie_msi_msg_ctrl** ppMsgControl){
	if (!ppMsgControl)
		return -1;
	if (pcie_get_cap_ptr(location, 0x5, (struct pcie_cap_link**)ppMsgControl)!=0)
		return -1;
	return 0;
}
int pcie_msi_enable(volatile struct pcie_msi_msg_ctrl* pMsgControl){
	if (!pMsgControl)
		return -1;
	struct pcie_msi_msg_ctrl msgControl = *pMsgControl;
	msgControl.msi_enable = 1;
	*pMsgControl = msgControl;
	return 0;
}
int pcie_msi_disable(volatile struct pcie_msi_msg_ctrl* pMsgControl){
	if (!pMsgControl)
		return -1;
	struct pcie_msi_msg_ctrl msgControl = *pMsgControl;
	msgControl.msi_enable = 0;
	*pMsgControl = msgControl;
	return 0;
}
int pcie_msix_get_table_base(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, volatile struct pcie_msix_table_entry** ppTableBase){
	if (!pMsgControl||!ppTableBase)
		return -1;
	struct pcie_msix_msg_ctrl msgControl = *pMsgControl;
	uint32_t tableOffset = msgControl.table_address&~0x7;
	uint32_t tableBar = msgControl.table_address&0x7;
	uint8_t bus = location.bus;
	uint8_t dev = location.dev;
	uint8_t func = location.func;
	volatile struct pcie_msix_table_entry* pTableBase = (volatile struct pcie_msix_table_entry*)0x0;
	uint64_t bar_base = 0;
	if (pcie_get_bar(location, tableBar, &bar_base)!=0)
		return -1;
	pTableBase = (volatile struct pcie_msix_table_entry*)(bar_base+tableOffset);
	printf("BAR%d table offset: %d\r\n", msgControl.table_bar, tableOffset);
	*ppTableBase = pTableBase;
	return 0;
}
int pcie_msix_get_pba_base(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, volatile uint8_t** ppPba){
	if (!pMsgControl||!ppPba)
		return -1;
	struct pcie_msix_msg_ctrl msgControl = *pMsgControl;
	uint32_t tableOffset = msgControl.pba_address&~0x7;
	uint32_t tableBar = msgControl.pba_address&0x7;
	uint8_t* pPba = (uint8_t*)0x0;
	uint64_t bar_base = 0;
	if (pcie_get_bar(location, tableBar, &bar_base)!=0)
		return -1;
	printf("PBA bar: %d\r\n", tableBar);
	printf("PBA offset: %d\r\n", tableOffset);
	pPba = (uint8_t*)(bar_base+tableOffset);
	*ppPba = pPba;
	return 0;
}
int pcie_get_msix_entry(struct pcie_location location, volatile struct pcie_msix_table_entry** ppEntry, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector){
	if (!ppEntry||!pMsgControl)
		return -1;
	uint8_t bus = location.bus;
	uint8_t dev = location.dev;
	uint8_t func = location.func;
	volatile struct pcie_msix_table_entry* pTableBase = (volatile struct pcie_msix_table_entry*)0x0;
	if (pcie_msix_get_table_base(location, pMsgControl, &pTableBase)!=0)
		return -1;
	volatile struct pcie_msix_table_entry* pEntry = pTableBase+msix_vector;
	*ppEntry = pEntry;
	return 0;
}
int pcie_set_msix_entry(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector, uint64_t lapic_id, uint8_t interrupt_vector){
	if (!pMsgControl)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	volatile struct pcie_msix_table_entry* pMsixTable = (volatile struct pcie_msix_table_entry*)0x0;
	if (pcie_msix_get_table_base(location, pMsgControl, &pMsixTable)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	volatile struct pcie_msix_table_entry* pEntry = (volatile struct pcie_msix_table_entry*)0x0;
	pEntry = pMsixTable+msix_vector;
	uint32_t oldStatus = pMsgControl->msix_enable;
	pMsgControl->msix_enable = 0;
	if (virtualMapPage((uint64_t)pEntry, (uint64_t)pEntry, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		pMsgControl->msix_enable = oldStatus;
		mutex_unlock(&mutex);
		return -1;
	}
	uint64_t lapic_base = 0;
	if (lapic_get_base(&lapic_base)!=0){
		pMsgControl->msix_enable = oldStatus;
		mutex_unlock(&mutex);
		return -1;
	}
	printf("LAPIC base: %p\r\n", lapic_base);
	printf("old LAPIC base: %p\r\n", pEntry->msg_addr);
	uint64_t msgAddress = lapic_base|(lapic_id<<12);
	*(uint32_t*)&pEntry->msg_addr = (uint32_t)msgAddress;
	union pcie_msix_table_entry_msg_data msgData = pEntry->msg_data;
	msgData.raw = (uint32_t)(interrupt_vector);
	pEntry->msg_data = msgData;
	pEntry->vector_ctrl.raw = 0;
	pMsgControl->msix_enable = oldStatus;
	mutex_unlock(&mutex);
	return 0;
}
int pcie_msix_enable(volatile struct pcie_msix_msg_ctrl* pMsgControl){
	if (!pMsgControl)
		return -1;
	pMsgControl->vector_mask = 0;
	pMsgControl->msix_enable = 1;
	return 0;
}
int pcie_msix_disable(volatile struct pcie_msix_msg_ctrl* pMsgControl){
	if (!pMsgControl)
		return -1;
	pMsgControl->vector_mask = 1;
	pMsgControl->msix_enable = 0;
	return 0;
}
int pcie_msix_enable_entry(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector){
	if (!pMsgControl)
		return -1;
	volatile struct pcie_msix_table_entry* pMsixEntry = (volatile struct pcie_msix_table_entry*)0x0;
	if (pcie_get_msix_entry(location, &pMsixEntry, pMsgControl, msix_vector)!=0)
		return -1;
	union pcie_msix_table_entry_vector_ctrl vectorCtrl = pMsixEntry->vector_ctrl;
	vectorCtrl.mask = 0;
	pMsixEntry->vector_ctrl = vectorCtrl;
	return 0;
}
int pcie_msix_disable_entry(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector){
	if (!pMsgControl)
		return -1;
	volatile struct pcie_msix_table_entry* pMsixEntry = (volatile struct pcie_msix_table_entry*)0x0;
	if (pcie_get_msix_entry(location, &pMsixEntry, pMsgControl, msix_vector)!=0)
		return -1;
	union pcie_msix_table_entry_vector_ctrl vectorCtrl = pMsixEntry->vector_ctrl;
	vectorCtrl.mask = 1;
	pMsixEntry->vector_ctrl = vectorCtrl;
	return 0;
}
int pcie_msix_clear_pba(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector){
	if (!pMsgControl)
		return -1;
	volatile uint32_t* pPba = (volatile uint32_t*)0x0;
	if (pcie_msix_get_pba_base(location, pMsgControl, (volatile uint8_t**)&pPba)!=0)
		return -1;
	uint64_t pba_dword = msix_vector/32;
	uint64_t pba_bit = msix_vector%32;
	uint32_t dword = pPba[pba_dword];
	dword&=~(1<<pba_bit);
	pPba[pba_dword] = dword;
	return 0;
}
int pcie_msix_get_pba(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector){
	if (!pMsgControl)
		return -1;
	volatile uint32_t* pPba = (volatile uint32_t*)0x0;
	if (pcie_msix_get_pba_base(location, pMsgControl, (volatile uint8_t**)&pPba)!=0)
		return -1;
	uint64_t pba_dword = msix_vector/32;
	uint64_t pba_bit = msix_vector%32;
	uint32_t dword = pPba[pba_dword];
	return (dword&(1<<pba_bit)) ? 0 : -1;
}
