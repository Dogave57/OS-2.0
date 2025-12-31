#ifndef _ACPI
#define _ACPI
#include <stdint.h>
#define XSDP_SIGNATURE (uint64_t)0x2052545020445352
#define XSDT_SIGNATURE (uint64_t)0x54445358
#define MADT_ENTRY_LAPIC 0
#define MADT_ENTRY_IOAPIC 1
#define AML_OPCODE_NAMESPACE_BEGIN 0x10
struct acpi_sdt_hdr{
	uint32_t signature;
	uint32_t len;
	uint8_t revision;
	uint8_t checksum;
	uint8_t oem_id[6];
	uint64_t oem_table_id;
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
}__attribute__((packed));
struct acpi_address{
	uint8_t address_space_id;
	uint8_t register_bit_width;
	uint8_t register_bit_offset;
	uint8_t reserved;
	uint64_t address;
}__attribute__((packed));
struct acpi_rsdp{
	uint64_t signature;
	uint8_t checksum;
	uint8_t oem_id[6];
	uint8_t revision;
	uint32_t rsdt_addr;
}__attribute__((packed));
struct acpi_xsdp{
	uint64_t signature;
	uint8_t checksum;
	uint8_t oem_id[6];
	uint8_t revision;
	uint32_t rsdt_addr;
	uint32_t len;
	uint64_t xsdt_addr;
	uint8_t extendedChecksum;
	uint8_t reserved[3];
}__attribute__((packed));
struct acpi_madt{
	struct acpi_sdt_hdr hdr;
	uint32_t lapic_base;
	uint32_t lapic_flags;
}__attribute__((packed));
struct acpi_madtEntry_hdr{
	uint8_t type;
	uint8_t len;
}__attribute__((packed));
struct acpi_madtEntry_lapic{
	uint16_t acpi_processor_id;
	uint8_t apic_id;
	uint32_t flags;
}__attribute__((packed));
struct acpi_madtEntry_ioapic{
	uint8_t ioapic_id;
	uint8_t reserved;
	uint32_t ioapic_base;
	uint32_t global_system_int_base;
}__attribute__((packed));
struct acpi_mcfgEntry{
	uint64_t pBase;
	uint16_t segmentGroup;
	uint8_t startBus;
	uint8_t endBus;
	uint8_t padding[4];
}__attribute__((packed));
struct acpi_mcfgHdr{
	struct acpi_sdt_hdr hdr;
	struct acpi_mcfgEntry firstEntry;
}__attribute__((packed));
struct acpi_fadt_address{
	uint8_t addressSpace;
	uint8_t bitWidth;
	uint8_t bitOffset;
	uint8_t accessSize;
	uint64_t address;
};
struct acpi_fadt{
	struct acpi_sdt_hdr hdr;
	uint32_t firmwareCtrl;
	uint32_t dsdt;
	uint8_t reserved0;
	uint8_t preferredPowerManagementProfile;
	uint16_t sciInterrupt;
	uint32_t smiCmdPort;
	uint8_t acpiEnable;
	uint8_t acpiDisable;
	uint8_t s4biosReq;
	uint8_t powerStateControl;
	uint32_t pm1aEventBlock;
	uint32_t pm1bEventBlock;
	uint32_t pm1aControlBlock;
	uint32_t pm1bControlBlock;
	uint32_t pm2ControlBlock;
	uint32_t pmTimerBlock;
	uint32_t GPE0Block;
	uint32_t GPE1Block;
	uint8_t pm1EventLength;
	uint8_t pm1ControlLength;
	uint8_t pm2ControlLength;
	uint8_t pmTimerLength;
	uint8_t GPE0Length;
	uint8_t GPE1Length;
	uint8_t GPE1Base;
	uint8_t CStateControl;
	uint16_t worstC2Latency;
	uint16_t worstC3Latency;
	uint16_t flushSize;
	uint16_t flushStride;
	uint8_t dutyOffset;
	uint8_t dutyWidth;
	uint8_t dayAlarm;
	uint8_t monthAlarm;
	uint8_t century;
	uint16_t bootArchitectureFlags;
	uint8_t reserved1;
	uint32_t flags;
	struct acpi_fadt_address resetReg;
	uint8_t resetValue;
	uint8_t reserved2[3];
	uint64_t x_firmwareControl;
	uint64_t x_dsdt;
	struct acpi_fadt_address x_pm1aEventBlock;
	struct acpi_fadt_address x_pm1bEventBlock;
	struct acpi_fadt_address x_pm1aControlBlock;
	struct acpi_fadt_address x_pm1bControlBlock;
	struct acpi_fadt_address x_pm2ControlBlock;
	struct acpi_fadt_address x_pmTimerBlock;
	struct acpi_fadt_address x_GPE0Block;
	struct acpi_fadt_address x_GPE1Block;
}__attribute__((packed));
struct acpi_hpet{
	struct acpi_sdt_hdr sdtHeader;	
	uint8_t ident;
	uint16_t vendor_id;
	uint8_t hpet_number;
	struct acpi_address base;
	uint16_t minimum_tick;
	uint8_t page_protection;
}__attribute__((packed));
struct aml_buffer{
	unsigned char* pBuffer;
	uint64_t bufferSize;
};
struct aml_context{
	struct aml_buffer aml;
};
struct aml_execution_context{
	unsigned char* ip;
	unsigned char* sb;
	unsigned char* sp;
	uint64_t stackSize;
};
extern struct acpi_xdsp* pXdsp;
extern struct acpi_sdt_hdr* pXsdt;
extern struct acpi_madtEntry_ioapic* pIoApicInfo;
int acpi_init(void);
int acpi_find_table(unsigned int signature, struct acpi_sdt_hdr** ppTable);
int acpi_find_xsdt(struct acpi_sdt_hdr** ppXsdt);
int acpi_find_xsdp(struct acpi_xsdp** ppRsdp);
int acpi_find_dsdt(struct acpi_sdt_hdr** ppDsdtHdr);
int aml_init_execution_context(struct aml_execution_context** ppExecutionContext);
int aml_push_stack(struct aml_execution_context* pExecutionContext, uint64_t size);
int aml_pop_stack(struct aml_execution_context* pExecutionContext, uint64_t size);
int aml_push_frame(struct aml_execution_context* pExecutionContext);
int aml_pop_frame(struct aml_execution_context* pExecutionContext);
int aml_run_instruction(struct aml_execution_context* pExecutionContext);
int shutdown(void);
#endif
