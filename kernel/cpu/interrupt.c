#include "bootloader.h"
#include "cpu/port.h"
#include "cpu/interrupt.h"
__attribute__((aligned(0x10)))
static struct idt_entry_t idt[IDT_MAX_ENTRIES] = {0};
static struct idt_ptr_t idtr = {0};
int idt_add_entry(uint8_t vector, uint64_t isr, uint8_t flags, uint8_t ist_entry){
	struct idt_entry_t* pentry = &idt[vector];
	pentry->flags = flags;
	pentry->isr_low = (uint16_t)(isr&0xFFFF);
	pentry->isr_middle = (uint16_t)((isr>>16)&0xFFFF);
	pentry->isr_high = (uint32_t)((isr>>32)&0xFFFFFFFF);
	pentry->cs = 0x08;
	pentry->reserved = 0;
	pentry->ist = ist_entry;
	return 0;
}
int idt_init(void){
	for (unsigned int i = 0;i<IDT_MAX_ENTRIES;i++){
		idt_add_entry(i, (uint64_t)default_isr, 0x8E, 0x0);
	}
	struct cpu_exception_entry cpu_exception_table[] = {
		{(uint64_t)isr0, 0, 0},
		{(uint64_t)isr1, 0, 1},
		{(uint64_t)isr2, 1, 2},
		{(uint64_t)isr3, 0, 3},
		{(uint64_t)isr4, 0, 4},
		{(uint64_t)isr5, 0, 5},
		{(uint64_t)isr6, 0, 6},
		{(uint64_t)isr7, 0, 7},
		{(uint64_t)isr8, 1, 8},
		{(uint64_t)isr10, 0, 10},
		{(uint64_t)isr11, 0, 11},
		{(uint64_t)isr12, 0, 12},
		{(uint64_t)isr13, 0, 13},
		{(uint64_t)isr14, 0, 14},
		{(uint64_t)isr16, 0, 16},
		{(uint64_t)isr17, 0, 17},
		{(uint64_t)isr18, 1, 18},
		{(uint64_t)isr19, 1, 19},
		{(uint64_t)isr20, 0, 20},
	};
	unsigned int cpu_exception_entries = sizeof(cpu_exception_table)/sizeof(cpu_exception_table[0]);
	for (unsigned int i = 0;i<cpu_exception_entries;i++){
		struct cpu_exception_entry exceptionEntry = cpu_exception_table[i];
		exceptionEntry.istVector = 1;
		idt_add_entry(exceptionEntry.idtVector, exceptionEntry.pIsr, 0x8E, exceptionEntry.istVector);
	}
	idt_add_entry(0x20, (uint64_t)pic_timer_isr, 0x8E, 0x0);
	idt_add_entry(0x30, (uint64_t)timer_isr, 0x8E, 0x0);
	idt_add_entry(0x31, (uint64_t)thermal_isr, 0x8E, 0x0);
	idt_add_entry(0x40, (uint64_t)ps2_kbd_isr, 0x8E, 0x0);
	idtr.limit = (uint16_t)(sizeof(struct idt_entry_t)*IDT_MAX_ENTRIES)-1;
	idtr.base = (uint64_t)idt;
	outb(0x21, 0xFF);
	outb(0xA1, 0xFF);
	load_idt(&idtr);
	return 0;
}
