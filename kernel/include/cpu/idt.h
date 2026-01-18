#ifndef _IDT
#define _IDT
#include <stdint.h>
#define IDT_MAX_ENTRIES 256
#define IDT_FREE_VECTOR_START (0x20)
#define IDT_FREE_VECTOR_END (IDT_MAX_ENTRIES)
struct idt_entry_t{
	uint16_t isr_low;
	uint16_t cs;
	uint8_t ist;
	uint8_t flags;
	uint16_t isr_middle;
	uint32_t isr_high;
	uint32_t reserved;
}__attribute__((packed));
struct idt_ptr_t{
	uint16_t limit;
	uint64_t base;
}__attribute__((packed));
int idt_add_entry(uint8_t vector, uint64_t isr, uint8_t flags, uint8_t ist_entry);
int idt_get_entry(uint8_t vector, struct idt_entry_t** ppEntry);
int idt_get_free_vector(uint8_t* pVector);
int idt_init(void);
int load_idt(struct idt_ptr_t* pidt);
#endif
