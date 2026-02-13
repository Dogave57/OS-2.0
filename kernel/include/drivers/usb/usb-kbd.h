#ifndef _USB_KBD
#define _USB_KBD
#include <stdint.h>
#include "kernel_include.h"
#include "drivers/usb/xhci.h"
#define HID_KEYCODE_CAPSLOCK (0x39)
static const unsigned char hid_keymap[255]={
	[0x04]='a',
	[0x05]='b',
	[0x06]='c',
	[0x07]='d',
	[0x08]='e',
	[0x09]='f',
	[0x0A]='g',
	[0x0B]='h',
	[0x0C]='i',
	[0x0D]='j',
	[0x0E]='k',
	[0x0F]='l',
	[0x10]='m',
	[0x11]='n',
	[0x12]='o',
	[0x13]='p',
	[0x14]='q',
	[0x15]='r',
	[0x16]='s',
	[0x17]='t',
	[0x18]='u',
	[0x19]='v',
	[0x1A]='w',
	[0x1B]='x',
	[0x1C]='y',
	[0x1D]='z',
	[0x1E]='1',
	[0x1F]='2',
	[0x20]='3',
	[0x21]='4',
	[0x22]='5',
	[0x23]='6',
	[0x24]='7',
	[0x25]='8',
	[0x26]='9',
	[0x27]='0',
	[0x2C]=' ',
	[0x2A]='\b',//BACKSPACEi
	[0x2C]=' ',
	[0x2D]='-',
	[0x2E]='=',
	[0x2F]='[',
	[0x30]=']',
	[0x31]='\\',
	[0x33]=';',
	[0x34]='\'',
	[0x35]='`',
	[0x36]=',',
	[0x37]='.',
	[0x38]='/',
	[0x3A]=0,//F1
	[0x3B]=0,//F2
	[0x3C]=0,//F3
	[0x3D]=0,//F4
	[0x3E]=0,//F5
	[0x3F]=0,//F6
	[0x40]=0,//F7
	[0x41]=0,//F8
	[0x42]=0,//F9
	[0x43]=0,//F10
	[0x44]=0,//F11
	[0x45]=0,//F12
	[0x28]='\n',
	[0x29]=0,//ESC
	[0x2B]='\t',//TAB
	[0x4F]=0,//RIGHT ARROW
	[0x50]=0,//LEFT ARROW
	[0x51]=0,//DOWN ARROW
	[0x52]=0,//UP ARROW
	[0x49]=0,//INSERT
	[0x4A]=0,//HOME
	[0x4B]=0,//PAGE UP
	[0x4C]='\b',//DELETE
};
struct hid_boot_kbd_modifiers{
	uint8_t left_ctrl:1;
	uint8_t left_shift:1;
	uint8_t left_alt:1;
	uint8_t left_gui:1;
	uint8_t right_ctrl:1;
	uint8_t right_shift:1;
	uint8_t right_alt:1;
	uint8_t right_gui:1;
}__attribute__((packed));
struct hid_boot_kbd_report{
	struct hid_boot_kbd_modifiers modifiers;	
	uint8_t reserved0;
	uint8_t keys[6];
}__attribute__((packed));
int usb_kbd_driver_init(void);
int usb_kbd_init(uint8_t port, uint8_t interfaceId);
int usb_kbd_deinit(uint8_t port, uint8_t interfaceId);
int usb_kbd_interrupt(struct xhci_transfer_desc* pTransferDesc);
int usb_kbd_handle_boot_report(void);
int usb_kbd_request_boot_report(struct xhci_device* pDevice, uint8_t interfaceId, uint8_t endpointId);
int usb_kbd_register(uint8_t port, uint8_t interfaceId);
int usb_kbd_unregister(uint8_t port, uint8_t interfaceId);
#endif
