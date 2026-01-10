#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "drivers/pcie.h"
#include "drivers/timer.h"
#include "subsystem/drive.h"
#include "align.h"
#include "drivers/ahci.h"
struct ahci_info ahciInfo = {0};
struct ahci_cmd_list_desc cmdListDesc = {0};
int ahci_init(void){
	if (ahci_get_info(&ahciInfo)!=0)
		return -1;
	if (virtualMapPages((uint64_t)ahciInfo.pBase, (uint64_t)ahciInfo.pBase, PTE_RW|PTE_NX, 2, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map AHCI controller registers\r\n");
		return -1;
	}
	uint32_t pcie_cmd_reg = 0;
	pcie_read_dword(ahciInfo.bus, ahciInfo.dev, ahciInfo.func, 0x4, &pcie_cmd_reg);
	pcie_cmd_reg|=(1<<0);
	pcie_cmd_reg|=(1<<1);
	pcie_cmd_reg|=(1<<2);
	pcie_write_dword(ahciInfo.bus, ahciInfo.dev, ahciInfo.func, 0x4, pcie_cmd_reg);
	volatile struct ahci_hba_mem* pBase = (volatile struct ahci_hba_mem*)ahciInfo.pBase;
	if (pBase->os_handoff_ctrl_status&(1<<0)&&0){
		printf("firmware still locked on\r\n");
		pBase->os_handoff_ctrl_status|=(1<<1);
		sleep(50);
		while (pBase->os_handoff_ctrl_status&(1<<2)){};
	}
	if (pBase->os_handoff_ctrl_status&(1<<0)){
		printf("firmware refused to remove AHCI HBA lock\r\n");
		return -1;
	}
	if (ahci_init_cmd_list(&cmdListDesc)!=0){
		printf("failed to initialize cmd list\r\n");
		return -1;
	}
	uint32_t ahci_version = 0;
	ahci_read_dword(0x10, &ahci_version);
	printf("AHCI controller version: 0x%x\r\n", ahci_version);
	for (uint32_t i = 0;i<AHCI_MAX_PORTS;i++){
		if (ahci_drive_exists(i)!=0)
			continue;
		printf("AHCI device at port: %d\r\n", i);
		struct ahci_drive_info driveInfo = {0};
		if (ahci_get_drive_info(i, &driveInfo)!=0){
			printf("failed to get drive info\r\n");
			continue;
		}
		printf("drive size: %dMB\r\n", (driveInfo.sector_count*DRIVE_SECTOR_SIZE)/MEM_MB);
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
		printf("failed to get AHCI controller\r\n");
		return -1;
	}
	if (pcie_get_bar(bus, dev, func, 5, &pBase)!=0){
		printf("failed to get AHCI base\r\n");
		return -1;
	}
	if (!pBase){
		printf("invalid AHCI base\r\n");
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
	volatile uint8_t* pReg = (uint8_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_byte(uint64_t offset, uint8_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	volatile uint8_t* pReg = (uint8_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_write_word(uint64_t offset, uint16_t value){
	if (!ahciInfo.pBase)
		return -1;
	volatile uint16_t* pReg = (uint16_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_word(uint64_t offset, uint16_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	volatile uint16_t* pReg = (uint16_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_write_dword(uint64_t offset, uint32_t value){
	if (!ahciInfo.pBase)
		return -1;
	volatile uint32_t* pReg = (uint32_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_dword(uint64_t offset, uint32_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	volatile uint32_t* pReg = (uint32_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_write_qword(uint64_t offset, uint64_t value){
	if (!ahciInfo.pBase)
		return -1;
	volatile uint64_t* pReg = (uint64_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pReg = value;	
	return 0;
}
int ahci_read_qword(uint64_t offset, uint64_t* pValue){
	if (!ahciInfo.pBase)
		return -1;
	if (!pValue)
		return -1;
	volatile uint64_t* pReg = (uint64_t*)(((uint8_t*)ahciInfo.pBase)+offset);
	*pValue = *pReg;
	return 0;
}
int ahci_get_port_base(uint8_t port, volatile struct ahci_port_mem** ppBase){
	if (!ppBase)
		return -1;
	uint64_t port_offset = AHCI_PORT_OFFSET(port);
	volatile struct ahci_port_mem* pBase = (struct ahci_port_mem*)(ahciInfo.pBase+port_offset);
	*ppBase = pBase;
	return 0;
}
int ahci_drive_exists(uint8_t drive_port){
	if (drive_port>32)
		return -1;
	volatile struct ahci_port_mem* pPort = (struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(drive_port, &pPort)!=0)
		return -1;
	if (pPort->signature!=AHCI_SATA_SIGNATURE){
		return -1;
	}
	return 0;
}
int ahci_init_cmd_list(struct ahci_cmd_list_desc* pCmdListDesc){
	if (!pCmdListDesc)
		return -1;
	volatile struct ahci_cmd_hdr* pCmdList = (volatile struct ahci_cmd_hdr*)0x0;
	if (virtualAllocPage((uint64_t*)&pCmdList, PTE_RW|PTE_NX, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	uint64_t maxTableSize = sizeof(struct ahci_cmd_table)+(AHCI_MAX_PRDT_ENTRIES*sizeof(struct ahci_prdt_entry));
	uint64_t cmdTableListSize = maxTableSize*AHCI_MAX_CMD_ENTRIES;
	uint64_t cmdTableListPages = align_up(cmdTableListSize, PAGE_SIZE)/PAGE_SIZE;
	volatile struct ahci_cmd_table* pCmdTableList = (volatile struct ahci_cmd_table*)0x0;
	printf("cmd list pages: %d\r\n", cmdTableListPages);
	if (virtualAllocPages((uint64_t*)&pCmdTableList, cmdTableListPages, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	memset((void*)pCmdList, 0, sizeof(struct ahci_cmd_hdr)*AHCI_MAX_CMD_ENTRIES);
	memset((void*)pCmdTableList, 0, cmdTableListSize);
	pCmdListDesc->pCmdList = pCmdList;
	pCmdListDesc->pCmdTableList = pCmdTableList;
	pCmdListDesc->cmdTableListPages = cmdTableListPages;
	pCmdListDesc->currentEntry = 0;
	if (virtualToPhysical((uint64_t)pCmdList, (uint64_t*)&pCmdListDesc->pCmdList_pa)!=0){
		return -1;
	}
	if (virtualToPhysical((uint64_t)pCmdTableList, (uint64_t*)&pCmdListDesc->pCmdTableList_pa)!=0){
		return -1;
	}
	if (pCmdListDesc->pCmdTableList_pa%1024)
		return -1;
	return 0;
}
int ahci_deinit_cmd_list(struct ahci_cmd_list_desc* pCmdListDesc){
	if (!pCmdListDesc)
		return -1;
	if (virtualFreePage((uint64_t)pCmdListDesc->pCmdList, 0)!=0)
		return -1;
	if (virtualFreePages((uint64_t)pCmdListDesc->pCmdTableList, pCmdListDesc->cmdTableListPages)!=0)
		return -1;
	return 0;
}
int ahci_push_cmd_table(struct ahci_cmd_list_desc* pCmdListDesc, uint64_t prdt_cnt, volatile struct ahci_cmd_hdr** ppCmdHdr, volatile struct ahci_cmd_table** ppCmdTable){
	if (!pCmdListDesc||!ppCmdHdr||!ppCmdTable)
		return -1;
	if (pCmdListDesc->currentEntry>=AHCI_MAX_CMD_ENTRIES)
		return -1;
	if (prdt_cnt>AHCI_MAX_PRDT_ENTRIES)
		return -1;
	volatile struct ahci_cmd_hdr* pCmdHdr = pCmdListDesc->pCmdList+pCmdListDesc->currentEntry;
	volatile struct ahci_cmd_table* pCmdTable = pCmdListDesc->pCmdTableList+pCmdListDesc->currentEntry;
	uint64_t cmdTableSize = sizeof(struct ahci_cmd_table)+(prdt_cnt*sizeof(struct ahci_prdt_entry));
	memset((void*)pCmdTable, 0, cmdTableSize);
	uint64_t pCmdTable_pa = 0;
	if (virtualToPhysical((uint64_t)pCmdTable, &pCmdTable_pa)!=0){
		printf("faileld to convert cmd table VA to PA\r\n");
		return -1;
	}
	pCmdHdr->cmd_table_base = (uint64_t)pCmdTable_pa;
	pCmdHdr->prdt_len = prdt_cnt;
	*ppCmdHdr = pCmdHdr;
	*ppCmdTable = pCmdTable;
	pCmdListDesc->currentEntry++;
	return 0;
}
int ahci_pop_cmd_table(struct ahci_cmd_list_desc* pCmdListDesc){
	if (!pCmdListDesc)
		return -1;
	if (!pCmdListDesc->currentEntry)
		return -1;
	volatile struct ahci_cmd_hdr* pTop = (pCmdListDesc->pCmdList+(pCmdListDesc->currentEntry-1));
	volatile struct ahci_cmd_table* pCmdTable = (volatile struct ahci_cmd_table*)pTop->cmd_table_base;
	pCmdListDesc->currentEntry--;
	return 0;
}
int ahci_write_prdt(volatile struct ahci_cmd_table* pCmdTable, uint64_t prdt_index, uint64_t va, uint64_t size){
	if (!pCmdTable)
		return -1;
	volatile struct ahci_prdt_entry* pPrdtEntry = (volatile struct ahci_prdt_entry*)(pCmdTable->prdt_list+prdt_index);
	uint64_t pa = 0;
	if (virtualToPhysical(va, &pa)!=0)
		return -1;
	if (!pa){
		printf("unmapped page\r\n");
		return -1;
	}
	pPrdtEntry->base = pa;
	pPrdtEntry->reserved0 = 0;
	pPrdtEntry->byte_cnt = size-1;
	pPrdtEntry->reserved1 = 0;
	pPrdtEntry->interrupt = 0;
	return 0;
}
int ahci_run_port(uint8_t port){
	volatile struct ahci_port_mem* pPort = (struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(port, &pPort)!=0)
		return -1;
	while (pPort->cmd&(1<<4)){};
	pPort->cmd|=(1<<0);
	pPort->cmd|=(1<<4);
	return 0;
}
int ahci_stop_port(uint8_t port){
	volatile struct ahci_port_mem* pPort = (struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(port, &pPort)!=0)
		return -1;
	pPort->cmd&=~(1<<0);
	pPort->cmd&=~(1<<4);
	while (pPort->cmd&(1<<4)){};
	return 0;
}
int ahci_poll_port_finish(uint8_t port, uint8_t cmd_max){
	if (cmd_max>AHCI_MAX_CMD_ENTRIES)
		return -1;
	volatile struct ahci_port_mem* pPort = (struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(port, &pPort)!=0)
		return -1;
	for (uint64_t i = 0;i<cmd_max;i++){
		pPort->cmd_issue|=(1<<i);
		while (pPort->cmd_issue&(1<<i)){};
	}
	return 0;
}
int ahci_debug_cmd_table(volatile struct ahci_cmd_hdr* pCmdHdr, volatile struct ahci_cmd_table* pCmdTable){
	if (!pCmdHdr||!pCmdTable)
		return -1;
	virtualMapPage((uint64_t)pCmdHdr, (uint64_t)pCmdHdr, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, 0, PAGE_TYPE_MMIO);
	virtualMapPage((uint64_t)pCmdTable, (uint64_t)pCmdTable, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, 0, PAGE_TYPE_MMIO);
	if (!pCmdHdr->cmd_table_base){
		printf("invalid cmd table\r\n");
		return -1;
	}
	if (!pCmdHdr->cmd_fis_len){
		printf("no FIS\r\n");
		return -1;
	}
	printf("--FIS data--\r\n");
	printf("FIS length: %d\r\n", pCmdHdr->cmd_fis_len);	
	volatile struct ahci_fis_host_to_dev* pFisBase = (volatile struct ahci_fis_host_to_dev*)pCmdTable->fis;
	if (!pFisBase){
		printf("invalid FIS\r\n");
		return -1;
	}
	printf("FIS type: 0x%x\r\n", (uint32_t)pFisBase->fis_type);
	switch (pFisBase->fis_type){
		case AHCI_FIS_TYPE_H2D:{
			volatile struct ahci_fis_host_to_dev* pFis = (volatile struct ahci_fis_host_to_dev*)pFisBase;
			printf("FIS cmd: 0x%x\r\n", pFis->cmd);	
			break;
		}
	};
	printf("--PRDT data\r\n");
	for (uint64_t prdt_index = 0;prdt_index<pCmdHdr->prdt_len;prdt_index++){
		volatile struct ahci_prdt_entry* pPrdt = (volatile struct ahci_prdt_entry*)(pCmdTable->prdt_list+prdt_index);
		printf("--prdt %d--\r\n", prdt_index);
		printf("physical address: %p\r\n", (void*)pPrdt->base);
		printf("size: %d\r\n", pPrdt->byte_cnt);
	}
	return 0;
}
int ahci_drive_error(uint8_t drive_port){
	volatile struct ahci_port_mem* pPort = (volatile struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(drive_port, &pPort)!=0)
		return -1;
	if (pPort->interrupt_status&(1<<30))
		return -1;
	if (pPort->task_file_data&1)
		return -1;
	return 0;
}
int ahci_get_drive_info(uint8_t drive_port, struct ahci_drive_info* pDriveInfo){
	if (!pDriveInfo)
		return -1;
	ahci_stop_port(drive_port);
	volatile struct ahci_port_mem* pPort = (volatile struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(drive_port, &pPort)!=0)
		return -1;
	volatile struct ahci_cmd_hdr* pCmdHdr = (volatile struct ahci_cmd_hdr*)0x0;
	volatile struct ahci_cmd_table* pCmdTable = (volatile struct ahci_cmd_table*)0x0;
	if (ahci_push_cmd_table(&cmdListDesc, 1, &pCmdHdr, &pCmdTable)!=0){
		printf("failed to push cmd to cmd table\r\n");
		return -1;
	}
	volatile struct ahci_fis_host_to_dev* pIdentFis = (volatile struct ahci_fis_host_to_dev*)pCmdTable->fis;
	if (!pIdentFis){
		printf("invalid FIS\r\n");
		ahci_pop_cmd_table(&cmdListDesc);
		return -1;
	}
	memset((void*)pIdentFis, 0, sizeof(struct ahci_fis_host_to_dev));
	pIdentFis->fis_type = AHCI_FIS_TYPE_H2D;
	pIdentFis->c = 1;
	pIdentFis->cmd = AHCI_CMD_IDENT;
	pCmdHdr->cmd_fis_len = sizeof(struct ahci_fis_host_to_dev)/sizeof(uint32_t);
	pCmdHdr->w = 0;
	uint16_t driveData[256] = {0};
	memset((void*)driveData, 0, sizeof(driveData));
	ahci_write_prdt(pCmdTable, 0, (uint64_t)driveData, sizeof(driveData));
	pPort->cmdlist_base = (uint64_t)cmdListDesc.pCmdList_pa;
	ahci_run_port(drive_port);
	if (ahci_poll_port_finish(drive_port, 1)!=0){
		printf("error when running commands\r\n");
		ahci_pop_cmd_table(&cmdListDesc);
		return -1;
	}
	ahci_stop_port(drive_port);
	if (ahci_drive_error(drive_port)!=0){
		printf("error when reading drive data sector\r\n");
		ahci_pop_cmd_table(&cmdListDesc);
		return -1;
	}
	if (ahci_pop_cmd_table(&cmdListDesc)!=0){
		printf("failed to pop cmd off cmd table\r\n");
		return -1;
	}
	uint16_t cap_value = driveData[83];
	uint64_t sector_count = 0;
	if (!(cap_value&(1<<26))){
		sector_count = (uint64_t)(*(uint32_t*)(driveData+60));
	}else
		sector_count = *(uint64_t*)(driveData+120);
	struct ahci_drive_info driveInfo = {0};
	driveInfo.port = drive_port;
	driveInfo.sector_count = sector_count;
	*pDriveInfo = driveInfo;
	return 0;
}
int ahci_read(struct ahci_drive_info driveInfo, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer){
		return -1;
	}
	if (lba+sector_count>driveInfo.sector_count){
		printf("sectors requested to read exceeds drive size\r\n");
		return -1;
	}
	if (sector_count>8){
		uint64_t reads_max = align_up(sector_count, 8)/8;
		for (uint64_t i = 0;i<reads_max;i++){
			uint64_t read_lba = lba+(i*8);
			unsigned char* read_buffer = pBuffer+(i*4096);
			uint64_t read_count = 8;
			if (i==reads_max-1&&sector_count%8)
				read_count = sector_count%8;
			if (ahci_read(driveInfo, read_lba, read_count, read_buffer)!=0){
				printf("failed to read sector\r\n");
				return -1;
			}
		}
		return 0;
	}
	uint64_t bufferSize = ((uint64_t)sector_count)*DRIVE_SECTOR_SIZE;
	ahci_stop_port(driveInfo.port);
	volatile struct ahci_port_mem* pPort = (volatile struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(driveInfo.port, &pPort)!=0){
		return -1;
	}
	volatile struct ahci_cmd_hdr* pCmdHdr = (volatile struct ahci_cmd_hdr*)0x0;
	volatile struct ahci_cmd_table* pCmdTable = (volatile struct ahci_cmd_table*)0x0;
	if (ahci_push_cmd_table(&cmdListDesc, 1, &pCmdHdr, &pCmdTable)!=0){
		printf("failed to push cmd table\r\n");
		return -1;
	}
	volatile struct ahci_fis_host_to_dev* pFis = (volatile struct ahci_fis_host_to_dev*)pCmdTable->fis;
	memset((void*)pFis, 0, sizeof(struct ahci_fis_host_to_dev));
	pCmdHdr->w = 0;
	pCmdHdr->cmd_fis_len = sizeof(struct ahci_fis_host_to_dev)/sizeof(uint32_t);
	pFis->fis_type = AHCI_FIS_TYPE_H2D;
	pFis->cmd = AHCI_CMD_READ;
	pFis->c = 1;
	pFis->lba0 = (uint8_t)lba;
	pFis->lba1 = (uint8_t)(lba>>8);
	pFis->lba2 = (uint8_t)(lba>>16);
	pFis->lba3 = (uint8_t)(lba>>24);
	pFis->lba4 = (uint8_t)(lba>>32);
	pFis->lba5 = (uint8_t)(lba>>40);
	pFis->count = sector_count;
	pFis->device = 1<<6;
	ahci_write_prdt(pCmdTable, 0, (uint64_t)pBuffer, bufferSize);
	pPort->cmdlist_base = cmdListDesc.pCmdList_pa;
	ahci_run_port(driveInfo.port);
	if (ahci_poll_port_finish(driveInfo.port, 1)!=0){
		printf("failed to poll port\r\n");
		ahci_pop_cmd_table(&cmdListDesc);
		return -1;
	}
	ahci_stop_port(driveInfo.port);
	if (ahci_drive_error(driveInfo.port)!=0){
		printf("error when reading sectors\r\n");
		ahci_pop_cmd_table(&cmdListDesc);
		return -1;
	}
	if (ahci_pop_cmd_table(&cmdListDesc)!=0){
		printf("failed to pop cmd table\r\n");
		return -1;
	}
	return 0;
}
int ahci_write(struct ahci_drive_info driveInfo, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	if (!pBuffer)
		return -1;
	if (lba+sector_count>driveInfo.sector_count)
		return -1;
	if (sector_count>8){
		uint64_t writes_max = align_up(sector_count, 8)/8;
		for (uint64_t i = 0;i<writes_max;i++){
			uint64_t write_lba = lba+(i*8);	
			unsigned char* write_buffer = pBuffer+(i*4096);
			uint64_t write_count = 8;
			if (i==writes_max-1&&sector_count%8)
				write_count = sector_count%8;
			if (ahci_write(driveInfo, write_lba, write_count, write_buffer)!=0)
				return -1;
		}	
		return 0;
	}
	uint64_t bufferSize = ((uint64_t)sector_count)*DRIVE_SECTOR_SIZE;
	ahci_stop_port(driveInfo.port);
	volatile struct ahci_port_mem* pPort = (volatile struct ahci_port_mem*)0x0;
	if (ahci_get_port_base(driveInfo.port, &pPort)!=0)
		return -1;
	volatile struct ahci_cmd_hdr* pCmdHdr = (volatile struct ahci_cmd_hdr*)0x0;
	volatile struct ahci_cmd_table* pCmdTable = (volatile struct ahci_cmd_table*)0x0;
	if (ahci_push_cmd_table(&cmdListDesc, 1, &pCmdHdr, &pCmdTable)!=0){
		printf("failed to push cmd table\r\n");
		return -1;
	}
	volatile struct ahci_fis_host_to_dev* pFis = (volatile struct ahci_fis_host_to_dev*)pCmdTable->fis;
	memset((void*)pFis, 0, sizeof(struct ahci_fis_host_to_dev));
	pCmdHdr->w = 1;
	pCmdHdr->cmd_fis_len = sizeof(struct ahci_fis_host_to_dev)/sizeof(uint32_t);
	pFis->fis_type = AHCI_FIS_TYPE_H2D;
	pFis->cmd = AHCI_CMD_WRITE;
	pFis->c = 1;
	pFis->lba0 = (uint8_t)lba;
	pFis->lba1 = (uint8_t)(lba>>8);
	pFis->lba2 = (uint8_t)(lba>>16);
	pFis->lba3 = (uint8_t)(lba>>24);
	pFis->lba4 = (uint8_t)(lba>>32);
	pFis->lba5 = (uint8_t)(lba>>40);
	pFis->count = sector_count;
	pFis->device = 1<<6;
	ahci_write_prdt(pCmdTable, 0, (uint64_t)pBuffer, bufferSize);
	pPort->cmdlist_base = cmdListDesc.pCmdList_pa;
	ahci_run_port(driveInfo.port);
	if (ahci_poll_port_finish(driveInfo.port, 1)!=0){
		printf("failed to poll port\r\n");
		ahci_pop_cmd_table(&cmdListDesc);
		return -1;
	}
	ahci_stop_port(driveInfo.port);
	if (ahci_drive_error(driveInfo.port)!=0){
		printf("error when reading sectors\r\n");
		ahci_pop_cmd_table(&cmdListDesc);
		return -1;
	}
	if (ahci_pop_cmd_table(&cmdListDesc)!=0){
		printf("failed to pop cmd table\r\n");
		return -1;
	}
	return 0;
}
int ahci_subsystem_read(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	if (!pHdr||!pBuffer)
		return -1;
	if (pHdr->type!=DRIVE_TYPE_AHCI)
		return -1;
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)pHdr;
	return ahci_read(pDev->driveInfo, lba, sector_count, pBuffer);
}
int ahci_subsystem_write(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer){
	if (!pHdr||!pBuffer)
		return -1;
	if (pHdr->type!=DRIVE_TYPE_AHCI)
		return -1;
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)pHdr;
	return ahci_write(pDev->driveInfo, lba, sector_count, pBuffer);
}
int ahci_subsystem_get_drive_info(struct drive_dev_hdr* pHdr, struct drive_info* pDriveInfo){
	if (!pHdr||!pDriveInfo)
		return -1;
	if (pHdr->type!=DRIVE_TYPE_AHCI)
		return -1;
	struct drive_dev_ahci* pDev = (struct drive_dev_ahci*)pHdr;
	struct drive_info driveInfo = {0};
	driveInfo.driveType = DRIVE_TYPE_AHCI;
	driveInfo.sector_count = pDev->driveInfo.sector_count;
	*pDriveInfo = driveInfo;
	return 0;
}
