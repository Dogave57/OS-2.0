#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/mutex.h"
#include "stdlib/stdlib.h"
#include "subsystem/usb.h"
#include "drivers/timer.h"
#include "drivers/usb/xhci.h"
#include "drivers/usb/usb-bot.h"
static uint64_t usbDriverId = 0;
static uint64_t driveDriverId = 0;
int usb_bot_driver_init(void){
	struct drive_driver_vtable driveDriverVtable = {0};
	memset((void*)&driveDriverVtable, 0, sizeof(struct drive_driver_vtable));
	driveDriverVtable.driveRegister = usb_bot_subsystem_drive_register;
	driveDriverVtable.driveUnregister = usb_bot_subsystem_drive_unregister;
	driveDriverVtable.getDriveInfo = usb_bot_subsystem_get_drive_info;
	if (drive_driver_register(driveDriverVtable, &driveDriverId)!=0){
		printf("failed to register USB BOT interface protocol drive driver\r\n");
		usb_driver_unregister(usbDriverId);
		return -1;
	}
	struct usb_driver_vtable usbDriverVtable = {0};
	memset((void*)&usbDriverVtable, 0, sizeof(struct usb_driver_vtable));
	usbDriverVtable.registerInterface = usb_bot_interface_register;
	usbDriverVtable.unregisterInterface = usb_bot_interface_unregister;
	if (usb_driver_register(usbDriverVtable, 0x08, 0x06, &usbDriverId)!=0){
		printf("failed to register USB BOT interface protocol driver\r\n");
		return -1;
	}
	return 0;
}
int usb_bot_send_packet(struct usb_bot_drive_desc* pDesc, struct usb_bot_packet_request* pPacketRequest, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pPacketRequest||!pCmdResponse)
		return -1;
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(pDesc->port, &pDevice)!=0)
		return -1;
	struct xhci_usb_packet_request usbPacketRequest = {0};
	memset((void*)&usbPacketRequest, 0, sizeof(struct xhci_usb_packet_request));
	pPacketRequest->cmd.signature = BOT_CMD_SIGNATURE;
	pPacketRequest->cmd.cmd_id = 1;
	usbPacketRequest.interfaceId = pDesc->interfaceId;
	usbPacketRequest.endpointId = pDesc->outputEndpointId;
	usbPacketRequest.pBuffer = (unsigned char*)&pPacketRequest->cmd;
	usbPacketRequest.bufferSize = 31;
	struct xhci_trb eventTrb = {0};
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		pCmdResponse->eventTrb = eventTrb;
		return -1;
	}	
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	usbPacketRequest.endpointId = pDesc->inputEndpointId;
	usbPacketRequest.pBuffer = pPacketRequest->pBuffer;
	usbPacketRequest.bufferSize = pPacketRequest->bufferSize;
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		pCmdResponse->eventTrb = eventTrb;
		return -1;
	}
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	usbPacketRequest.pBuffer = (unsigned char*)&pPacketRequest->cmdStatus;	
	usbPacketRequest.bufferSize = 13;
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";	
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		pCmdResponse->eventTrb = eventTrb;
		return -1;
	}
	if (pPacketRequest->cmdStatus.status!=0x0){
		pCmdResponse->eventTrb = eventTrb;
		struct usb_bot_cmd_response cmdResponse = {0};
		pCmdResponse->senseAvailable = (!usb_bot_request_sense(pDesc, &pCmdResponse->cmdSense, &cmdResponse)) ? 1 : 0;
		return -1;
	}	
	pCmdResponse->eventTrb = eventTrb;	
	return 0;
}
int usb_bot_send_packet_no_data(struct usb_bot_drive_desc* pDesc, struct usb_bot_packet_request* pPacketRequest, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pPacketRequest||!pCmdResponse)
		return -1;
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(pDesc->port, &pDevice)!=0)
		return -1;
	struct xhci_usb_packet_request usbPacketRequest = {0};
	memset((void*)&usbPacketRequest, 0, sizeof(struct xhci_usb_packet_request));
	pPacketRequest->cmd.signature = BOT_CMD_SIGNATURE;
	pPacketRequest->cmd.cmd_id = 1;	
	usbPacketRequest.interfaceId = pDesc->interfaceId;
	usbPacketRequest.endpointId = pDesc->outputEndpointId;
	usbPacketRequest.pBuffer = (unsigned char*)&pPacketRequest->cmd;
	usbPacketRequest.bufferSize = 31;
	struct xhci_trb eventTrb = {0};
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);	
		pCmdResponse->eventTrb = eventTrb;
		return -1;
	}
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	usbPacketRequest.pBuffer = (unsigned char*)&pPacketRequest->cmdStatus;
	usbPacketRequest.endpointId = pDesc->inputEndpointId;
	usbPacketRequest.bufferSize = 13;
	if (xhci_send_usb_packet(pDevice, usbPacketRequest, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		pCmdResponse->eventTrb = eventTrb;
		return -1;
	}
	if (pPacketRequest->cmdStatus.status!=0x0){
		struct usb_bot_cmd_response cmdResponse = {0};
		pCmdResponse->senseAvailable = (!usb_bot_request_sense(pDesc, &pCmdResponse->cmdSense, &cmdResponse)) ? 1 :0;
		pCmdResponse->eventTrb = eventTrb;
		return -1;
	}	
	pCmdResponse->eventTrb = eventTrb;
	return 0;
}
int usb_bot_get_info(struct usb_bot_drive_desc* pDesc, struct usb_bot_info_desc* pDriveInfo, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pDriveInfo||!pCmdResponse)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct usb_bot_info driveInfo = {0};
	struct usb_bot_cmd_status cmdStatus = {0};
	memset((void*)&cmdStatus, 0, sizeof(struct usb_bot_cmd_status));	
	struct usb_bot_packet_request packetRequest = {0};
	memset((void*)&packetRequest, 0, sizeof(struct usb_bot_packet_request));	
	struct usb_bot_get_drive_info_cmd* pDriveInfoCmd = (struct usb_bot_get_drive_info_cmd*)packetRequest.cmd.cmd_block;
	pDriveInfoCmd->opcode = BOT_OPCODE_GET_INFO;
	pDriveInfoCmd->alloc_len = 36;
	packetRequest.cmd.signature = BOT_CMD_SIGNATURE;
	packetRequest.cmd.cmd_id = 1;
	packetRequest.cmd.data_transfer_len = 36;
	packetRequest.cmd.flags = (1<<7);
	packetRequest.cmd.cmd_block_len = 0x06;
	packetRequest.pBuffer = (unsigned char*)&driveInfo;
	packetRequest.bufferSize = 36;
	if (usb_bot_send_packet(pDesc, &packetRequest, pCmdResponse)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(pCmdResponse->eventTrb.event.completion_code, &errorName);
		printf("failed to send BOT get drive info packet (%s)\r\n", errorName);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct usb_bot_info_desc driveInfoDesc = {0};
	memset((void*)&driveInfoDesc, 0, sizeof(struct usb_bot_info_desc));
	driveInfoDesc.info = driveInfo;
	*pDriveInfo = driveInfoDesc;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_get_standard_capacity(struct usb_bot_drive_desc* pDesc, struct usb_bot_capacity_desc* pCapacityDesc, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pCapacityDesc||!pCmdResponse)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct usb_bot_standard_capacity capacity = {0};
	memset((void*)&capacity, 0, sizeof(struct usb_bot_standard_capacity));
	struct usb_bot_packet_request packetRequest = {0};
	memset((void*)&packetRequest, 0, sizeof(struct usb_bot_packet_request));
	struct usb_bot_get_standard_capacity_cmd* pCmdBlock = (struct usb_bot_get_standard_capacity_cmd*)packetRequest.cmd.cmd_block;
	pCmdBlock->opcode = BOT_OPCODE_GET_STANDARD_CAPACITY;
	packetRequest.cmd.signature = BOT_CMD_SIGNATURE;
	packetRequest.cmd.cmd_id = 1;	
	packetRequest.cmd.data_transfer_len = 8;
	packetRequest.cmd.flags = (1<<7);
	packetRequest.cmd.cmd_block_len = 10;
	packetRequest.pBuffer = (unsigned char*)&capacity;
	packetRequest.bufferSize = 8;
	if (usb_bot_send_packet(pDesc, &packetRequest, pCmdResponse)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct usb_bot_capacity_desc capacityDesc = {0};
	memset((void*)&capacityDesc, 0, sizeof(struct usb_bot_capacity_desc));
	capacityDesc.standardCapacity = capacity;
	*pCapacityDesc = capacityDesc;	
	mutex_unlock_isr_safe(&mutex);	
	return 0;
}
int usb_bot_get_extended_capacity(struct usb_bot_drive_desc* pDesc, struct usb_bot_capacity_desc* pCapacityDesc, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pCapacityDesc||!pCmdResponse)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct usb_bot_extended_capacity capacity = {0};
	memset((void*)&capacity, 0, sizeof(struct usb_bot_extended_capacity));	
	struct usb_bot_packet_request packetRequest = {0};
	memset((void*)&packetRequest, 0, sizeof(struct usb_bot_packet_request));
	struct usb_bot_get_extended_capacity_cmd* pCmdBlock = (struct usb_bot_get_extended_capacity_cmd*)packetRequest.cmd.cmd_block;
	pCmdBlock->opcode = BOT_OPCODE_SERVICE_ACTION_IN;	
	pCmdBlock->service_action = BOT_SERVICE_ACTION_READ_EXTENDED_CAPACITY;	
	pCmdBlock->alloc_len = 32;
	packetRequest.cmd.signature = BOT_CMD_SIGNATURE;
	packetRequest.cmd.cmd_id = 1;
	packetRequest.cmd.data_transfer_len = 32;
	packetRequest.cmd.flags = (1<<7);
	packetRequest.cmd.cmd_block_len = 16;
	packetRequest.pBuffer = (unsigned char*)&capacity;
	packetRequest.bufferSize = 32;
	if (usb_bot_send_packet(pDesc, &packetRequest, pCmdResponse)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct usb_bot_capacity_desc capacityDesc = {0};
	memset((void*)&capacityDesc, 0, sizeof(struct usb_bot_capacity_desc));
	capacityDesc.extendedSupport = 1;	
	capacityDesc.extendedCapacity = capacity;
	*pCapacityDesc = capacityDesc;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_get_capacity(struct usb_bot_drive_desc* pDesc, struct usb_bot_capacity_desc* pCapacityDesc, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pCapacityDesc||!pCmdResponse)
		return -1;
	if (!usb_bot_get_extended_capacity(pDesc, pCapacityDesc, pCmdResponse)){
		pCapacityDesc->lbaCount = __builtin_bswap64(pCapacityDesc->extendedCapacity.last_lba);
		pCapacityDesc->blockSize = __builtin_bswap32(pCapacityDesc->extendedCapacity.block_size);
		return 0;
	}
	if (!usb_bot_get_standard_capacity(pDesc, pCapacityDesc, pCmdResponse)){
		pCapacityDesc->lbaCount = __builtin_bswap32(pCapacityDesc->standardCapacity.last_lba);
		pCapacityDesc->blockSize = __builtin_bswap32(pCapacityDesc->standardCapacity.block_size);
		return 0;
	}
	return -1;
}
int usb_bot_request_sense(struct usb_bot_drive_desc* pDesc, struct usb_bot_cmd_sense* pSense, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pSense||!pCmdResponse)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct usb_bot_packet_request packetRequest = {0};
	memset((void*)&packetRequest, 0, sizeof(struct usb_bot_packet_request));
	struct usb_bot_cmd_sense sense = {0};
	memset((void*)&sense, 0, sizeof(struct usb_bot_cmd_sense));
	struct usb_bot_request_sense_cmd* pCmdBlock = (struct usb_bot_request_sense_cmd*)packetRequest.cmd.cmd_block;
	pCmdBlock->opcode = BOT_OPCODE_REQUEST_SENSE;
	pCmdBlock->alloc_length = 18;
	packetRequest.cmd.signature = BOT_CMD_SIGNATURE;
	packetRequest.cmd.cmd_id = 1;
	packetRequest.cmd.data_transfer_len = 18;
	packetRequest.cmd.flags = (1<<7);
	packetRequest.cmd.cmd_block_len = 6;
	packetRequest.pBuffer = (unsigned char*)&sense;
	packetRequest.bufferSize = 18;
	if (usb_bot_send_packet(pDesc, &packetRequest, pCmdResponse)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	*pSense = sense;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_ping(struct usb_bot_drive_desc* pDesc, struct usb_bot_cmd_response* pCmdResponse){
	if (!pDesc||!pCmdResponse)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);	
	struct usb_bot_packet_request packetRequest = {0};
	memset((void*)&packetRequest, 0, sizeof(struct usb_bot_packet_request));
	packetRequest.cmd.signature = BOT_CMD_SIGNATURE;
	packetRequest.cmd.cmd_id = 1;
	packetRequest.cmd.cmd_block_len = 0x6;
	*(uint8_t*)packetRequest.cmd.cmd_block = BOT_OPCODE_PING;
	struct xhci_trb eventTrb = {0};
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	if (usb_bot_send_packet_no_data(pDesc, &packetRequest, pCmdResponse)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(pCmdResponse->eventTrb.event.completion_code, &errorName);
		printf("failed to send BOT ping (%s)\r\n", errorName);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_get_response_code_name(uint8_t responseCode, const unsigned char** ppResponseName){
	if (!ppResponseName)
		return -1;
	if (responseCode>0x73||responseCode<0x70)
		return -1;
	static const unsigned char* mapping[]={
		[BOT_RESPONSE_CODE_FIXED_CURRENT_ERROR-0x70]="Fixed current error",
		[BOT_RESPONSE_CODE_FIXED_DEFERRED_ERROR-0x70]="Fixed deferred error",
		[BOT_RESPONSE_CODE_DESC_CURRENT_ERROR-0x70]="Descriptor current error",
		[BOT_RESPONSE_CODE_DESC_DEFERRED_ERROR-0x70]="Descriptor deferred error",
	};
	*ppResponseName = mapping[responseCode-0x70];
	return 0;
}
int usb_bot_get_sense_key_name(uint8_t senseKey, const unsigned char** ppKeyName){
	if (!ppKeyName)
		return -1;
	if (senseKey>0xE)
		return -1;
	static const unsigned char* mapping[]={
		[BOT_SENSE_KEY_NO_SENSE]="No sense",
		[BOT_SENSE_KEY_RECOVERED_ERROR]="Recovered error",
		[BOT_SENSE_KEY_NOT_READY]="Device not ready",
		[BOT_SENSE_KEY_MEDIUM_ERROR]="Medium error",
		[BOT_SENSE_KEY_HARDWARE_ERROR]="Hardware error",
		[BOT_SENSE_KEY_ILLEGAL_REQUEST]="Illegal request",
		[BOT_SENSE_KEY_UNIT_ATTENTION]="Unit attention",
		[BOT_SENSE_KEY_DATA_PROTECT]="Data write protected",
		[BOT_SENSE_KEY_BLANK_CHECK]="Blank check",
		[BOT_SENSE_KEY_VENDOR_SPECIFIC]="Vendor specific",
		[BOT_SENSE_KEY_COPY_ABORTED]="Copy aborted",
		[BOT_SENSE_KEY_ABORTED_CMD]="Aborted command",
		[BOT_SENSE_KEY_EQUAL]="Equal",
		[BOT_SENSE_KEY_VOLUME_OVERFLOW]="Volume overflow",
		[BOT_SENSE_KEY_VERIFY_FAIL]="Verification failure",
	};
	*ppKeyName = mapping[senseKey];
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
	printf("registering drive\r\n");
	if (drive_register(driveDriverId, port, &driveId)!=0){
		printf("failed to register USB interface following BOT at port %d interface %d\r\n", port, interfaceId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	printf("done\r\n");
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
	struct usb_bot_cmd_response cmdResponse = {0};
	memset((void*)&cmdResponse, 0, sizeof(struct usb_bot_cmd_response));	
	struct usb_bot_info_desc driveInfo = {0};
	memset((void*)&driveInfo, 0, sizeof(struct usb_bot_info_desc));	
	if (usb_bot_get_info(pBotDesc, &driveInfo, &cmdResponse)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(cmdResponse.eventTrb.event.completion_code, &errorName);
		const unsigned char* responseName = "Unknown response";
		usb_bot_get_response_code_name(cmdResponse.cmdSense.response_code, &responseName);
		const unsigned char* senseKeyName = "Unknown sense key";
		usb_bot_get_sense_key_name(cmdResponse.cmdSense.sense_key, &senseKeyName);
		printf("failed to get USB mass storage device using BOT protocol info (%s) response code (%s) sense key (%s)\r\n", errorName, responseName, senseKeyName);
		drive_unregister(driveId);
		kfree((void*)pBotDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	printf("device type: %d\r\n", driveInfo.info.device_type);
	printf("device removable: %d\r\n", driveInfo.info.removable ? 1 : 0);
	print("vendor: ");
	for (uint64_t i = 0;i<sizeof(driveInfo.info.extra.vendor_id)&&driveInfo.info.extra.vendor_id[i];i++){
		putchar(driveInfo.info.extra.vendor_id[i]);
	}
	putchar('\n');
	print("product: ");
	for (uint64_t i = 0;i<sizeof(driveInfo.info.extra.product_id)&&driveInfo.info.extra.product_id[i];i++){
		putchar(driveInfo.info.extra.product_id[i]);	
	}	
	putchar('\n');
	struct usb_bot_capacity_desc capacityDesc = {0};		
	memset((void*)&capacityDesc, 0, sizeof(struct usb_bot_extended_capacity));
	if (usb_bot_get_capacity(pBotDesc, &capacityDesc, &cmdResponse)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(cmdResponse.eventTrb.event.completion_code, &errorName);
		const unsigned char* responseName = "Unknown response";
		usb_bot_get_response_code_name(cmdResponse.cmdSense.response_code, &responseName);
		const unsigned char* senseKeyName = "Unknown sense key";
		usb_bot_get_sense_key_name(cmdResponse.cmdSense.sense_key, &senseKeyName);
		printf("failed to get BOT mass storage device capacity (%s) response code (%s) sense key (%s)\r\n", errorName, responseName, senseKeyName);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	uint64_t driveSize = capacityDesc.lbaCount*capacityDesc.blockSize;
	printf("block size: %d\r\n", capacityDesc.blockSize);
	printf("LBA count: %d\r\n", capacityDesc.lbaCount);	
	printf("drive size: %dMB\r\n", driveSize/MEM_MB);
	printf("extended capacity support: %s\r\n", capacityDesc.extendedSupport ? "Yes" : "No");	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_interface_unregister(uint8_t port, uint8_t interfaceId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);

	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_subsystem_drive_register(uint64_t driveId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);

	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_subsystem_drive_unregister(uint64_t driveId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);

	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int usb_bot_subsystem_get_drive_info(uint64_t driveId, struct drive_info* pDriveInfo){
	if (!pDriveInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);

	mutex_unlock_isr_safe(&mutex);
	return 0;
}
