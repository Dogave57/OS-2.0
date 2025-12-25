#ifndef _GDT
#define _GDT
#include <stdint.h>
#define GDT_ENTRIES 3
struct gdt_entry_t{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t flags;
	uint8_t base_high;
}__attribute__((packed));
struct gdt_ptr{
	uint16_t limit;
	uint64_t base;
}__attribute__((packed));
struct tss_entry_t{
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist[7];
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomap_base;
}__attribute__((packed));
int gdt_add_entry(struct gdt_entry_t* ptarget, uint32_t base, uint32_t limit, uint8_t access, uint16_t flags);
int gdt_init(void);
int load_gdt(struct gdt_ptr* pgdt);
#endif
