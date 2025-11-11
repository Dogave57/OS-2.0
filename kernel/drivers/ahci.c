#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/timer.h"
#include "drivers/ahci.h"
struct ahci_info ahciInfo = {0};
int ahci_init(void){
	if (ahci_get_info(&ahciInfo)!=0)
		return -1;
	if (virtualMapPages((uint64_t)ahciInfo.pBase, (uint64_t)ahciInfo.pBase, PTE_RW|PTE_NX, 2, 1, 0, PAGE_TYPE_MMIO)!=0){
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
		uint64_t sector_data[64] = {0};
		memset((void*)sector_data, 0, 512);
		if (ahci_read_sectors(i, 0, 1, (uint8_t*)sector_data)!=0){
			printf(L"failed to read sector\r\n");
			return -1;
		}	
		printf(L"successfully read sector\r\n");
		for (unsigned int i = 0;i<64;i++){
			printf(L"%d, ", sector_data[i]);
		}
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
int ahci_read_sectors(uint8_t drive_port, uint64_t sector, uint64_t sector_cnt, uint8_t* pBuffer){
	if (!pBuffer||!sector_cnt)
		return -1;
	uint64_t port_offset = AHCI_DRIVE_MMIO_OFFSET+(drive_port*0x80);
	struct ahci_cmd_hdr* pCmdList = (struct ahci_cmd_hdr*)kmalloc(sizeof(struct ahci_cmd_hdr)*32);
	if (!pCmdList){
		printf(L"failed to allocate command list\r\n");
		return -1;
	}
	uint64_t cmdTableSize = sizeof(struct ahci_cmd_table)+(sizeof(struct ahci_prdt_entry)*sector_cnt);
	struct ahci_cmd_table* pCmdTable = (struct ahci_cmd_table*)kmalloc(cmdTableSize);
	if (!pCmdTable){
		printf(L"failed to allocate memory for cmd table\r\n");
		kfree((void*)pCmdList);
		return -1;
	}
	for (uint64_t i = 0;i<sector_cnt;i++){
		uint64_t pSector = ((uint64_t)pBuffer)+(i*512);
		struct ahci_prdt_entry* pEntry = pCmdTable->prdt+i;
		pEntry->buffer_base_lower = (uint32_t)(pSector&0xFFFFFFFFUL);
		pEntry->buffer_base_upper = (uint32_t)(pSector>>32);
	}
	struct ahci_cmd_hdr* pFirstCmd = pCmdList;
	pFirstCmd->cmd_table_base_lower = ((uint64_t)pCmdTable)&0xFFFFFFFFUL;
	pFirstCmd->cmd_table_base_upper = ((uint64_t)pCmdTable)>>32;
	uint32_t pCmdList_lower = (uint32_t)(((uint64_t)pCmdList)&0xFFFFFFFFUL);
	uint32_t pCmdList_higher = (uint32_t)(((uint64_t)pCmdList)>>32);
	ahci_write_dword(port_offset, pCmdList_lower);	
	ahci_write_dword(port_offset+0x4, pCmdList_higher);	
	uint32_t drive_status = 0;
	ahci_read_dword(port_offset+0x18, &drive_status);
	drive_status|=(1<<0);
	ahci_write_dword(port_offset+0x18, drive_status);	
	uint32_t cmd_issue = 0;
	while (1){
		ahci_read_dword(port_offset+0x38, &cmd_issue);
		if (cmd_issue!=0)
			continue;
		break;
	}
	uint32_t sata_error = 0;
	ahci_read_dword(port_offset+0x30, &sata_error);
	if (sata_error!=0){
		printf(L"SATA read fail\r\n");
		kfree((void*)pCmdTable);
		kfree((void*)pCmdList);
		return -1;
	}
	kfree((void*)pCmdTable);
	kfree((void*)pCmdList);
	return 0;
}
