#include "mem/pmm.h"
#include "mem/vmm.h"
#include "stdlib/stdlib.h"
#include "subsystem/usb.h"
#include "cpu/mutex.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "drivers/usb/xhci.h"
#include "drivers/usb/usb-kbd.h"
struct hid_boot_kbd_report bootKbdReport = {0};
struct hid_boot_kbd_report lastBootKbdReport = {0};
static uint64_t driverId = 0;
int usb_kbd_driver_init(void){
	struct usb_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct usb_driver_vtable));
	vtable.registerInterface = usb_kbd_register;
	vtable.unregisterInterface = usb_kbd_unregister;
	if (usb_driver_register(vtable, 3, 1, &driverId)!=0)
		return -1;
	return 0;
}
int usb_kbd_init(uint8_t port, uint8_t interfaceId){
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(port, &pDevice)!=0)
		return -1;
	struct xhci_interface_desc* pInterfaceDesc = (struct xhci_interface_desc*)0x0;
	if (xhci_get_interface_desc(pDevice, interfaceId, &pInterfaceDesc)!=0)
		return -1;
	if (pInterfaceDesc->usbInterfaceDesc.interfaceClass!=0x3||pInterfaceDesc->usbInterfaceDesc.interfaceSubClass!=0x1||pInterfaceDesc->usbInterfaceDesc.interfaceProtocol!=0x1){
		printf("not a valid USB HID keyboard\r\n");
		return -1;
	}
	struct xhci_endpoint_desc* pEndpointDesc = (struct xhci_endpoint_desc*)0x0;
	uint64_t endpointId = 0;
	uint64_t endpointIndex = 0;
	for (uint8_t i = 0;i<pInterfaceDesc->endpointCount;i++){
		struct xhci_endpoint_desc* pCurrentEndpointDesc = pInterfaceDesc->pEndpointDescList+i;
		if (pCurrentEndpointDesc->pEndpointContext->type!=XHCI_ENDPOINT_TYPE_INT_IN){
			continue;
		}
		pEndpointDesc = pCurrentEndpointDesc;
		endpointIndex = pCurrentEndpointDesc->endpointIndex;
		endpointId = i;
		break;
	}	
	if (!pEndpointDesc||!endpointIndex)
		return -1;
	struct xhci_trb eventTrb = {0};
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	if (xhci_set_protocol(pDevice, pDevice->pControlTransferRingInfo, interfaceId, 0, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to set protocol (%s)\r\n", errorName);
		return -1;
	}	
	if (usb_kbd_request_boot_report(pDevice, interfaceId, endpointId)!=0){
		printf("failed to request boot report\r\n");
		return -1;
	}
	return 0;
}
int usb_kbd_deinit(uint8_t port, uint8_t interfaceId){
	
	return 0;
}
int usb_kbd_interrupt(struct xhci_transfer_desc* pTransferDesc){
	if (!pTransferDesc)
		return -1;
	struct xhci_trb eventTrb = pTransferDesc->eventTrb;
	usb_kbd_handle_boot_report();
	struct xhci_transfer_ring_info* pTransferRingInfo = pTransferDesc->pTransferRingInfo;
	if (!pTransferRingInfo){
		printf("invalid transfer ring info\r\n");
		return -1;
	}
	if (usb_kbd_request_boot_report(pTransferRingInfo->pDevice, pTransferRingInfo->interfaceId, pTransferRingInfo->endpointId)!=0)
		return -1;
	return 0;
}
int usb_kbd_handle_boot_report(void){
	if (bootKbdReport.modifiers.left_shift)
		key_register_press(KEY_LSHIFT);
	else
		key_register_release(KEY_LSHIFT);
	if (bootKbdReport.modifiers.right_shift)
		key_register_press(KEY_RSHIFT);
	else
		key_register_release(KEY_RSHIFT);
	if (bootKbdReport.modifiers.left_ctrl)
		key_register_press(KEY_LCTRL);
	else
		key_register_release(KEY_LCTRL);
	if (bootKbdReport.modifiers.right_ctrl)
		key_register_press(KEY_RCTRL);
	else
		key_register_release(KEY_RCTRL);
	if (bootKbdReport.modifiers.left_alt)
		key_register_press(KEY_LALT);
	else
		key_register_release(KEY_LALT);
	if (bootKbdReport.modifiers.right_alt)
		key_register_press(KEY_RALT);
	else
		key_register_release(KEY_RALT);
	uint8_t spam = 0;
	for (uint64_t i = 0;i<sizeof(bootKbdReport.keys);i++){
		if (bootKbdReport.keys[i]<4)
			continue;
		uint8_t keyCode = bootKbdReport.keys[i];
		uint8_t character = hid_keymap[keyCode];
		uint8_t lastPressed = 0;
		uint64_t pressTime = 0;
		key_get_press_time_us(character, &pressTime);
		uint8_t spamAllowed = !(pressTime<1000000)&&!spam;
		for (uint64_t j = 0;j<sizeof(lastBootKbdReport.keys)&&!spamAllowed;j++){
			if (lastBootKbdReport.keys[j]!=keyCode)
				continue;
			lastPressed = 1;
			break;
		}
		if (lastPressed){
			continue;
		}
		if (character<128&&character)
			putchar((!key_pressed(KEY_LSHIFT)||!key_pressed(KEY_RSHIFT)||!key_pressed(KEY_CAPSLOCK)) ? toUpper(character) : character);
		if (spamAllowed)
			spam = 1;
		if (character)
			key_register_press(character);
	}
	for (uint64_t i = 0;i<sizeof(lastBootKbdReport.keys);i++){
		uint64_t pressed = 0;
		uint8_t keyCode = lastBootKbdReport.keys[i];
		unsigned char character = hid_keymap[keyCode];
		for (uint64_t j = 0;j<sizeof(bootKbdReport.keys);j++){
			if (bootKbdReport.keys[j]!=keyCode)
				continue;
			pressed = 1;
			break;
		}
		if (pressed)
			continue;
		if (keyCode==HID_KEYCODE_CAPSLOCK)
			key_register_release(KEY_CAPSLOCK);
		if (character)
			key_register_release(character);
	}
	return 0;
}
int usb_kbd_request_boot_report(struct xhci_device* pDevice, uint8_t interfaceId, uint8_t endpointId){
	lastBootKbdReport = bootKbdReport;
	struct xhci_trb eventTrb = {0};
	struct xhci_usb_packet_request packetRequest = {0};
	memset((void*)&packetRequest, 0, sizeof(struct xhci_usb_packet_request));
	packetRequest.interfaceId = interfaceId;
	packetRequest.endpointId = endpointId;
	packetRequest.pBuffer = (unsigned char*)&bootKbdReport;
	packetRequest.bufferSize = sizeof(struct hid_boot_kbd_report);
	packetRequest.completionFunc = usb_kbd_interrupt;
	if (xhci_send_usb_packet(pDevice, packetRequest, &eventTrb)!=0){
		printf("failed to send packet\r\n");
		return -1;
	}
	return 0;
}
int usb_kbd_register(uint8_t port, uint8_t interfaceId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	if (usb_kbd_init(port, interfaceId)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_kbd_unregister(uint8_t port, uint8_t interfaceId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	if (usb_kbd_deinit(port, interfaceId)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;	
}
