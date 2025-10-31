#include "port.h"
#include "bootloader.h"
#include "graphics.h"
#include "keyboard.h"
unsigned int keysPressed[256]={0};
void ps2_keyboard_handler(void){
	if (!inb(0x64)&1)
		return;
	unsigned char scancode = inb(0x60);
	if (!scancode)
		return;
	unsigned short mapping = (unsigned short)scantoascii[scancode];
	switch (scancode){
		case 0x1D:
		keysPressed[KEY_LCTRL] = 1;
		break;
		case 0x9D:
		keysPressed[KEY_LCTRL] = 0;
		break;
		case 0x2A:
		keysPressed[KEY_LSHIFT] = 1;
		break;
		case 0x36:
		keysPressed[KEY_RSHIFT] = 1;
		break;
		case 0xAA:
		keysPressed[KEY_LSHIFT] = 0;
		break;
		case 0xB6:
		keysPressed[KEY_RSHIFT] = 0;
		break;
		case 0x01:
		keysPressed[KEY_ESC] = 1;
		break;
		case 0x81:
		keysPressed[KEY_ESC] = 0;
		break;
		default:
		break;
	}
	if (!mapping)
		return;
	putchar(mapping);
	return;
}
