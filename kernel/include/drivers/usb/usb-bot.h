#ifndef _USB_BOT_DRIVER
#define _USB_BOT_DRIVER
struct usb_bot_drive_desc{
	
};
int usb_bot_driver_init(void);
int usb_bot_interface_register(uint8_t port, uint8_t interfaceId);
int usb_bot_interface_unregister(uint8_t port, uint8_t interfaceId);
#endif
