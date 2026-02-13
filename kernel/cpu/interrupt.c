#include "bootloader.h"
#include "stdlib/stdlib.h"
#include "cpu/port.h"
#include "cpu/interrupt.h"
__attribute__((aligned(0x10)))
static struct idt_entry_t idt[IDT_MAX_ENTRIES] = {0};
static struct idt_ptr_t idtr = {0};
int idt_add_entry(uint8_t vector, uint64_t target_isr, uint8_t flags, uint8_t ist_entry, uint8_t no_stub){
	no_stub = (vector<32) ? 1 : no_stub;
	uint64_t isr = no_stub ? target_isr : isr_stub_list[vector];
	if (!no_stub)
		isr_stub_target_list[vector] = target_isr;
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
int idt_get_entry(uint8_t vector, struct idt_entry_t** ppEntry){
	if (!ppEntry||vector>=IDT_MAX_ENTRIES)
		return -1;
	*ppEntry = idt+(uint64_t)vector;
	return 0;
}
int idt_get_free_vector(uint8_t* pVector){
	if (!pVector)
		return -1;
	for (uint64_t i = IDT_FREE_VECTOR_START;i<IDT_FREE_VECTOR_END;i++){
		struct idt_entry_t* pEntry = (struct idt_entry_t*)0x0;
		if (idt_get_entry((uint8_t)i, &pEntry)!=0)
			return -1;
		if (pEntry->isr_low)
			continue;
		*pVector = (uint8_t)i;
		return 0;
	}
	return -1;
}
int idt_init(void){
	memset((void*)idt, 0, sizeof(idt));
	struct cpu_exception_entry cpu_exception_table[] = {
		{(uint64_t)isr0, 1, 0},
		{(uint64_t)isr1, 1, 1},
		{(uint64_t)isr2, 1, 2},
		{(uint64_t)isr3, 1, 3},
		{(uint64_t)isr4, 1, 4},
		{(uint64_t)isr5, 1, 5},
		{(uint64_t)isr6, 1, 6},
		{(uint64_t)isr7, 1, 7},
		{(uint64_t)isr8, 1, 8},
		{(uint64_t)isr10, 1, 10},
		{(uint64_t)isr11, 1, 11},
		{(uint64_t)isr12, 1, 12},
		{(uint64_t)isr13, 1, 13},
		{(uint64_t)isr14, 1, 14},
		{(uint64_t)isr16, 1, 16},
		{(uint64_t)isr17, 1, 17},
		{(uint64_t)isr18, 1, 18},
		{(uint64_t)isr19, 1, 19},
		{(uint64_t)isr20, 1, 20},
	};
	unsigned int cpu_exception_entries = sizeof(cpu_exception_table)/sizeof(cpu_exception_table[0]);
	for (unsigned int i = 0;i<cpu_exception_entries;i++){
		struct cpu_exception_entry exceptionEntry = cpu_exception_table[i];
		idt_add_entry(exceptionEntry.idtVector, exceptionEntry.pIsr, 0x8E, exceptionEntry.istVector, 1);
	}
	idt_add_entry(0x20, (uint64_t)pic_timer_isr, 0x8E, 0x0, 1);
	idt_add_entry(0x21, (uint64_t)default_isr, 0x8E, 0x0, 1);
	idt_add_entry(0x30, (uint64_t)timer_isr, 0x8E, 0x0, 1);
	idt_add_entry(0x31, (uint64_t)thermal_isr, 0x8E, 0x0, 1);
	idt_add_entry(0x40, (uint64_t)ps2_kbd_isr, 0x8E, 0x0, 1);
	idtr.limit = (uint16_t)(sizeof(struct idt_entry_t)*IDT_MAX_ENTRIES)-1;
	idtr.base = (uint64_t)idt;
	outb(0x21, 0xFF);
	outb(0xA1, 0xFF);
	load_idt(&idtr);
	return 0;
}
