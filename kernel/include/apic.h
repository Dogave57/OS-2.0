#ifndef _APIC
#define _APIC
#include <stdint.h>
#define LAPIC_BASE_MSR 0x1B
#define LAPIC_REG_LAPIC_ID 0x20
#define LAPIC_REG_LAPIC_VERSION 0x30
#define LAPIC_REG_TPR 0x80
#define LAPIC_REG_APR 0x90
#define LAPIC_REG_PPR 0xA0
#define LAPIC_REG_EOI 0xB0
#define LAPIC_REG_RRD 0xC0
#define LAPIC_REG_LDR 0xD0
#define LAPIC_REG_DFR 0xE0
#define LAPIC_REG_SPI 0xF0
#define LAPIC_REG_ERR_STATUS 0x280
#define LAPIC_REG_LVT_CMCI 0x2F0
#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_LVT_THERMAL 0x330
#define LAPIC_REG_LVT_PMC 0x340
#define LAPIC_REG_INIT_COUNT 0x380
#define LAPIC_REG_CURRENT_COUNT 0x390
#define LAPIC_REG_DIV_CONFIG 0x3E0
extern uint64_t apic_base;
int apic_init(void);
int lapic_get_version(uint64_t* pversion);
int x2lapic_write_reg(unsigned int reg, uint64_t value);
int x2lapic_read_reg(unsigned int reg, uint64_t* pvalue);
int lapic_send_eoi(void);
int lapic_write_reg(unsigned int reg, uint64_t value);
int lapic_read_reg(unsigned int reg, uint64_t* pvalue);
int x2lapic_is_supported(unsigned int* psupported);
int ioapic_get_base(uint64_t* pbase);
#endif
