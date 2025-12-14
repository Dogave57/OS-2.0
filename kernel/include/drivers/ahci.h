#ifndef _AHCI
#define _AHCI
#include "subsystem/drive.h"
#define AHCI_DRIVE_MMIO_OFFSET 0x100
#define AHCI_SATA_SIGNATURE 0x101
#define AHCI_MAX_PORTS 32
#define AHCI_MAX_CMD_ENTRIES 32
#define AHCI_MAX_PRDT_ENTRIES 8
#define AHCI_PORT_OFFSET(port)(AHCI_DRIVE_MMIO_OFFSET+(port*0x80))
#define AHCI_CMD_IDENT 0xEC
#define AHCI_CMD_READ 0x25
#define AHCI_CMD_WRITE 0x35
#define AHCI_CMD_CLEAR_BIT 0x8000
#define AHCI_FIS_TYPE_H2D 0x27
#define AHCI_FIS_TYPE_D2H 0x34
#define AHCI_FIS_TYPE_DMA_ACT 0x39
#define AHCI_FIS_TYPE_DMA_SETUP 0x41
#define AHCI_FIS_TYPE_DATA 0x46
#define AHCI_FIS_TYPE_BIST 0x58
#define AHCI_FIS_TYPE_PIO_SETUP 0x5F
#define AHCI_FIS_TYPE_DEV_BITS 0xA1
struct ahci_info{
	uint64_t pBase;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
};
struct ahci_fis_host_to_dev{
	uint8_t fis_type;
	uint8_t portmulti:4;
	uint8_t reserved0:3;
	uint8_t c:1;
	uint8_t cmd;
	uint8_t feature_low;
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t feature_high;
	uint16_t count;
	uint8_t icc;
	uint8_t ctrl;
	uint8_t reserved1[4];
}__attribute__((packed));
struct ahci_fis_dev_to_host{
	uint8_t fis_type;
	uint8_t portmulti:4;
	uint8_t reserved0:2;
	uint8_t interrupt:1;
	uint8_t reserved1:1;
	uint8_t status;
	uint8_t error;
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t reserved2;
	uint8_t count_low;
	uint8_t count_high;
	uint8_t reserved3[2];
	uint8_t reserved4[4];
}__attribute__((packed));
struct ahci_port_mem{
	uint64_t cmdlist_base;
	uint64_t fis_base;
	uint32_t interrupt_status;
	uint32_t interrupt_enable;
	uint32_t cmd;
	uint32_t reserved0;
	uint32_t task_file_data;
	uint32_t signature;
	uint32_t sata_status;
	uint32_t sata_ctrl;
	uint32_t sata_error;
	uint32_t sata_active;
	uint32_t cmd_issue;
	uint32_t sata_notification;
	uint32_t fis_based_switch_ctrl;
	uint32_t reserved1[11];
	uint32_t vendor[4];
}__attribute__((packed));
struct ahci_hba_mem{
	uint32_t capabilities;
	uint32_t global_host_ctrl;
	uint32_t interrupt_status;
	uint32_t port_implemented;
	uint32_t version;
	uint32_t ccc_ctrl;
	uint32_t ccc_ports;
	uint32_t enclosure_management_location;
	uint32_t enclosure_management_ctrl;
	uint32_t capabilities_extended;
	uint32_t os_handoff_ctrl_status;
	uint8_t reserved[0x74];
	uint8_t vendor[0x60];
	struct ahci_port_mem ports[32];
}__attribute__((packed));
struct ahci_cmd_hdr{
	uint8_t cmd_fis_len:5;
	uint8_t atapi:1;
	uint8_t w:1;
	uint8_t p:1;
	uint8_t reset:1;
	uint8_t b:1;
	uint8_t c:1;
	uint8_t reserved0:1;
	uint8_t portmulti:4;
	uint16_t prdt_len;
	uint32_t prdt_byte_count_transferred;
	uint64_t cmd_table_base;
	uint32_t reserved1[4];
}__attribute__((packed));
struct ahci_prdt_entry{
	uint64_t base;
	uint32_t reserved0;
	uint32_t byte_cnt:22;
	uint32_t reserved1:9;
	uint32_t interrupt:1;
}__attribute__((packed));
struct ahci_cmd_table{
	uint8_t fis[64];
	uint8_t atapi[16];
	uint8_t reserved0[48];
	struct ahci_prdt_entry prdt_list[];
}__attribute__((packed));
struct ahci_cmd_list_desc{
	volatile struct ahci_cmd_hdr* pCmdList;
	volatile struct ahci_cmd_table* pCmdTableList;
	uint64_t cmdTableListPages;
	uint64_t currentEntry;
	uint64_t pCmdList_pa;
	uint64_t pCmdTableList_pa;
};
int ahci_init(void);
int ahci_get_info(struct ahci_info* pInfo);
int ahci_write_byte(uint64_t offset, uint8_t value);
int ahci_read_byte(uint64_t offset, uint8_t* pValue);
int ahci_write_word(uint64_t offset, uint16_t value);
int ahci_read_word(uint64_t offset, uint16_t* pValue);
int ahci_write_dword(uint64_t offset, uint32_t value);
int ahci_read_dword(uint64_t offset, uint32_t* pValue);
int ahci_write_qword(uint64_t offset, uint64_t value);
int ahci_read_qword(uint64_t offset, uint64_t* pValue);
int ahci_get_port_base(uint8_t port, volatile struct ahci_port_mem** ppBase);
int ahci_drive_exists(uint8_t port);
int ahci_init_cmd_list(struct ahci_cmd_list_desc* pCmdListDesc);
int ahci_deinit_cmd_list(struct ahci_cmd_list_desc* pCmdListDesc);
int ahci_push_cmd_table(struct ahci_cmd_list_desc* pCmdListDesc, uint64_t prdt_cnt, volatile struct ahci_cmd_hdr** ppCmdHdr, volatile struct ahci_cmd_table** ppCmdTable);
int ahci_write_prdt(volatile struct ahci_cmd_table* pCmdTable, uint64_t prdt_index, uint64_t va, uint64_t size);
int ahci_pop_cmd_table(struct ahci_cmd_list_desc* pCmdListDesc);
int ahci_run_port(uint8_t port);
int ahci_stop_port(uint8_t port);
int ahci_poll_port_finish(uint8_t port, uint8_t cmd_max);
int ahci_debug_cmd_list(volatile struct ahci_cmd_hdr* pCmdHdr, volatile struct ahci_cmd_table* pCmdTable);
int ahci_drive_error(uint8_t port);
int ahci_get_drive_info(uint8_t drive_port, struct ahci_drive_info* pInfo);
int ahci_read(struct ahci_drive_info driveInfo, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int ahci_write(struct ahci_drive_info driveInfo, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int ahci_subsystem_read(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int ahci_subsystem_write(struct drive_dev_hdr* pHdr, uint64_t lba, uint16_t sector_count, unsigned char* pBuffer);
int ahci_subsystem_get_drive_info(struct drive_dev_hdr* pHdr, struct drive_info* pDriveInfo);
#endif
