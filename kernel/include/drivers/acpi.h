#ifndef _ACPI
#define _ACPI
#include <stdint.h>
#define XSDP_SIGNATURE (uint64_t)0x2052545020445352
#define XSDT_SIGNATURE (uint64_t)0x54445358
#define MADT_ENTRY_LAPIC 0
#define MADT_ENTRY_IOAPIC 1
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
extern struct acpi_xdsp* pXdsp;
extern struct acpi_sdt_hdr* pXsdt;
extern struct acpi_madtEntry_ioapic* pIoApicInfo;
int acpi_init(void);
int acpi_find_table(unsigned int signature, struct acpi_sdt_hdr** ppTable);
int acpi_find_xsdt(struct acpi_sdt_hdr** ppXsdt);
int acpi_find_xsdp(struct acpi_xsdp** ppRsdp);
#endif
