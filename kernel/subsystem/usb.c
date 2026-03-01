#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/subsystem.h"
#include "cpu/mutex.h"
#include "stdlib/stdlib.h"
#include "drivers/usb/xhci.h"
#include "subsystem/usb.h"
static struct subsystem_desc* pDriverSubsystemDesc = (struct subsystem_desc*)0x0;
struct usb_driver_desc* pLastDriverDesc = (struct usb_driver_desc*)0x0;
struct usb_driver_desc* pFirstDriverDesc = (struct usb_driver_desc*)0x0;
int usb_subsystem_init(void){
	if (subsystem_init(&pDriverSubsystemDesc, USB_SUBSYSTEM_MAX_DRIVER_COUNT)!=0){
		printf("failed to initialize USB driver subsystem\r\n");
		return -1;
	}
	return 0;
}
int usb_driver_register(struct usb_driver_vtable vtable, uint8_t interfaceClass, uint8_t interfaceSubClass, uint64_t* pDriverId){
	if (!pDriverId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct usb_driver_desc* pDriverDesc = (struct usb_driver_desc*)kmalloc(sizeof(struct usb_driver_desc));
	if (!pDriverDesc){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	memset((void*)pDriverDesc, 0, sizeof(struct usb_driver_desc));
	pDriverDesc->vtable = vtable;
	pDriverDesc->interfaceClass = interfaceClass;
	pDriverDesc->interfaceSubClass = interfaceSubClass;
	if (!pFirstDriverDesc){
		pFirstDriverDesc = pDriverDesc;
	}
	if (pLastDriverDesc){
		pLastDriverDesc->pFlink = pDriverDesc;
	}
	pDriverDesc->pBlink = pLastDriverDesc;
	pLastDriverDesc = pDriverDesc;
	uint64_t driverId = 0;
	if (subsystem_alloc_entry(pDriverSubsystemDesc, (unsigned char*)pDriverDesc, &driverId)!=0){
		printf("failed to allocate subsystem entry\r\n");
		kfree((void*)pDriverDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	pDriverDesc->driverId = driverId;
	struct xhci_info xhciInfo = {0};
	if (xhci_get_info(&xhciInfo)!=0){
		printf("failed to get XHC info\r\n");
		kfree((void*)pDriverDesc);
		subsystem_free_entry(pDriverSubsystemDesc, driverId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct xhci_device* pCurrentDevice = xhciInfo.pFirstDevice;
	uint64_t resolvedInterfaceCount = 0;
	while (pCurrentDevice){
		if (!pCurrentDevice->deviceInitialized){
			pCurrentDevice = pCurrentDevice->pFlink;
			continue;
		}	
		for (uint64_t i = 0;i<pCurrentDevice->interfaceCount&&pCurrentDevice->unresolvedInterfaceCount;i++){
			struct xhci_interface_desc* pCurrentInterfaceDesc = pCurrentDevice->pInterfaceDescList+i;
			if (pCurrentInterfaceDesc->driverId!=0xFFFFFFFFFFFFFFFF){
				continue;
			}
			if (pCurrentInterfaceDesc->usbInterfaceDesc.interfaceClass!=interfaceClass||pCurrentInterfaceDesc->usbInterfaceDesc.interfaceSubClass!=interfaceSubClass){
				continue;
			}
			if (pDriverDesc->vtable.registerInterface(pCurrentDevice->port, i)!=0){
				continue;
			}
			pCurrentDevice->unresolvedInterfaceCount--;
		}	
		pCurrentDevice = pCurrentDevice->pFlink;
	}
	*pDriverId = driverId;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_driver_unregister(uint64_t driverId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);	
	struct usb_driver_desc* pDriverDesc = (struct usb_driver_desc*)0x0;
	if (subsystem_get_entry(pDriverSubsystemDesc, driverId, (uint64_t*)&pDriverDesc)!=0){
		printf("failed to get subsystem entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (pDriverDesc->pBlink)
		pDriverDesc->pBlink->pFlink = pDriverDesc->pFlink;
	if (pDriverDesc->pFlink)
		pDriverDesc->pFlink->pBlink = pDriverDesc->pBlink;
	if (pDriverDesc==pFirstDriverDesc)
		pFirstDriverDesc = pDriverDesc->pFlink;
	if (pDriverDesc==pLastDriverDesc)
		pLastDriverDesc = pDriverDesc->pBlink;
	kfree((void*)pDriverDesc);
	if (subsystem_free_entry(pDriverSubsystemDesc, driverId)!=0){
		printf("failed to free entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int usb_get_driver_desc(uint64_t driverId, struct usb_driver_desc** ppDriverDesc){
	if (!ppDriverDesc)
		return -1;
	struct usb_driver_desc* pDriverDesc = (struct usb_driver_desc*)0x0;
	if (subsystem_get_entry(pDriverSubsystemDesc, driverId, (uint64_t*)&pDriverDesc)!=0){
		printf("failed to get subsystem entry\r\n");
		return -1;
	}
	*ppDriverDesc = pDriverDesc;
	return 0;
}
int usb_get_first_driver_desc(struct usb_driver_desc** ppFirstDriverDesc){
	if (!ppFirstDriverDesc)
		return -1;
	*ppFirstDriverDesc = pFirstDriverDesc;	
	return 0;
}
int usb_get_last_driver_desc(struct usb_driver_desc** ppLastDriverDesc){
	if (!ppLastDriverDesc)
		return -1;
	*ppLastDriverDesc = pLastDriverDesc;
	return 0;
}
int usb_get_next_driver_desc(struct usb_driver_desc* pCurrentDriverDesc, struct usb_driver_desc** ppNextDriverDesc){
	if (!ppNextDriverDesc||!pFirstDriverDesc)
		return -1;
	if (!pCurrentDriverDesc){
		*ppNextDriverDesc = pFirstDriverDesc;
		return 0;
	}
	struct usb_driver_desc* pNextDriverDesc = pCurrentDriverDesc->pFlink;
	if (!pNextDriverDesc)
		return -1;
	*ppNextDriverDesc = pNextDriverDesc;
	return 0;
}
int usb_register_interface(uint8_t port, uint8_t interfaceId, uint64_t driverId){
	struct usb_driver_desc* pDriverDesc = (struct usb_driver_desc*)0x0;
	if (usb_get_driver_desc(driverId, &pDriverDesc)!=0)
		return -1;	
	if (pDriverDesc->vtable.registerInterface(port, interfaceId)!=0)
		return -1;
	return 0;
}
int usb_unregister_interface(uint8_t port, uint8_t interfaceId, uint64_t driverId){
	struct usb_driver_desc* pDriverDesc = (struct usb_driver_desc*)0x0;
	if (usb_get_driver_desc(driverId, &pDriverDesc)!=0)
		return -1;
	if (pDriverDesc->vtable.unregisterInterface(port, interfaceId)!=0)
		return -1;	
	return 0;
}
