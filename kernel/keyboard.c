#include "port.h"
#include "bootloader.h"
#include "graphics.h"
#include "keyboard.h"
void ps2_keyboard_handler(void){
	if (!inb(0x64)&1)
		return;
	unsigned char scancode = inb(0x60);
	if (!scancode)
		return;
	unsigned short mapping = (unsigned short)scantoascii[scancode];
	if (!mapping)
		return;
	if (mapping=='\n')
		putchar('\r');
	putchar(mapping);	
	return;
}
