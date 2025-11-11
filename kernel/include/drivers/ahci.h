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
#endif
