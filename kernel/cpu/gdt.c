#include "bootloader.h"
#include "mem/vmm.h"
#include "stdlib/stdlib.h"
#include "cpu/interrupt.h"
#include "cpu/gdt.h"
struct gdt_entry_t gdt[GDT_ENTRIES] = {0};
__attribute__((aligned(16))) struct tss_entry_t tss = {0};
struct gdt_ptr pgdt = {0};
unsigned char safe_stack[SAFE_STACK_SIZE] = {0};
unsigned char* pSafeStackTop = (unsigned char*)(safe_stack+SAFE_STACK_SIZE);
int gdt_add_entry(struct gdt_entry_t* pTarget, uint32_t base, uint32_t limit, uint8_t access, uint16_t flags){
	if (limit>0xFFFFF)
		return -1;
	pTarget->limit_low = (uint16_t)(limit&0xFFFF);
	pTarget->base_low = (uint16_t)(base&0xFFFF);
	pTarget->base_mid = (uint8_t)((base>>16)&0xFF);
	pTarget->base_high = (uint8_t)((base>>24)&0xFF);
	pTarget->access = access;
	pTarget->flags_high_limit = (uint8_t)((limit>>16)&0xF);
	pTarget->flags_high_limit |= (flags<<4);
	return 0;
}
int gdt_add_long_entry(struct gdt_entry_long_t* pTarget, uint64_t base, uint32_t limit, uint8_t access, uint16_t flags){
	if (!pTarget)
		return -1;
	uint64_t* pLowEntry = (uint64_t*)&pTarget->lowEntry;
	gdt_add_entry((struct gdt_entry_t*)pTarget, (uint32_t)(base&0xFFFFFFFF), limit, access, flags);
	pTarget->base_high = (uint32_t)((base>>32)&0xFFFFFFFF);	
	pTarget->reserved0 = 0;
	return 0;
}
int gdt_init(void){
	memset((void*)&tss, 0, sizeof(struct tss_entry_t));
	tss.rsp0 = (uint64_t)(safe_stack+SAFE_STACK_SIZE);
	tss.ist[0] = (uint64_t)(safe_stack+SAFE_STACK_SIZE);
	tss.iomap_base = sizeof(struct tss_entry_t);
	gdt_add_entry(gdt, 0, 0x0, 0, 0);
	gdt_add_entry(gdt+1, 0, 0x0, 0x9A, 0xA);
	gdt_add_entry(gdt+2, 0, 0x0, 0x92, 0xC);
	gdt_add_long_entry((struct gdt_entry_long_t*)(gdt+3), (uint64_t)&tss, sizeof(struct tss_entry_t)-1, 0x89, 0x0);
	pgdt.limit = (sizeof(struct gdt_entry_t)*GDT_ENTRIES)-1;
	pgdt.base = (uint64_t)gdt;
	load_gdt(&pgdt);
	return 0;
}
