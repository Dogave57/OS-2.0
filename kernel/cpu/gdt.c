#include "bootloader.h"
#include "mem/vmm.h"
#include "drivers/graphics.h"
#include "stdlib/stdlib.h"
#include "cpu/interrupt.h"
#include "cpu/gdt.h"
struct gdt_entry_t gdt[GDT_ENTRIES] = {0};
struct tss_entry_t tss = {0};
struct gdt_ptr pgdt = {0};
int gdt_add_entry(struct gdt_entry_t* ptarget, uint32_t base, uint32_t limit, uint8_t access, uint16_t flags){
	if (limit>0xFFFFF)
		return -1;
	uint64_t entry = 0;
	entry = (limit&0xFFFF);
	entry |= (base&0xFFFFF)<<16;
	entry|=((uint64_t)access)<<40;
	entry|=(uint64_t)((limit>>16)&0xF)<<48;
	entry|=((uint64_t)(flags&0xF))<<52;
	entry|= ((uint64_t)(base>>24)&0xFF)<<56;
	*((uint64_t*)ptarget) = entry;
	return 0;
}
int gdt_init(void){
	tss.ist[0] = safe_stack_top;	
	tss.iomap_base = sizeof(struct tss_entry_t);
	gdt_add_entry(gdt, 0, 0x0, 0, 0);
	gdt_add_entry(gdt+1, 0, 0x0, 0x9A, 0xA);
	gdt_add_entry(gdt+2, 0, 0x0, 0x92, 0xC);
//	gdt_add_entry(gdt+3, (uint32_t)(&tss), sizeof(struct tss_entry_t)-1, 0x89, 0x0);
	pgdt.limit = (sizeof(struct gdt_entry_t)*GDT_ENTRIES)-1;
	pgdt.base = (uint64_t)gdt;
	load_gdt(&pgdt);
	return 0;
}
