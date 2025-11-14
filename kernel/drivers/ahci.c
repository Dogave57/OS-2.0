#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/timer.h"
#include "align.h"
#include "drivers/ahci.h"
struct ahci_info ahciInfo = {0};
struct ahci_cmd_hdr* pCmdList = (struct ahci_cmd_hdr*)0x0;
int ahci_init(void){
	if (ahci_get_info(&ahciInfo)!=0)
		return -1;
	if (virtualMapPages((uint64_t)ahciInfo.pBase, (uint64_t)ahciInfo.pBase, PTE_RW|PTE_NX, 2, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf(L"failed to map AHCI controller registers\r\n");
		return -1;
	}
	printf(L"AHCI base: %p\r\n", (void*)ahciInfo.pBase);
	printf(L"AHCI controller bus: %d, device: %d, function: %d\r\n", ahciInfo.bus, ahciInfo.dev, ahciInfo.func);
	if (ahci_init_cmd_list(&pCmdList)!=0){
		printf(L"failled to initialize cmd list\r\n");
		return -1;
	}
	uint32_t ahci_version = 0;
	if (ahci_read_dword(0x10, &ahci_version)!=0){
		printf(L"failed to read AHCI controller version register\r\n");
		return -1;
	}
	printf(L"AHCI controller version: 0x%x\r\n", ahci_version);
	struct ahci_drive_info driveInfo = {0};
	if (ahci_get_drive_info(0, &driveInfo)!=0){
		printf(L"failed to get drive info\r\n");
		return -1;
	}
	for (uint32_t i = 0;i<AHCI_MAX_PORTS;i++){
		if (ahci_drive_exists(i)!=0)
			continue;
		printf(L"AHCI device at port: %d\r\n", i);
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
	uint64_t drive_base = AHCI_PORT_OFFSET(drive_port);
	uint32_t drive_signature = 0;
	if (ahci_read_dword(drive_base+AHCI_PORT_SIGNATURE_REG, &drive_signature)!=0){
		printf(L"failed to read drive status\r\n");
		return -1;
	}
	if (drive_signature!=AHCI_DRIVE_SIGNATURE)
		return -1;
	return 0;
}
int ahci_init_cmd_table(struct ahci_cmd_hdr** ppCmdTable){
	if (!ppCmdTable)
		return -1;
	uint64_t cmdTableSize = sizeof(struct ahci_cmd_hdr)*32;
	struct ahci_cmd_hdr* pCmdTable = (struct ahci_cmd_hdr*)kmalloc(cmdTableSize);
	if (!pCmdTable)
		return -1;
	memset((void*)pCmdTable, 0, cmdTableSize);
	*ppCmdTable = pCmdTable;
	return 0;
}
int ahci_deinit_cmd_table(struct ahci_cmd_hdr* pCmdTable){
	if (!pCmdTable)
		return -1;
	for (uint64_t i = 0;i<32;i++){
		struct ahci_cmd_hdr* pHdr = pCmdTable+i;
		uint64_t pTable = ((uint64_t)pCmdTable->cmd_table_base_upper<<32)|(pCmdTable->cmd_table_base_lower);
		if (!pTable)
			continue;
		if (kfree((void*)pTable)!=0){
			kfree((void*)pCmdTable);
			return -1;
		}
	}
	return kfree((void*)pCmdTable);
}
int ahci_set_cmd_table(uint8_t drive_port, struct ahci_cmd_hdr* pCmdTable){
	if (!pCmdTable)
		return -1;
	uint64_t cmdTable_lower = ((uint64_t)pCmdTable)&0xFFFFFFFFUL;
	uint64_t cmdTable_upper = (((uint64_t)pCmdTable)>>32)&0xFFFFFFFFUL;
	uint64_t port_offset = AHCI_PORT_OFFSET(drive_port);
	if (ahci_write_dword(AHCI_PORT_CMD_TABLE_LOW_REG, cmdTable_lower)!=0){
		printf(L"failed to write lower cmd table\r\n");
		return -1;
	}
	if (ahci_write_dword(AHCI_PORT_CMD_TABLE_HIGH_REG, cmdTable_upper)!=0){
		printf(L"failed to write upper cmd table\r\n");
		return -1;
	}
	return 0;
}
int ahci_write_prdt(struct ahci_prdt* pPrdt, uint64_t va, uint32_t buffer_size){
	if (!pPrdt)
		return -1;
	uint64_t pa = 0;
	if (virtualToPhysical(va, &pa)!=0)
		return -1;
	pPrdt->buffer_base_lower = pa&0xFFFFFFFFUL;
	pPrdt->buffer_base_upper = (pa>>32)&0xFFFFFFFFUL;
	pPrdt->buffer_size = buffer_size;
	return 0;
}
int ahci_start_cmd(uint8_t port){
	uint64_t port_offset = AHCI_PORT_OFFSET(port);
	uint32_t port_cmd = 0;
	while (1){
		ahci_read_dword(port_offset+AHCI_PORT_CR_REG, &port_cmd);
		if (port_cmd&(1<<15))
			continue;
		break;
	}
	ahci_read_dword(port_offset+AHCI_PORT_CR_REG, &port_cmd);
	port_cmd|=(0x10);
	port_cmd|=(0x01);
	return 0;
}
int ahci_stop_cmd(uint8_t port){
	uint64_t port_offset = AHCI_PORT_OFFSET(port);
	uint32_t port_cmd = 0;
	ahci_read_dword(port_offset+AHCI_PORT_CR_REG, &port_cmd);
	port_cmd&=~(0x10);
	port_cmd&=~(0x01);
	while (1){
		ahci_read_dword(port_offset+AHCI_PORT_CR_REG, &port_cmd);
		if (port_cmd&(1<<4)||port_cmd&(1<<15))
			continue;
		break;
	}
	return 0;
}
int ahci_init_cmd_list(struct ahci_cmd_hdr** ppCmdTable){
	if (!ppCmdTable)
		return -1;
	struct ahci_cmd_hdr* pCmdTable = (struct ahci_cmd_hdr*)0x0;
	uint64_t cmdListSize = sizeof(struct ahci_cmd_hdr)*AHCI_MAX_CMD_SLOTS;
	if (virtualAllocPage((uint64_t*)&pCmdTable, PTE_RW, 0, PAGE_TYPE_MMIO)!=0){
		printf(L"failed to allocate cmd list\r\n");
		return -1;
	}
	memset((void*)pCmdTable, 0, cmdListSize);
	*ppCmdTable = pCmdTable;
	return 0;
}
int ahci_deinit_cmd_list(struct ahci_cmd_hdr* pCmdTable){
	if (!pCmdTable)
		return -1;
	if (virtualFreePage((uint64_t)pCmdTable, 0)!=0){
		printf(L"failed to free cmd list\r\n");
		return -1;
	}
	return 0;
}
int ahci_clear_cmd_list(struct ahci_cmd_hdr* pCmdTable){
	if (!pCmdTable)
		return -1;
	for (uint64_t i = 0;i<AHCI_MAX_CMD_SLOTS;i++){
		if (!pCmdTable->cmd_table_base_lower&&!pCmdTable->cmd_table_base_upper)
			continue;
		if (ahci_free_cmd(pCmdTable, i)!=0)
			return -1;
	}
	return 0;
}
int ahci_alloc_cmd(struct ahci_cmd_table** ppCmd, struct ahci_cmd_hdr* pCmdTable, uint64_t index, uint64_t prdt_cnt){
	if (!ppCmd||!pCmdTable)
		return -1;
	struct ahci_cmd_hdr* pCmdHdr = pCmdTable+index;
	if (pCmdHdr->cmd_table_base_lower||pCmdHdr->cmd_table_base_upper)
		return -1;
	uint64_t cmdSize = align_up(sizeof(struct ahci_cmd_table)+(prdt_cnt*sizeof(struct ahci_prdt)), 8);
	if (cmdSize>PAGE_SIZE)
		return -1;
	struct ahci_cmd_table* pCmd = (struct ahci_cmd_table*)0x0;
	if (virtualAllocPage((uint64_t*)&pCmd, PTE_RW, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	memset((void*)pCmd, 0, cmdSize);
	uint64_t lower = ((uint64_t)pCmd)&0xFFFFFFFFUL;
	uint64_t upper = (((uint64_t)pCmd)>>32)&0xFFFFFFFFUL;
	pCmdTable->cmd_table_base_lower = lower;
	pCmdTable->cmd_table_base_upper = upper;
	pCmdTable->prdt_len = prdt_cnt;
	pCmdTable->prdt_byte_count = sizeof(struct ahci_prdt)*prdt_cnt;
	*ppCmd = pCmd;
	return 0;
}
int ahci_free_cmd(struct ahci_cmd_hdr* pCmdTable, uint64_t index){
	if (!pCmdTable)
		return -1;
	struct ahci_cmd_hdr* pCmdHdr = pCmdTable+index;
	uint64_t pCmd = (((uint64_t)pCmdHdr->cmd_table_base_upper)<<32)|((uint64_t)pCmdHdr->cmd_table_base_lower);
	if (virtualFreePage(pCmd, 0)!=0)
		return -1;
	pCmdHdr->cmd_table_base_lower = 0;
	pCmdHdr->cmd_table_base_upper = 0;
	return 0;
}
int ahci_get_drive_info(uint8_t drive_port, struct ahci_drive_info* pDriveInfo){
	if (!pDriveInfo)
		return -1;
	uint64_t port_offset = AHCI_PORT_OFFSET(drive_port);
	struct ahci_cmd_table* pCmd = (struct ahci_cmd_table*)0x0;
	if (ahci_alloc_cmd(&pCmd, pCmdList, 0, 1)!=0)
		return -1;
	pCmd->cmd_fis[0] = AHCI_CMD_IDENT_DEV;
	uint8_t dataBuffer[512] = {0};
	memset((void*)dataBuffer, 0, sizeof(dataBuffer));
	if (ahci_write_prdt(pCmd->prdt_list, (uint64_t)dataBuffer, 512)!=0){
		printf(L"failed to write prdt\r\n");
		ahci_clear_cmd_list(pCmdList);
		return -1;
	}
	ahci_start_cmd(drive_port);
	sleep(200);
	ahci_stop_cmd(drive_port);
	uint64_t sectorCnt_low = *(uint64_t*)(dataBuffer+120);
	uint64_t sectorCnt_high = *(uint64_t*)(dataBuffer+200);
	printf(L"sector cnt low: %d\r\n", sectorCnt_low);
	printf(L"sector cnt high: %d\r\n", sectorCnt_high);
	if (ahci_clear_cmd_list(pCmdList)!=0){
		printf(L"failed to clear cmd list\r\n");
		return -1;
	}
	return 0;
}
