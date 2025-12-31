#ifndef _HPET
#define _HPET
#include <stdint.h>
#include "kernel_include.h"
#include "drivers/acpi.h"
struct hpet_timer_mmio{
	uint64_t config_cap;
	uint64_t comparator;
	uint64_t interrupt_route;
	uint8_t reserved0[170];
}__attribute__((packed));
struct hpet_base_mmio{
	uint64_t cap;
	uint64_t reserved0;
	uint64_t config;
	uint64_t reserved1;
	uint64_t status;
	uint64_t reserved2[25];
	uint64_t counter;
	uint64_t reserved3;
	struct hpet_timer_mmio timer_mmio[];
}__attribute__((packed));
struct hpet_info_t{
	struct acpi_hpet* pAcpiTable;
	volatile struct hpet_base_mmio* pBaseMmio;
};
int hpet_init(void);
KAPI int hpet_get_counter(uint64_t* pCounter);
KAPI int hpet_set_counter(uint64_t counter);
KAPI int hpet_get_tick_period(uint32_t* pTickPeriod);
KAPI int hpet_get_ticks_per_ms(uint64_t* pTicks);
KAPI uint64_t hpet_get_ms(void);
KAPI int hpet_set_ms(uint64_t time);
int hpet_get_info(struct hpet_info_t* pHpetInfo);
#endif
