#include "bootloader.h"
#include "port.h"
#include "interrupt.h"
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
	};
	unsigned int cpu_exception_entries = sizeof(cpu_exception_table)/sizeof(uint64_t);
	for (unsigned int i = 0;i<cpu_exception_entries;i++){
		uint64_t isr = cpu_exception_table[i];
		idt_add_entry(i, isr, 0x8E);
	}
	idtr.limit = (uint16_t)(sizeof(struct idt_entry_t)*IDT_MAX_ENTRIES)-1;
	idtr.base = (uint64_t)idt;
	outb(0x21, 0xFF);
	outb(0xA1, 0xFF);
	load_idt(idtr);
	return 0;
}
