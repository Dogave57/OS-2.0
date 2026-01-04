#include "kernel_include.h"
#include "panic.h"
#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "drivers/acpi.h"
#include "drivers/hpet.h"
struct hpet_info_t hpetInfo = {0};
int hpet_init(void){
	if (hpet_get_info(&hpetInfo)!=0){
		printf("failed to get HPET info\r\n");
		return -1;
	}
	volatile struct hpet_base_mmio* pBase = hpetInfo.pBaseMmio;
	if (virtualMapPage((uint64_t)pBase, (uint64_t)pBase, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map HPET MMIO base\r\n");
		return -1;
	}
	uint32_t tickPeriod = 0;
	if (hpet_get_tick_period(&tickPeriod)!=0){
		printf("failed to get HPET tick period\r\n");
		return -1;
	}
	pBase->config|=(1<<0);
	return 0;
}
KAPI int hpet_get_counter(uint64_t* pCounter){
	if (!pCounter)
		return -1;
	volatile struct hpet_base_mmio* pBase = hpetInfo.pBaseMmio;
	if (!pBase)
		return -1;
	*pCounter = pBase->counter;
	return 0;
}
KAPI int hpet_set_counter(uint64_t counter){
	volatile struct hpet_base_mmio* pBase = hpetInfo.pBaseMmio;
	if (!pBase)
		return -1;
	pBase->counter = counter;
	return 0;
}
KAPI int hpet_get_tick_period(uint32_t* pTickPeriod){
	if (!pTickPeriod)
		return -1;
	volatile struct hpet_base_mmio* pBase = hpetInfo.pBaseMmio;
	if (!pBase)
		return -1;
	uint32_t tickPeriod = (uint32_t)((pBase->cap>>32)&0xFFFFFFFF);
	*pTickPeriod = tickPeriod;
	return 0;
}
KAPI int hpet_get_ticks_per_us(uint64_t* pMicroSeconds){
	if (!pMicroSeconds)
		return -1;
	uint64_t tickPeriod = 0;
	if (hpet_get_tick_period((uint32_t*)&tickPeriod)!=0)
		return -1;
	*pMicroSeconds = 1000000000/tickPeriod;
	return 0;
}
KAPI uint64_t hpet_get_us(void){
	uint64_t tickCount = 0;
	if (hpet_get_counter(&tickCount)!=0)
		return -1;
	if (!tickCount)
		return 0;
	uint64_t tpus = 0;
	if (hpet_get_ticks_per_us(&tpus)!=0)
		return -1;
	uint64_t time_us = tickCount/tpus;
	return time_us;
}
KAPI int hpet_set_us(uint64_t time_us){
	uint64_t tpus = 0;
	if (hpet_get_ticks_per_us(&tpus)!=0)
		return -1;
	uint64_t tickCount = tpus*time_us;
	if (hpet_set_counter(tickCount)!=0)
		return -1;
	return 0;
}
int hpet_get_info(struct hpet_info_t* pHpetInfo){
	if (!pHpetInfo)
		return -1;
	if (hpetInfo.pBaseMmio){
		*pHpetInfo = hpetInfo;
		return 0;
	}
	struct acpi_hpet* pAcpiTable = (struct acpi_hpet*)0x0;
	if (acpi_find_table((unsigned int)'TEPH', (struct acpi_sdt_hdr**)&pAcpiTable)!=0)
		return -1;
	pHpetInfo->pAcpiTable = pAcpiTable;
	pHpetInfo->pBaseMmio = (struct hpet_base_mmio*)(pAcpiTable->base.address);
	if (!pHpetInfo->pBaseMmio){
		panic("your firmware is cooked bro\r\n");
		return -1;
	}
	return 0;
}
