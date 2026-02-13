#ifndef _SUBSYSTEM_USB
#define _SUBSYSTEM_USB
#define USB_SUBSYSTEM_MAX_DRIVER_COUNT (4096)
typedef int(*usbDriverRegisterInterface)(uint8_t port, uint8_t interfaceId);
typedef int(*usbDriverUnregisterInterface)(uint8_t port, uint8_t interfaceId);
struct usb_driver_vtable{
	usbDriverRegisterInterface registerInterface;
	usbDriverUnregisterInterface unregisterInterface;
};
struct usb_driver_desc{
	struct usb_driver_vtable vtable;
	uint64_t driverId;
	uint8_t interfaceClass;
	uint8_t interfaceSubClass;
	struct usb_driver_desc* pFlink;
	struct usb_driver_desc* pBlink;
};
int usb_subsystem_init(void);
int usb_driver_register(struct usb_driver_vtable vtable, uint8_t interfaceClass, uint8_t interfaceSubClass, uint64_t* pDriverId);
int usb_driver_unregister(uint64_t driverId);
int usb_get_driver_desc(uint64_t driverId, struct usb_driver_desc** ppDriverDesc);
int usb_get_first_driver_desc(struct usb_driver_desc** ppFirstDriverDesc);
int usb_get_last_driver_desc(struct usb_driver_desc** ppLaStDriverDesc);
int usb_get_next_driver_desc(struct usb_driver_desc* pCurrentDriverDesc, struct usb_driver_desc** ppNextDriverDesc);
int usb_register_interface(uint8_t port, uint8_t interfaceId, uint64_t driverId);
int usb_unregister_interface(uint8_t port, uint8_t interfaceId, uint64_t driverId);
#endif

