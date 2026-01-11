#ifndef _PCIE
#define _PCIE
#define PCIE_CLASS_DRIVE_CONTROLLER 0x01
#define PCIE_SUBCLASS_NVME_CONTROLLER 0x08
#define PCIE_CLASS_USB_HCI (0x0C)
#define PCIE_SUBCLASS_USB (0x03)
#define PCIE_PROGIF_XHCI (0x30)
#define PCIE_CAP_ID_PM 0x04
struct pcie_info{
	uint64_t pBase;
	uint64_t pBase_phys;
	uint8_t startBus;
	uint8_t endBus;
};
struct pcie_location{
	uint8_t bus;
	uint8_t dev;
	uint8_t func;	
};
struct pcie_cap_link{
	uint16_t cap_id:8;
	uint16_t next_offset:8;
}__attribute__((packed));
struct pcie_cap_pm{
	uint16_t power_state:2;
	uint16_t reserved0:1;
	uint16_t pme_enable:1;
	uint16_t reserved1:4;
	uint16_t pme_status:1;
	uint16_t reserved2:7;
}__attribute__((packed));
struct pcie_msix_msg_ctrl{
	struct pcie_cap_link capLink;
	uint32_t table_size:11;
	uint32_t reserved0:3;
	uint32_t vector_mask:1;
	uint32_t msix_enable:1;
	uint32_t table_info_bar:3;
	uint32_t table_info_offset:29;
	uint32_t pba_offset;
}__attribute__((packed));
struct pcie_msix_table_info{
	uint32_t table_bar:3;
	uint32_t table_offset:29;
}__attribute__((packed));
struct pcie_msix_table_entry_msg_addr{
	uint64_t reserved0:2;
	uint64_t dest_mode:1;
	uint64_t redir_hint:1;
	uint64_t reserved1:8;
	uint64_t dest_lapic_id:8;
	uint64_t magic:12;
	uint64_t reserved2:32;
}__attribute__((packed));
struct pcie_msix_table_entry_msg_data{
	uint32_t vector:8;
	uint32_t delivery_mode:3;
	uint32_t reserved0:3;
	uint32_t level:1;
	uint32_t trigger:1;
	uint32_t reserved1:26;
}__attribute__((packed));
struct pcie_msix_table_entry_vector_ctrl{
	uint32_t mask:1;
	uint32_t reserved0:31;
}__attribute__((packed));
struct pcie_msix_table_entry{
	struct pcie_msix_table_entry_msg_addr msg_addr;
	struct pcie_msix_table_entry_msg_data msg_data;
	struct pcie_msix_table_entry_vector_ctrl vector_ctrl;
}__attribute__((packed));
int pcie_init(void);
int pcie_get_info(struct pcie_info* pInfo);
int pcie_get_ecam_base(uint8_t bus, uint8_t dev, uint8_t func, uint64_t* pBase);
int pcie_read_byte(uint8_t bus, uint8_t dev, uint8_t func, uint64_t byte_offset, uint8_t* pValue);
int pcie_write_byte(uint8_t bus, uint8_t dev, uint8_t func, uint64_t byte_offset, uint8_t value);
int pcie_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint64_t word_offset, uint16_t* pValue);
int pcie_write_word(uint8_t bus, uint8_t dev, uint8_t func, uint64_t word_offset, uint16_t value);
int pcie_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t dword_offset, uint32_t* pValue);
int pcie_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t dword_offset, uint32_t value);
int pcie_read_qword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t qword_offset, uint64_t* pValue);
int pcie_write_qword(uint8_t bus, uint8_t dev, uint8_t func, uint64_t qword_offset, uint64_t value);
int pcie_get_cap_ptr(uint8_t bus, uint8_t dev, uint8_t func, uint8_t cap_id, struct pcie_cap_link** ppCapLink);
int pcie_get_vendor_id(uint8_t bus, uint8_t dev, uint8_t func, uint16_t* pVendorId);
int pcie_get_device_id(uint8_t bus, uint8_t dev, uint8_t func, uint16_t* pDeviceId);
int pcie_get_progif(uint8_t bus, uint8_t dev, uint8_t func, uint8_t* pProgIf);
int pcie_get_bar(uint8_t bus, uint8_t dev, uint8_t func, uint64_t barType, uint64_t* pBar);
int pcie_get_class(uint8_t bus, uint8_t dev, uint8_t func, uint8_t* pClass);
int pcie_get_subclass(uint8_t bus, uint8_t dev, uint8_t func, uint8_t* pSubclass);
int pcie_device_exists(uint8_t bus, uint8_t dev, uint8_t func);
int pcie_get_device_by_class(uint8_t class, uint8_t subclass, uint8_t* pBus, uint8_t* pDev, uint8_t* pFunc);
int pcie_get_device_by_id(uint16_t vendor_id, uint16_t device_id, uint8_t* pBus, uint8_t* pDev, uint8_t* pFunc);
int pcie_msix_get_table_info(uint8_t bus, uint8_t dev, uint8_t func, volatile struct pcie_msix_msg_ctrl* pMsgControl, volatile struct pcie_msix_table_info** ppTableInfo);
int pcie_msix_get_table_base(uint8_t bus, uint8_t dev, uint8_t func, volatile struct pcie_msix_msg_ctrl* pMsgControl, volatile struct pcie_msix_table_entry** ppTableBase);
int pcie_set_msix_entry(volatile struct pcie_msix_table_entry* pEntry, uint64_t lapic_id, uint8_t interrupt_vector);
#endif
