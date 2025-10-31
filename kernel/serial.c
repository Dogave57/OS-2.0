#include "port.h"
#include "serial.h"
static unsigned int serial_portmap[]={
	[0] = 0x3F8,
	[1] = 0x2F8,
	[2] = 0x3E8,
	[3] = 0x2E8,
	[4] = 0x5F8,
	[5] = 0x4F8,
	[6] = 0x5E8,
	[7] = 0x4E8,
};
unsigned int isInitialized = 0;
int serial_init_port(unsigned int port){
	unsigned int serial = serial_portmap[port];
	outb(port+1, 0x00);
	outb(0x00, 0x00);
	outb(port+3, 0x80);
	outb(0x00, 0x00);
	outb(port, 0x03);
	outb(0x00, 0x00);
	outb(port+1, 0x00);
	outb(0x00, 0x00);
	outb(port+3, 0x03);
	outb(0x00, 0x00);
	outb(port+2, 0xC7);
	outb(0x00, 0x00);
	outb(port+4, 0x0B);
	outb(0x00, 0x00);
	outb(port+4, 0x1E);
	outb(0x00, 0x00);
	outb(port, 0xAE);
	outb(0x00, 0x00);
	return 0;
}
int serial_init(void){
	for (unsigned int i = 0;i<7;i++){
		serial_init_port(i);
	}
	isInitialized = 1;
	return 0;
}
unsigned int serial_initialized(void){
	return isInitialized;
}
int serial_putchar(unsigned int port, unsigned char ch){
	if (!isInitialized)
		return -1;
	unsigned int serial = serial_portmap[port];
	while (!(inb(port+5)&0x20)){};
	outb(serial, ch);
	return 0;
}
int serial_print(unsigned int port, CHAR16* msg){
	if (!msg)
		return -1;
	if (port>7)
		return -1;
	for (unsigned int i = 0;msg[i];i++){
		unsigned char ch = (unsigned char)msg[i];
		serial_putchar(port, ch);
	}
	return 0;
}
