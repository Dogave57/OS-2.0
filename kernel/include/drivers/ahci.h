#ifndef _AHCI
#define _AHCI
#define AHCI_DRIVE_MMIO_OFFSET 0x100
#define AHCI_DRIVE_SIGNATURE 0x101
#define AHCI_MAX_PORTS 32
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
struct ahci_prdt_entry{
	uint32_t buffer_base_lower;
	uint32_t buffer_base_upper;
	uint32_t byte_count;
	uint32_t flags;
}__attribute__((packed));
struct ahci_cmd_table{
	uint8_t cmd_fis[64];
	uint8_t atapi[16];
	uint8_t reserved[48];
	struct ahci_prdt_entry prdt[];
}__attribute__((packed));
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
int ahci_read_sectors(uint8_t port, uint64_t sector, uint64_t sector_cnt, uint8_t* pBuffer);
#endif
