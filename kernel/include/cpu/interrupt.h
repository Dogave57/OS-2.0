#ifndef _INTERRUPT
#define _INTERRUPT
#include "idt.h"
#define SAFE_STACK_SIZE 8192
extern unsigned char safe_stack[SAFE_STACK_SIZE];
extern unsigned char* pSafeStackTop;
struct cpu_exception_entry{
	uint64_t pIsr;
	uint64_t istVector;
	uint64_t idtVector;
};
void default_isr(void);
void pic_timer_isr(void);
void timer_isr(void);
void thermal_isr(void);
void ps2_kbd_isr(void);
void isr0(void);
void isr1(void);
void isr2(void);
void isr3(void);
void isr4(void);
void isr5(void);
void isr6(void);
void isr7(void);
void isr8(void);
void isr10(void);
void isr11(void);
void isr12(void);
void isr13(void);
void isr14(void);
void isr16(void);
void isr17(void);
void isr18(void);
void isr19(void);
void isr20(void);
#endif
