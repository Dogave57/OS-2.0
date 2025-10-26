#include "msr.h"
#include "stdlib.h"
#include "cpuid.h"
#include "port.h"
#include "pit.h"
#include "pic.h"
#include "acpi.h"
#include "timer.h"
#include "apic.h"
uint64_t lapic_base = 0;
uint64_t lapic_version = 0;
uint64_t main_lapic_id = 0;
uint64_t ioapic_base = 0;
uint32_t ioapic_version = 0;
uint32_t ioapic_id = 0;
uint32_t ioapic_max_redirs = 0;
unsigned int x2lapic_supported = 0;
int apic_init(void){
	x2lapic_is_supported(&x2lapic_supported);
	if (!x2lapic_supported){
		printf(L"x2lapic is unsupported!\r\n");
		return -1;
	}
	lapic_base = read_msr(LAPIC_BASE_MSR);
	lapic_base&=0xfffff000;
	if (x2lapic_supported)
		lapic_base|=0xC00;
	else
		lapic_base|=0x800;
	write_msr(LAPIC_BASE_MSR, lapic_base);
	uint64_t base = 0;
	uint64_t value = 0;
	uint64_t div_conf = 3;
	lapic_get_version(&lapic_version);
	printf(L"LAPIC version: %x\r\n", lapic_version);
	lapic_get_id(&main_lapic_id);
	printf(L"LAPIC id: 0x%x\r\n", main_lapic_id);
	lapic_write_reg(LAPIC_REG_TPR, 0);
	lapic_write_reg(LAPIC_REG_LVT_TIMER, (1<<17));
	lapic_write_reg(LAPIC_REG_DIV_CONFIG, div_conf);
	pic_init();
	pit_set_freq(0, 1000);
	uint64_t start_time = get_time_ms();
	uint64_t elapsed_ms = 0;
	uint64_t time_ms = 0;
	uint64_t start_ticks = 0xFFFFFFFF;
	uint64_t time_precision_ms = 10;
	lapic_write_reg(LAPIC_REG_INIT_COUNT, start_ticks);
	while (elapsed_ms<time_precision_ms){
		time_ms = get_time_ms();
		elapsed_ms = time_ms-start_time;
	}
	lapic_write_reg(LAPIC_REG_LVT_TIMER, (1<<16));
	uint64_t elapsed_ticks = 0;
	lapic_read_reg(LAPIC_REG_CURRENT_COUNT, &elapsed_ticks);
	elapsed_ticks = (start_ticks-elapsed_ticks);
	uint64_t tpms = elapsed_ticks/time_precision_ms;
	outb(0x21, 0xFF);
	outb(0x00, 0x00);
	outb(0xA1, 0xFF);
	outb(0x00, 0x00);
	timer_reset();
	timer_set_tpms(tpms);
	lapic_write_reg(LAPIC_REG_DIV_CONFIG, div_conf);
	lapic_write_reg(LAPIC_REG_INIT_COUNT, tpms);
	lapic_write_reg(LAPIC_REG_LVT_TIMER, 0x30|(1<<17));
	lapic_read_reg(LAPIC_REG_SPI, &value);
	value|=0x100;
	lapic_write_reg(LAPIC_REG_SPI, value);
	if (ioapic_get_base(&ioapic_base)){
		printf(L"failed to get IOAPIC base\r\n");
		return -1;
	}
	ioapic_get_version(&ioapic_version);
	ioapic_get_id(&ioapic_id);
	ioapic_get_max_redirs(&ioapic_max_redirs);
	printf(L"IOAPIC base: %p\r\n", (void*)ioapic_base);
	printf(L"IOAPIC version: 0x%x\r\n", ioapic_version);
	printf(L"IOAPIC id: 0x%x\r\n", ioapic_id);
	printf(L"IOAPIC max redirection entries: %d\r\n", ioapic_max_redirs);
	ioapic_enable_irq(1, 0x40, (uint8_t)main_lapic_id);
	return 0;
}
int lapic_get_version(uint64_t* pversion){
	if (!pversion)
		return -1;
	return lapic_read_reg(LAPIC_REG_LAPIC_VERSION, pversion);
}
int lapic_get_id(uint64_t* pId){
	if (!pId)
		return -1;
	return lapic_read_reg(LAPIC_REG_LAPIC_ID, pId);
}
int x2lapic_write_reg(unsigned int reg, uint64_t value){
	reg/=16;
	write_msr(0x800+reg, value);
	return 0;
}
int x2lapic_read_reg(unsigned int reg, uint64_t* pvalue){
	if (!pvalue)
		return -1;
	reg/=16;
	*pvalue = read_msr(0x800+reg);
	return 0;
}
int lapic_write_reg(unsigned int reg, uint64_t value){
	if (x2lapic_supported){
		x2lapic_write_reg(reg, value);
		return 0;
	}
	*(unsigned int*)(lapic_base+reg) = (unsigned int)value;
	return 0;
}
int lapic_read_reg(unsigned int reg, uint64_t* pvalue){
	if (!pvalue)
		return -1;
	if (x2lapic_supported){
		x2lapic_read_reg(reg, pvalue);
		return 0;
	}
	*(unsigned int*)pvalue = *(unsigned int*)(lapic_base+reg);
	return 0;
}
int lapic_send_eoi(void){
	lapic_write_reg(LAPIC_REG_EOI, 0);
	return 0;
}
int x2lapic_is_supported(unsigned int* psupported){
	unsigned int ecx = 0;
	cpuid(0x1, 0x0, (unsigned int*)0x0, (unsigned int*)0x0, &ecx, (unsigned int*)0x0);
	*psupported = (ecx>>21)&1;
	return 0;
}
int ioapic_get_base(uint64_t* pbase){
	if (!pbase)
		return -1;
	if (!pIoApicInfo)
		return -1;
	*pbase = (uint64_t)pIoApicInfo->ioapic_base;
	return 0;
}
int ioapic_get_version(uint32_t* pversion){
	if (!pversion)
		return -1;
	uint32_t version = 0;
	if (ioapic_read_reg(IOAPIC_REG_VERSION, &version)!=0)
		return -1;
	version&=0xFF;
	*pversion = version;
	return 0;
}
int ioapic_get_max_redirs(uint32_t* pmax_redirs){
	if (!pmax_redirs)
		return -1;
	uint32_t max_redirs = 0;
	if (ioapic_read_reg(IOAPIC_REG_VERSION, &max_redirs)!=0)
		return -1;
	max_redirs = ((max_redirs>>16)&0xFF)+1;
	*pmax_redirs = max_redirs;
	return 0;
}
int ioapic_get_id(uint32_t* pId){
	if (!pId)
		return -1;
	return ioapic_read_reg(IOAPIC_REG_ID, pId);
}
int ioapic_write_reg(uint32_t reg, uint32_t value){
	unsigned char* pbase = (unsigned char*)ioapic_base;
	*(unsigned int*)(pbase) = (unsigned int)reg;
	*(unsigned int*)(pbase+0x10) = value;
	outb(0x00, 0x00);
	return 0;	
}
int ioapic_read_reg(uint32_t reg, uint32_t* pvalue){
	if (!pvalue)
		return -1;
	unsigned char* pbase = (unsigned char*)ioapic_base;
	*(unsigned int*)(pbase) = reg;
	outb(0x00, 0x00);
	*pvalue = *(unsigned int*)(pbase+0x10);
	return 0;
}
int ioapic_enable_irq(uint8_t irq, uint8_t vector, uint8_t lapic_id){
	__asm__ volatile("cli");
	uint32_t reg = 0x10+(irq*2);
	uint32_t low = vector;
	low|=(0<<8);
	low|=(0<<15);
	uint32_t high = (uint32_t)lapic_id<<24;
	ioapic_write_reg(reg, low);
	ioapic_write_reg(reg+1, high);
	__asm__ volatile("sti");
	return 0;
}
