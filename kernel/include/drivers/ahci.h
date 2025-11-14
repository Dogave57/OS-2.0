#ifndef _AHCI
#define _AHCI
#define AHCI_DRIVE_MMIO_OFFSET 0x100
#define AHCI_DRIVE_SIGNATURE 0x101
#define AHCI_MAX_PORTS 32
#define AHCI_MAX_CMD_SLOTS 32
#define AHCI_PORT_CMD_TABLE_LOW_REG 0x0
#define AHCI_PORT_CMD_TABLE_HIGH_REG 0x4
#define AHCI_PORT_SIGNATURE_REG 0x24
#define AHCI_PORT_CR_REG 0x18
#define AHCI_PORT_SATA_ERROR_REG 0x30
#define AHCI_PORT_SATA_STATUS_REG 0x28
#define AHCI_PORT_CMD_ISSUE_REG 0x38
#define AHCI_PORT_OFFSET(port)(AHCI_DRIVE_MMIO_OFFSET+(0x80*port))
#define AHCI_CMD_IDENT_DEV 0xEC
#define AHCI_CMD_CLEAR_BIT 0x8000
struct ahci_info{
	uint64_t pBase;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
};
struct ahci_cmd_hdr{
	uint16_t flags;
	uint16_t prdt_len;
	uint32_t prdt_byte_count;
	uint32_t cmd_table_base_lower;
	uint32_t cmd_table_base_upper;
	uint32_t reserved[4];
}__attribute__((packed));
struct ahci_prdt{
	uint32_t buffer_base_lower;
	uint32_t buffer_base_upper;
	uint32_t buffer_size;
	uint32_t flags;
}__attribute__((packed));
struct ahci_cmd_table{
	uint8_t cmd_fis[64];
	uint8_t atapi[16];
	uint8_t reserved[48];
	struct ahci_prdt prdt_list[];
}__attribute__((packed));
struct ahci_drive_info{
	uint8_t port;
	uint64_t driveSize;
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
int ahci_drive_exists(uint8_t port);
int ahci_init_cmd_table(struct ahci_cmd_hdr** ppCmdTable);
int ahci_deinit_cmd_table(struct ahci_cmd_hdr* pCmdTable);
int ahci_set_cmd_table(uint8_t drive_port, struct ahci_cmd_hdr* pCmdHdr);
int ahci_write_prdt(struct ahci_prdt* pPrdt, uint64_t va, uint32_t buffer_size);
int ahci_start_cmd(uint8_t port);
int ahci_stop_cmd(uint8_t port);
int ahci_init_cmd_list(struct ahci_cmd_hdr** pCmdTable);
int ahci_deinit_cmd_list(struct ahci_cmd_hdr* pCmdTable);
int ahci_clear_cmd_list(struct ahci_cmd_hdr* pCmdTable);
int ahci_alloc_cmd(struct ahci_cmd_table** ppCmd, struct ahci_cmd_hdr* pCmdTable, uint64_t index, uint64_t prdt_cnt);
int ahci_free_cmd(struct ahci_cmd_hdr* pCmdTable, uint64_t index);
int ahci_get_drive_info(uint8_t drive_port, struct ahci_drive_info* pInfo);
#endif
