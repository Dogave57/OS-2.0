#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/mutex.h"
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
int usb_bot_send_packet(struct usb_bot_drive_desc* pDesc, struct usb_bot_packet_request packetRequest, struct xhci_trb* pEventTrb){
	if (!pDesc||!pEventTrb)
		return -1;
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(pDesc->port, &pDevice)!=0)
		return -1;
	struct xhci_usb_packet_request usbPacketRequest = {0};
	memset((void*)&usbPacketRequest, 0, sizeof(struct xhci_usb_packet_request));
	usbPacketRequest.interfaceId = pDesc->interfaceId;
	usbPacketRequest.endpointId = pDesc->outputEndpointId;
	usbPacketRequest.pBuffer = (unsigned char*)&packetRequest.cmd;
	usbPacketRequest.bufferSize = 31;
	struct xhci_trb eventTrb = {0};
	printf("sending setup packet\r\n");
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to send cmd packet to USB device (%s)\r\n", errorName);
		*pEventTrb = eventTrb;
		return -1;
	}	
	printf("done\r\n");
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	usbPacketRequest.endpointId = pDesc->inputEndpointId;
	usbPacketRequest.pBuffer = packetRequest.pBuffer;
	usbPacketRequest.bufferSize = packetRequest.bufferSize;
	printf("sending data packet\r\n");
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to send data packet to USB device (%s)\r\n", errorName);
		*pEventTrb = eventTrb;
		return -1;
	}
	printf("done\r\n");
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	usbPacketRequest.pBuffer = (unsigned char*)packetRequest.pCmdStatus;	
	usbPacketRequest.bufferSize = 13;
	printf("sending status packet\r\n");
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";	
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to send status packet to USB device (%s)\r\n", errorName);
		*pEventTrb = eventTrb;
		return -1;
	}
	printf("done\r\n");
	*pEventTrb = eventTrb;
	return 0;
}
int usb_bot_get_info(struct usb_bot_drive_desc* pDesc, struct usb_bot_info* pDriveInfo){
	if (!pDesc||!pDriveInfo)
		return -1;
	struct usb_bot_info driveInfo = {0};
	struct usb_bot_cmd_status cmdStatus = {0};
	memset((void*)&cmdStatus, 0, sizeof(struct usb_bot_cmd_status));	
	struct usb_bot_packet_request packetRequest = {0};
	memset((void*)&packetRequest, 0, sizeof(struct usb_bot_packet_request));	
	struct usb_bot_get_drive_info_cmd* pDriveInfoCmd = (struct usb_bot_get_drive_info_cmd*)packetRequest.cmd.cmd_block;
	pDriveInfoCmd->opcode = BOT_OPCODE_GET_INFO;
	pDriveInfoCmd->alloc_len = sizeof(struct usb_bot_info);
	pDriveInfoCmd->alloc_len = 36;
	packetRequest.cmd.signature = BOT_CMD_SIGNATURE;
	packetRequest.cmd.cmd_id = 1;
	packetRequest.cmd.data_transfer_len = 36;
	packetRequest.cmd.flags = (1<<7);
	packetRequest.cmd.cmd_block_len = sizeof(struct usb_bot_get_drive_info_cmd);
	packetRequest.cmd.cmd_block_len = 0x06;
	packetRequest.pCmdStatus = &cmdStatus;
	packetRequest.pBuffer = (unsigned char*)&driveInfo;
	packetRequest.bufferSize = sizeof(struct usb_bot_info);
	struct xhci_trb eventTrb = {0};
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	if (usb_bot_send_packet(pDesc, packetRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to send BOT get drive info packet (%s)\r\n", errorName);
		return -1;
	}
	*pDriveInfo = driveInfo;
	return 0;
}
int usb_bot_interface_register(uint8_t port, uint8_t interfaceId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(port, &pDevice)!=0){
		mutex_unlock(&mutex);
		return -1;		
	}
	struct xhci_interface_desc* pInterfaceDesc = pDevice->pInterfaceDescList+interfaceId;
	if (pInterfaceDesc->usbInterfaceDesc.interfaceProtocol!=0x50){
		printf("USB mass storage device does not follow BOT protocol!\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	uint64_t driveId = 0;
	if (drive_register(driverId, port, &driveId)!=0){
		printf("failed to register USB interface following BOT at port %d interface %d\r\n", port, interfaceId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct drive_desc* pDriveDesc = (struct drive_desc*)0x0;
	if (drive_get_desc(driveId, &pDriveDesc)!=0){
		printf("failed to get drive desc\r\n");
		drive_unregister(driveId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	printf("successfully registered USB mass storage interface following BOT protocol at port %d interface %d\r\n", port, interfaceId);
	uint64_t inputEndpointId = 0xFFFFFFFFFFFFFFFF;
	uint64_t outputEndpointId = 0xFFFFFFFFFFFFFFFF;
	struct xhci_trb eventTrb = {0};
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	for (uint64_t i = 0;i<pInterfaceDesc->endpointCount;i++){
		struct xhci_endpoint_desc* pCurrentEndpointDesc = pInterfaceDesc->pEndpointDescList+i;
		if (pCurrentEndpointDesc->pEndpointContext->type==XHCI_ENDPOINT_TYPE_BULK_IN){
			inputEndpointId = i;
			continue;
		}
		if (pCurrentEndpointDesc->pEndpointContext->type==XHCI_ENDPOINT_TYPE_BULK_OUT){
			outputEndpointId = i;
			continue;
		}
	}	
	if (inputEndpointId==0xFFFFFFFFFFFFFFFF){
		printf("failed to find bulk input endpoint descriptor\r\n");
		drive_unregister(driveId);
		mutex_unlock_isr_safe(&mutex);
		return -1;	
	}
	if (outputEndpointId==0xFFFFFFFFFFFFFFFF){
		printf("failed to find bulk output endpoint descriptor\r\n");
		drive_unregister(driveId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct usb_bot_drive_desc* pBotDesc = (struct usb_bot_drive_desc*)kmalloc(sizeof(struct usb_bot_drive_desc));
	if (!pBotDesc){
		printf("failed to allocate USB BOT drive desc\r\n");
		drive_unregister(driveId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	memset((void*)pBotDesc, 0, sizeof(struct usb_bot_drive_desc));
	pBotDesc->port = port;
	pBotDesc->interfaceId = interfaceId;
	pBotDesc->inputEndpointId = inputEndpointId;
	pBotDesc->outputEndpointId = outputEndpointId;	
	struct usb_bot_info driveInfo = {0};
	memset((void*)&driveInfo, 0, sizeof(struct usb_bot_info));	
	mutex_unlock_isr_safe(&mutex);
	return 0;
	if (usb_bot_get_info(pBotDesc, &driveInfo)!=0){
		printf("failed to get USB mass storage device using BOT protocol info\r\n");
		drive_unregister(driveId);
		kfree((void*)pBotDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_interface_unregister(uint8_t port, uint8_t interfaceId){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);

	mutex_unlock(&mutex);
	return 0;
}
