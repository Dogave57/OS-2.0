#include "bootloader.h"
#include "cpu/port.h"
#include "cpu/interrupt.h"
__attribute__((aligned(0x10)))
static struct idt_entry_t idt[IDT_MAX_ENTRIES] = {0};
static struct idt_ptr_t idtr = {0};
int idt_add_entry(uint8_t vector, uint64_t isr, uint8_t flags){
	struct idt_entry_t* pentry = &idt[vector];
	pentry->flags = flags;
	pentry->isr_low = (uint16_t)(isr&0xFFFF);
	pentry->isr_middle = (uint16_t)((isr>>16)&0xFFFF);
	pentry->isr_high = (uint32_t)((isr>>32)&0xFFFFFFFF);
	pentry->cs = 0x08;
	pentry->reserved = 0;
	pentry->ist = 0;
	return 0;
}
int idt_init(void){
	for (unsigned int i = 0;i<IDT_MAX_ENTRIES;i++){
		idt_add_entry(i, (uint64_t)default_isr, 0x8E);
	}
	uint64_t cpu_exception_table[] = {
		(uint64_t)isr0,
		(uint64_t)isr1,
		(uint64_t)isr2,
		(uint64_t)isr3,
		(uint64_t)isr4,
		(uint64_t)isr5,
		(uint64_t)isr5,
		(uint64_t)isr6,
		(uint64_t)isr7,
		(uint64_t)isr8,
		(uint64_t)isr10,
		(uint64_t)isr11,
		(uint64_t)isr12,
		(uint64_t)isr13,
		(uint64_t)isr14,
		(uint64_t)0,
		(uint64_t)isr16,
		(uint64_t)isr17,
		(uint64_t)isr18,
		(uint64_t)isr19,
		(uint64_t)isr20,
	};
	unsigned int cpu_exception_entries = sizeof(cpu_exception_table)/sizeof(uint64_t);
	for (unsigned int i = 0;i<cpu_exception_entries;i++){
		uint64_t isr = cpu_exception_table[i];
		idt_add_entry(i, isr, 0x8E);
	}
	idt_add_entry(0x20, (uint64_t)pic_timer_isr, 0x8E);
	idt_add_entry(0x30, (uint64_t)timer_isr, 0x8E);
	idt_add_entry(0x31, (uint64_t)thermal_isr, 0x8E);
	idt_add_entry(0x40, (uint64_t)ps2_kbd_isr, 0x8E);
	idtr.limit = (uint16_t)(sizeof(struct idt_entry_t)*IDT_MAX_ENTRIES)-1;
	idtr.base = (uint64_t)idt;
	outb(0x21, 0xFF);
	outb(0xA1, 0xFF);
	load_idt(idtr);
	return 0;
}
