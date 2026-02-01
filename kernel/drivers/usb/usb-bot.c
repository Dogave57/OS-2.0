#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "stdlib/stdlib.h"
#include "subsystem/usb.h"
#include "drivers/usb/xhci.h"
#include "drivers/usb/usb-bot.h"
static uint64_t driverId = 0;
int usb_bot_driver_init(void){
	struct usb_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct usb_driver_vtable));
	vtable.registerInterface = usb_bot_interface_register;
	vtable.unregisterInterface = usb_bot_interface_unregister;
	if (usb_driver_register(vtable, 0x08, 0x06, &driverId)!=0){
		printf("failed to register USB BOT interface protocol driver\r\n");
		return -1;
	}
	return 0;
}
int usb_bot_interface_register(uint8_t port, uint8_t interfaceId){
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(port, &pDevice)!=0)
		return -1;
	struct xhci_interface_desc* pInterfaceDesc = pDevice->pInterfaceDescList+interfaceId;
	if (pInterfaceDesc->usbInterfaceDesc.interfaceProtocol!=0x50){
		printf("USB mass storage device does not follow BOT protocol!\r\n");
		return -1;
	}
	printf("successfully registered USB mass storage interface following BOT protocol at port %d interface %d\r\n", port, interfaceId);
	return 0;
}
int usb_bot_interface_unregister(uint8_t port, uint8_t interfaceId){

	return 0;
}
