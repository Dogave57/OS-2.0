#ifndef _PCIE
#define _PCIE
#define PCIE_CLASS_DRIVE_CONTROLLER (0x01)
#define PCIE_SUBCLASS_NVME_CONTROLLER (0x08)
#define PCIE_CLASS_USB_HC (0x0C)
#define PCIE_SUBCLASS_USB (0x03)
#define PCIE_PROGIF_XHC (0x30)
#define PCIE_CAP_ID_PM (0x04)
#define PCIE_MAX_BUS_COUNT (256)
#define PCIE_MAX_DEVICE_COUNT (PCIE_MAX_BUS_COUNT*32)
#define PCIE_MAX_FUNCTION_COUNT (PCIE_MAX_DEVICE_COUNT*8)
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
	uint8_t cap_id;
	uint8_t next_offset;
}__attribute__((packed));
struct pcie_cap_pm{
	uint16_t power_state:2;
	uint16_t reserved0:1;
	uint16_t pme_enable:1;
	uint16_t reserved1:4;
	uint16_t pme_status:1;
	uint16_t reserved2:7;
}__attribute__((packed));
struct pcie_msi_msg_ctrl{
	struct pcie_cap_link capLink;
	uint16_t msi_enable:1;
	uint16_t multi_message_enable:1;
	uint16_t multi_message_capable:3;
	uint16_t reserved0:9;
}__attribute__((packed));
struct pcie_msix_msg_ctrl{
	struct pcie_cap_link capLink;
	uint16_t table_size:11;
	uint16_t reserved0:3;
	uint16_t vector_mask:1;
	uint16_t msix_enable:1;
	union{
		struct{
			uint32_t table_bar:3;
			uint32_t table_offset:29;
		};
		uint32_t table_address;
	};
	union{
		struct{
			uint32_t pba_bar:3;
			uint32_t pba_offset:29;
		};
		uint32_t pba_address;
	};
}__attribute__((packed));
union pcie_msix_table_entry_msg_data{
	struct{
		uint32_t vector:8;
		uint32_t delivery_mode:3;
		uint32_t reserved0:3;
		uint32_t level:1;
		uint32_t trigger:1;
		uint32_t reserved1:16;
	};
	uint32_t raw;
}__attribute__((packed));
union pcie_msix_table_entry_vector_ctrl{
	struct{
		uint32_t mask:1;
		uint32_t reserved0:31;
	};
	uint32_t raw;
}__attribute__((packed));
struct pcie_msix_table_entry{
	uint32_t msg_addr_low;
	uint32_t msg_addr_high;
	union pcie_msix_table_entry_msg_data msg_data;
	union pcie_msix_table_entry_vector_ctrl vector_ctrl;
}__attribute__((packed));
int pcie_init(void);
int pcie_get_info(struct pcie_info* pInfo);
int pcie_get_ecam_base(struct pcie_location location, uint64_t* pBase);
int pcie_read_byte(struct pcie_location location, uint64_t byte_offset, uint8_t* pValue);
int pcie_write_byte(struct pcie_location location, uint64_t byte_offset, uint8_t value);
int pcie_read_word(struct pcie_location location, uint64_t word_offset, uint16_t* pValue);
int pcie_write_word(struct pcie_location location, uint64_t word_offset, uint16_t value);
int pcie_read_dword(struct pcie_location location, uint64_t dword_offset, uint32_t* pValue);
int pcie_write_dword(struct pcie_location location, uint64_t dword_offset, uint32_t value);
int pcie_read_qword(struct pcie_location location, uint64_t qword_offset, uint64_t* pValue);
int pcie_write_qword(struct pcie_location location, uint64_t qword_offset, uint64_t value);
int pcie_get_cap_ptr(struct pcie_location location, uint8_t cap_id, struct pcie_cap_link** ppCapLink);
int pcie_get_vendor_id(struct pcie_location location, uint16_t* pVendorId);
int pcie_get_device_id(struct pcie_location location, uint16_t* pDeviceId);
int pcie_get_progif(struct pcie_location location, uint8_t* pProgIf);
int pcie_get_bar(struct pcie_location location, uint64_t barType, uint64_t* pBar);
int pcie_get_bar_flags(struct pcie_location location, uint64_t barType, uint8_t* pFlags);
int pcie_get_class(struct pcie_location location, uint8_t* pClass);
int pcie_get_subclass(struct pcie_location location, uint8_t* pSubclass);
int pcie_function_exists(struct pcie_location location);
int pcie_get_function_by_class(uint8_t class, uint8_t subclass, struct pcie_location* pLocation);
int pcie_get_function_by_id(uint16_t vendor_id, uint16_t device_id, struct pcie_location* pLocation);
int pcie_msix_get_msg_ctrl(struct pcie_location location, volatile struct pcie_msix_msg_ctrl** ppMsgControl);
int pcie_msi_get_msg_ctrl(struct pcie_location location, volatile struct pcie_msi_msg_ctrl** ppMsgControl);
int pcie_msi_enable(volatile struct pcie_msi_msg_ctrl* pMsgControl);
int pcie_msi_disable(volatile struct pcie_msi_msg_ctrl* pMsgControl);
int pcie_msix_get_table_base(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, volatile struct pcie_msix_table_entry** ppTableBase);
int pcie_msix_get_pba_base(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, volatile uint8_t** ppPba);
int pcie_get_msix_entry(struct pcie_location location, volatile struct pcie_msix_table_entry** ppEntry, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector);
int pcie_set_msix_entry(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector, uint64_t lapic_id, uint8_t interrupt_vector);
int pcie_msix_enable(volatile struct pcie_msix_msg_ctrl* pMsgControl);
int pcie_msix_disable(volatile struct pcie_msix_msg_ctrl* pMsgControl);
int pcie_msix_enable_entry(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector);
int pcie_msix_disable_entry(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector);
int pcie_msix_clear_pba(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector);
int pcie_msix_get_pba(struct pcie_location location, volatile struct pcie_msix_msg_ctrl* pMsgControl, uint64_t msix_vector);
#endif
