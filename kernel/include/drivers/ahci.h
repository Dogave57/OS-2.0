#ifndef _AHCI
#define _AHCI
struct ahci_info{
	uint64_t pBase;
	uint8_t bus;
	uint8_t dev;
	uint8_t func;
};
int ahci_init(void);
int ahci_get_info(struct ahci_info* pInfo);
#endif
