#ifndef _PCIE
#define _PCIE
#define PCIE_CLASS_DRIVE_CONTROLLER 0x01
#define PCIE_SUBCLASS_NVME_CONTROLLER 0x08
struct pcie_info{
	uint64_t pBase;
	uint8_t startBus;
	uint8_t endBus;
};
struct pcie_location{
	uint8_t bus;
	uint8_t dev;
	uint8_t func;	
};
int pcie_init(void);
int pcie_get_info(struct pcie_info* pInfo);
int pcie_get_ecam_base(uint8_t bus, uint8_t dev, uint8_t func, uint64_t* pBase);
int pcie_read_byte(uint8_t bus, uint8_t dev, uint8_t func, uint64_t byte_offset, uint8_t* pValue);
int pcie_write_byte(uint8_t bus, uint8_t dev, uint8_t func, uint64_t byte_offset, uint8_t value);
int pcie_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint64_t word_offset, uint16_t* pValue);
int pcie_write_word(uint8_t bus, uint8_t dev, uint8_t func, uint64_t word_offset, uint16_t value);
int pcie_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t dword_offset, uint16_t* pValue);
int pcie_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t dword_offset, uint32_t value);
int pcie_read_qword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t qword_offset, uint64_t* pValue);
int pcie_write_qword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t qword_offset, uint64_t value);
int pcie_get_vendor_id(uint8_t bus, uint8_t dev, uint8_t func, uint16_t* pVendorId);
int pcie_get_device_id(uint8_t bus, uint8_t dev, uint8_t func, uint16_t* pDeviceId);
int pcie_get_bar(uint8_t bus, uint8_t dev, uint8_t func, uint64_t barType, uint64_t* pBar);
int pcie_get_class(uint8_t bus, uint8_t dev, uint8_t func, uint8_t* pClass);
int pcie_get_subclass(uint8_t bus, uint8_t dev, uint8_t func, uint8_t* pSubclass);
int pcie_device_exists(uint8_t bus, uint8_t dev, uint8_t func);
int pcie_get_device_by_class(uint8_t class, uint8_t subclass, uint8_t* pBus, uint8_t* pDev, uint8_t* pFunc);
#endif
