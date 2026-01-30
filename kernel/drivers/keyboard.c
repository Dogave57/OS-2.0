#include "cpu/port.h"
#include "bootloader.h"
#include "stdlib/stdlib.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
unsigned int keysPressed[256]={0};
static struct keyboard_context keyboardContext = {0};
void ps2_keyboard_handler(void){
	if (!inb(0x64)&1)
		return;
	unsigned char scancode = inb(0x60);
	if (!scancode)
		return;
	unsigned char mapping = scantoascii[scancode];	
	switch (scancode){
		case 0x1D:
		key_register_press(KEY_LCTRL);
		break;
		case 0x9D:
		key_register_release(KEY_LCTRL);
		break;
		case 0x2A:
		key_register_press(KEY_LSHIFT);
		break;
		case 0x36:
		key_register_press(KEY_RSHIFT);
		break;
		case 0xAA:
		key_register_release(KEY_LSHIFT);
		break;
		case 0xB6:
		key_register_release(KEY_RSHIFT);	
		break;
		case 0x01:
		keysPressed[KEY_ESC] = 1;
		key_register_press(KEY_ESC);
		break;
		case 0x81:
		key_register_release(KEY_ESC);
		break;
		default:
		if (scancode<0x80)
			break;
		unsigned char releaseMapping = scantoascii[scancode-0x80];
		if (!releaseMapping)
			break;
		key_register_release(releaseMapping);	
		break;
	}
	if (!mapping)
		return;
	if (keysPressed[KEY_LCTRL]&&mapping=='c'){
		printf("\r\nSYSTEM HAULT!\r\n");
		__asm__ volatile("cli");
		__asm__ volatile("hlt");
		while (1){};
		return;
	}
	if (keysPressed[KEY_LSHIFT]&&keysPressed[KEY_ESC]&&mapping=='d'){
		__asm__ volatile("int $1");
		__asm__ volatile("hlt");
		while (1){};
		return;
	}
	if (keysPressed[KEY_LSHIFT]||keysPressed[KEY_RSHIFT])
		mapping = toUpper(mapping);
	key_register_press(mapping);	
	putchar(mapping);
	return;
}
int key_register_press(uint8_t key){
	struct key_desc* pKeyDesc = (struct key_desc*)0x0;
	if (key_get_desc(key, &pKeyDesc)!=0)
		return -1;
	if (pKeyDesc->isPressed)
		return 0;
	pKeyDesc->isPressed = 1;
	pKeyDesc->time_us = get_time_us();
	return 0;
}
int key_register_release(uint8_t key){
	struct key_desc* pKeyDesc = (struct key_desc*)0x0;
	if (key_get_desc(key, &pKeyDesc)!=0)
		return -1;
	if (!pKeyDesc->isPressed)
		return 0;
	pKeyDesc->isPressed = 0;
	return 0;
}
int key_pressed(uint8_t key){
	struct key_desc* pKeyDesc = (struct key_desc*)0x0;
	if (key_get_desc(key, &pKeyDesc)!=0)
		return -1;
	return (pKeyDesc->isPressed) ? 0 : -1;
}
int key_get_press_time_us(uint8_t key, uint64_t* pTime){
	if (!pTime)
		return -1;
	struct key_desc* pKeyDesc = (struct key_desc*)0x0;
	if (key_get_desc(key, &pKeyDesc)!=0)
		return -1;
	uint64_t elapsedTime = get_time_us()-pKeyDesc->time_us;
	*pTime = elapsedTime;	
	return 0;
}
int key_get_desc(uint8_t key, struct key_desc** ppKeyDesc){
	if (!ppKeyDesc)
		return -1;
	struct key_desc* pKeyDesc = keyboardContext.keyList+key;
	*ppKeyDesc = pKeyDesc;
	return 0;
}
