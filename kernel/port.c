#include <stdint.h>
void outb(uint16_t port, uint8_t value){
	__asm__ volatile("outb %0, %1" :: "a"(value),"Nd"(port));
	return;	
}
uint8_t inb(uint16_t port){
	uint8_t val = 0;
	__asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}
void outw(uint16_t port, uint16_t value){
	__asm__ volatile("outw %0, %1" :: "a"(value),"Nd"(port));
	return;	
}
uint16_t inw(uint16_t port){
	uint16_t val = 0;
	__asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}
void outl(uint16_t port, uint32_t value){
	__asm__ volatile("outl %0, %1" :: "a"(value),"Nd"(port));
	return;	
}
uint32_t inl(uint16_t port){
	uint32_t val = 0;
	__asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}
