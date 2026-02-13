#include "stdlib/stdlib.h"
#include "align.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/idt.h"
#include "cpu/interrupt.h"
#include "cpu/mutex.h"
#include "subsystem/usb.h"
#include "subsystem/pcie.h"
#include "drivers/pcie.h"
#include "drivers/apic.h"
#include "drivers/timer.h"
#include "drivers/usb/usb-kbd.h"
#include "drivers/usb/usb-bot.h"
#include "drivers/usb/xhci.h"
struct pcie_location xhciLocation = {0};
struct xhci_info xhciInfo = {0};
uint64_t driverId = 0;
int xhci_driver_init(void){
	struct pcie_driver_vtable vtable = {0};
	memset((void*)&vtable, 0, sizeof(struct pcie_driver_vtable));	
	vtable.registerFunction = xhci_subsystem_register_function;
	vtable.unregisterFunction = xhci_subsystem_unregister_function;
	if (pcie_subsystem_driver_register(vtable, PCIE_CLASS_USB_HC, PCIE_SUBCLASS_USB, &driverId)!=0)
		return -1;
	return 0;
}
int xhci_init(void){
	memset((void*)&xhciInfo, 0, sizeof(struct xhci_info));
	if (xhci_get_info(&xhciInfo)!=0)
		return -1;
	printf("XHCI controller bus: %d, dev: %d, func: %d\r\n", xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func);
	uint32_t pcie_cmd_reg = 0;
	pcie_read_dword(xhciInfo.location, 0x4, &pcie_cmd_reg);
	pcie_cmd_reg|=(1<<0);
	pcie_cmd_reg|=(1<<1);
	pcie_cmd_reg|=(1<<2);
	pcie_cmd_reg|=(1<<10);
	pcie_write_dword(xhciInfo.location, 0x4, pcie_cmd_reg);
	if (usb_subsystem_init()!=0){
		printf("failed to initialize USB subsystem\r\n");
		return -1;
	}	
	if (xhci_reset()!=0){
		printf("failed to reset XHCI controller\r\n");
		return -1;
	}
	if (xhci_stop()!=0){
		printf("failed to stop XHCI controller\r\n");
		return -1;
	}
	if (xhci_init_device_context_list()!=0){
		printf("failed to initialize device context list\r\n");
		return -1;
	}
	if (xhci_init_device_desc_list()!=0){
		printf("failed to initialize device desc list\r\n");
		return -1;
	}	
	if (xhci_init_transfer_ring_list()!=0){
		printf("failed to initialize transfer ring list\r\n");
		return -1;
	}
	if (xhci_init_transfer_desc_list()!=0){
		printf("failed to initialize transfer desc list\r\n");
		return -1;
	}
	if (xhci_init_cmd_ring(&xhciInfo.pCmdRingInfo)!=0){
		printf("failed to initialize cmd TRB ring list\r\n");
		return -1;
	}	
	xhci_set_cmd_ring_base(xhciInfo.pCmdRingInfo->pRingBuffer_phys);	
	if (xhci_init_interrupter()!=0){
		printf("failed to initialize XHCI interrupter\r\n");
		return -1;
	}
	uint8_t portCount = 0;
	if (xhci_get_port_count(&portCount)!=0)
		return -1;
	struct xhci_trb trb = {0};
	uint64_t trbIndex = 0;
	memset((void*)&trb, 0, sizeof(struct xhci_trb));
	trb.command.control.type = XHCI_TRB_TYPE_NOP_CMD;
	trb.command.control.ioc = 1;
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, trb, &pCmdDesc)!=0){
		printf("failed to allocate TRB entry\r\n");
		return -1;
	}
	struct xhci_cmd_desc* pNewCmdDesc = (struct xhci_cmd_desc*)0x0;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, trb, &pNewCmdDesc)!=0){
		printf("failed to allocate new TRB entry\r\n");
		return -1;
	}
	xhci_start();
	xhci_ring(0);
	uint64_t time_us = get_time_us();
	while (!pCmdDesc->cmdComplete){};
	const unsigned char* completionStatusName = "Unknown completion status\r\n";
	xhci_get_error_name(pCmdDesc->eventTrb.event.completion_code, &completionStatusName);
	while (!pCmdDesc->cmdComplete){};	
	xhci_get_error_name(pCmdDesc->eventTrb.event.completion_code, &completionStatusName);
	uint64_t elapsed_us = get_time_us()-time_us;
	struct xhci_usb_status usb_status = xhciInfo.pOperational->usb_status;
	if (usb_status.host_system_error)
		printf("host system error\r\n");
	if (usb_status.host_controller_error)
		printf("host controller error\r\n");
	for (uint8_t i = 0;i<portCount;i++){
		if (xhci_device_exists(i)!=0)
			continue;
		xhci_reset_port(i);
	}
	if (usb_kbd_driver_init()!=0){
		printf("failed to initialize USB keyboard HID driver\r\n");
		return -1;
	}
	if (usb_bot_driver_init()!=0){
		printf("failed to initialize USB BOT interface protocol driver\r\n");
		return -1;
	}
	return 0;
}
int xhci_get_info(struct xhci_info* pInfo){
	if (!pInfo)
		return -1;
	if (xhciInfo.pBaseMmio){
		*pInfo = xhciInfo;
		return 0;
	}
	if (xhci_get_location(&pInfo->location)!=0)
		return -1;
	if (pcie_get_bar(pInfo->location, 0, &pInfo->pBaseMmio_physical)!=0)
		return -1;
	uint64_t baseMmioPages = 64;
	if (virtualGetSpace((uint64_t*)&pInfo->pBaseMmio, baseMmioPages)!=0)
		return -1;
	if (virtualMapPages(pInfo->pBaseMmio_physical, pInfo->pBaseMmio, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, baseMmioPages, 1, 0, PAGE_TYPE_MMIO)!=0){
		return -1;
	}
	pInfo->pCapabilities = (volatile struct xhci_cap_mmio*)pInfo->pBaseMmio;
	uint64_t doorbellOffset = pInfo->pCapabilities->doorbell_offset&(~0x3);
	uint64_t runtimeOffset = pInfo->pCapabilities->runtime_register_offset&(~0x1F);
	pInfo->pOperational = (volatile struct xhci_operational_mmio*)(pInfo->pBaseMmio+(uint64_t)pInfo->pCapabilities->cap_len);
	pInfo->pRuntime = (volatile struct xhci_runtime_mmio*)(pInfo->pBaseMmio+runtimeOffset);
	pInfo->pDoorBells = (volatile uint32_t*)(pInfo->pBaseMmio+doorbellOffset);
	struct xhci_cap_param0 capParam = pInfo->pCapabilities->cap_param0;
	pInfo->pExtendedCapList = (volatile struct xhci_extended_cap_hdr*)(((uint32_t*)pInfo->pBaseMmio)+(((uint64_t)capParam.extended_cap_ptr)));
	return 0;
}
int xhci_get_location(struct pcie_location* pLocation){
	if (!pLocation)
		return -1;
	*pLocation = xhciLocation;
	return 0;
}
int xhci_read_qword(volatile uint64_t* pQword, uint64_t* pValue){
	if (!pQword||!pValue)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint32_t low = *(volatile uint32_t*)pQword;
	uint32_t high = *(((volatile uint32_t*)pQword)+1);
	*(uint32_t*)pValue = low;
	*(((uint32_t*)pValue)+1) = high;
	mutex_unlock(&mutex);
	return 0;
}
int xhci_write_qword(volatile uint64_t* pQword, uint64_t value){
	if (!pQword)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint32_t low = (uint32_t)(value&0xFFFFFFFF);
	uint32_t high = (uint32_t)((value>>32)&0xFFFFFFFF);
	*(uint32_t*)pQword = low;
	*(((uint32_t*)pQword)+1) = high;
	mutex_unlock(&mutex);
	return 0;
}
int xhci_get_port_list(volatile struct xhci_port** ppPortList){
	if (!ppPortList)
		return -1;
	volatile struct xhci_port* pPortList = (volatile struct xhci_port*)(((uint64_t)xhciInfo.pOperational)+XHCI_PORT_LIST_OFFSET);	
	*ppPortList = pPortList;
	return 0;
}
int xhci_get_port(uint8_t port, volatile struct xhci_port** ppPort){
	if (!ppPort)
		return -1;
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port_list(&pPort)!=0)
		return -1;
	pPort+=((uint64_t)port);
	*ppPort = pPort;
	return 0;
}
int xhci_get_port_speed(uint8_t port, uint8_t* pSpeed){
	if (!pSpeed)
		return -1;
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port(port, &pPort)!=0)
		return -1;
	struct xhci_port_status status = pPort->port_status;
	*pSpeed = status.port_speed;
	return 0;
}
int xhci_device_exists(uint8_t port){
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port(port, &pPort)!=0)
		return -1;
	struct xhci_port_status status = pPort->port_status;	
	return status.connection_status ? 0 : -1;
}
int xhci_reset_port(uint8_t port){
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port(port, &pPort)!=0)
		return -1;
	struct xhci_port_status status = pPort->port_status;
	status.port_reset = 1;
	pPort->port_status = status;
	while (status.port_reset){
		status = pPort->port_status;
	}	
	status.port_power = 1;
	pPort->port_status = status;
	while (!status.port_power){
		status = pPort->port_status;
	}	
	return 0;
}
int xhci_enable_port(uint8_t port){
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port(port, &pPort)!=0)
		return -1;
	struct xhci_port_status status = pPort->port_status;
	status.enable = 1;	
	pPort->port_status = status;
	while (!status.enable){
		status = pPort->port_status;
	}
	return 0;
}
int xhci_disable_port(uint8_t port){
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port(port, &pPort)!=0)
		return -1;
	struct xhci_port_status status = pPort->port_status;
	status.enable = 0;
	pPort->port_status = status;
	while (status.enable){
		status = pPort->port_status;
	}
	return 0;
}
int xhci_get_port_count(uint8_t* pPortCount){
	if (!pPortCount)
		return -1;
	if (!xhciInfo.pBaseMmio){
		if (xhci_get_info(&xhciInfo)!=0)
			return -1;
	}
	volatile struct xhci_cap_mmio* pCap = xhciInfo.pCapabilities;
	if (!pCap)
		return -1;
	struct xhci_structure_param0 param0 = pCap->structure_param0;	
	uint8_t portCount = param0.max_ports;
	*pPortCount = portCount;
	return 0;
}
int xhci_get_device(uint8_t port, struct xhci_device** ppDevice){
	if (!ppDevice)
		return -1;
	struct xhci_device* pDevice = xhciInfo.pDeviceDescList+(uint64_t)port;
	*ppDevice = pDevice;
	return 0;
}
int xhci_init_device_desc_list(void){
	uint64_t deviceDescListSize = XHCI_MAX_SLOT_COUNT*sizeof(struct xhci_device);
	struct xhci_device* pDeviceDescList = (struct xhci_device*)0x0;
	if (virtualAlloc((uint64_t*)&pDeviceDescList, deviceDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	xhciInfo.pDeviceDescList = pDeviceDescList;	
	return 0;
}
int xhci_init_cmd_ring(struct xhci_cmd_ring_info** ppRingInfo){
	if (!ppRingInfo)
		return -1;
	volatile struct xhci_trb* pRingBuffer = (volatile struct xhci_trb*)0x0;
	if (virtualAllocPage((uint64_t*)&pRingBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	struct xhci_cmd_desc* pCmdDescList = (struct xhci_cmd_desc*)0x0;
	uint64_t cmdListDescSize = XHCI_MAX_CMD_TRB_ENTRIES*sizeof(struct xhci_cmd_desc);
	if (virtualAlloc((uint64_t*)&pCmdDescList, cmdListDescSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		virtualFreePage((uint64_t)pRingBuffer, 0);
		return -1;
	}
	struct xhci_cmd_ring_info* pRingInfo = (struct xhci_cmd_ring_info*)kmalloc(sizeof(struct xhci_cmd_ring_info));
	if (!pRingInfo){
		printf("failed to allocate cmd TRB ring info\r\n");
		virtualFree((uint64_t)pCmdDescList, cmdListDescSize);
		return -1;
	}
	memset((void*)pRingInfo, 0, sizeof(struct xhci_cmd_ring_info));
	pRingInfo->cycle_state = 1;
	pRingInfo->pRingBuffer = pRingBuffer;
	pRingInfo->maxEntries = XHCI_MAX_CMD_TRB_ENTRIES;
	pRingInfo->pCmdDescList = pCmdDescList;
	pRingInfo->cmdDescListSize = cmdListDescSize;
	uint64_t ringBufferSize = pRingInfo->maxEntries*sizeof(struct xhci_trb);
	pRingInfo->ringBufferSize = ringBufferSize;
	*ppRingInfo = pRingInfo;
	if (virtualToPhysical((uint64_t)pRingBuffer, &pRingInfo->pRingBuffer_phys)!=0){
		printf("failed to get physical address of TRB ring buffer\r\n");
		virtualFreePage((uint64_t)pRingBuffer, 0);
		virtualFreePage((uint64_t)pCmdDescList, 0);
		return -1;
	}
	memset((void*)pRingBuffer, 0, ringBufferSize);
	struct xhci_trb linkTrb = {0};
	memset((void*)&linkTrb, 0, sizeof(struct xhci_trb));
	linkTrb.generic.ptr = (uint64_t)pRingInfo->pRingBuffer_phys;
	linkTrb.generic.control.tc_bit = 1;
	linkTrb.generic.control.type = XHCI_TRB_TYPE_LINK;
	if (xhci_write_cmd(pRingInfo, XHCI_MAX_CMD_TRB_ENTRIES-1, linkTrb)!=0){
		xhci_deinit_cmd_ring(pRingInfo);
		return -1;
	}
	return 0;
}
int xhci_deinit_cmd_ring(struct xhci_cmd_ring_info* pRingInfo){
	if (!pRingInfo)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pRingBuffer, 0)!=0)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pCmdDescList, 0)!=0)
		return -1;
	return 0;
}
int xhci_alloc_cmd(struct xhci_cmd_ring_info* pRingInfo, struct xhci_trb trb, struct xhci_cmd_desc** ppCmdDesc){
	if (!pRingInfo||!ppCmdDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	uint64_t trbIndex = pRingInfo->currentEntry;
	if (trbIndex>=XHCI_MAX_CMD_TRB_ENTRIES-1){
		trbIndex = 0;
		pRingInfo->currentEntry = 0;
		volatile struct xhci_trb* pLinkTrb = (volatile struct xhci_trb*)0x0;
		xhci_get_cmd(pRingInfo, XHCI_MAX_CMD_TRB_ENTRIES-1, &pLinkTrb);
		pLinkTrb->command.control.cycle_bit = pRingInfo->cycle_state;
		pRingInfo->cycle_state = !pRingInfo->cycle_state;
	}
	xhci_write_cmd(pRingInfo, trbIndex, trb);
	volatile struct xhci_trb* pCmdTrb = pRingInfo->pRingBuffer+trbIndex;
	uint64_t pCmdTrb_phys = pRingInfo->pRingBuffer_phys+(trbIndex*sizeof(struct xhci_trb));	
	struct xhci_cmd_desc* pCmdDesc = pRingInfo->pCmdDescList+trbIndex;
	memset((void*)pCmdDesc, 0, sizeof(struct xhci_cmd_desc));
	pCmdDesc->pCmdTrb = pCmdTrb;
	pCmdDesc->pCmdTrb_phys = pCmdTrb_phys;
	pCmdDesc->trbIndex = trbIndex;
	pRingInfo->currentEntry++;	
	*ppCmdDesc = pCmdDesc;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int xhci_get_cmd(struct xhci_cmd_ring_info* pRingInfo, uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry){
	if (!pRingInfo||!ppTrbEntry)
		return -1;
	*ppTrbEntry = pRingInfo->pRingBuffer+trbIndex;
	return 0;
}
int xhci_write_cmd(struct xhci_cmd_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb trbEntry){
	if (!pRingInfo)
		return -1;
	unsigned char cycle_state = pRingInfo->cycle_state;
	trbEntry.generic.control.cycle_bit = cycle_state;
	*(pRingInfo->pRingBuffer+trbIndex) = trbEntry;
	return 0;
}
int xhci_read_cmd(struct xhci_cmd_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb* pTrbEntry){
	if (!pRingInfo||!pTrbEntry)
		return -1;
	*pTrbEntry = *(pRingInfo->pRingBuffer+trbIndex);
	return 0;
}
int xhci_init_transfer_ring_list(void){
	uint64_t listSize = sizeof(struct xhci_transfer_ring_info)*XHCI_MAX_SLOT_COUNT*XHCI_MAX_ENDPOINT_COUNT;
	struct xhci_transfer_ring_info* pTransferRingList = (struct xhci_transfer_ring_info*)0x0;
	if (virtualAlloc((uint64_t*)&pTransferRingList, listSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	xhciInfo.pTransferRingList = pTransferRingList;	
	return 0;
}
int xhci_init_transfer_desc_list(void){
	uint64_t listSize = sizeof(struct xhci_transfer_desc)*XHCI_MAX_DEVICE_COUNT*XHCI_MAX_ENDPOINT_COUNT*256;
	struct xhci_transfer_desc* pTransferDescList = (struct xhci_transfer_desc*)0x0;
	if (virtualAlloc((uint64_t*)&pTransferDescList, listSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	xhciInfo.pTransferDescList = pTransferDescList;
	return 0;
}
int xhci_get_transfer_ring(uint8_t slotId, uint8_t endpointIndex, struct xhci_transfer_ring_info** ppTransferRing){
	if (!ppTransferRing)
		return -1;
	uint64_t transferRingOffset = ((uint64_t)slotId*XHCI_MAX_ENDPOINT_COUNT)+(uint64_t)endpointIndex;
	struct xhci_transfer_ring_info* pTransferRing = xhciInfo.pTransferRingList+transferRingOffset;
	*ppTransferRing = pTransferRing;	
	return 0;
}
int xhci_get_transfer_desc(uint8_t slotId, uint8_t endpointIndex, uint64_t trbIndex, struct xhci_transfer_desc** ppTransferDesc){
	if (!ppTransferDesc)
		return -1;
	uint64_t transferDescOffset = (((uint64_t)slotId*XHCI_MAX_DEVICE_COUNT*XHCI_MAX_ENDPOINT_COUNT)+((uint64_t)endpointIndex*256))+trbIndex;
	struct xhci_transfer_desc* pTransferDesc = xhciInfo.pTransferDescList+transferDescOffset;
	*ppTransferDesc = pTransferDesc;
	return 0;
}
int xhci_init_transfer_ring(struct xhci_device* pDevice, uint8_t interfaceId, uint8_t endpointIndex, uint8_t endpointId, struct xhci_transfer_ring_info** ppTransferRingInfo){
	if (!pDevice||!ppTransferRingInfo)
		return -1;
	volatile struct xhci_trb* pTransferRing = (volatile struct xhci_trb*)0x0;
	uint64_t pTransferRing_phys = 0;
	unsigned char cycle_state = 1;
	if (virtualAllocPage((uint64_t*)&pTransferRing, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate transfer TRB ring\r\n");
		return -1;
	}
	if (virtualToPhysical((uint64_t)pTransferRing, &pTransferRing_phys)!=0){
		printf("failed to get physical addrses of transfer TRB ring\r\n");
		virtualFreePage((uint64_t)pTransferRing, 0);
		return -1;
	}
	memset((void*)pTransferRing, 0, PAGE_SIZE);
	struct xhci_transfer_ring_info* pTransferRingInfo = (struct xhci_transfer_ring_info*)0x0;
	if (xhci_get_transfer_ring(pDevice->deviceContext.slotId, endpointIndex, &pTransferRingInfo)!=0){
		printf("failed to get transfer ring descriptor\r\n");
		virtualFreePage((uint64_t)pTransferRing, 0);
		return -1;
	}
	memset((void*)pTransferRingInfo, 0, sizeof(struct xhci_transfer_ring_info));
	pTransferRingInfo->pRingBuffer = pTransferRing;
	pTransferRingInfo->pRingBuffer_phys = pTransferRing_phys;
	pTransferRingInfo->ringBufferSize = XHCI_MAX_TRANSFER_TRB_ENTRIES;
	pTransferRingInfo->interfaceId = interfaceId;
	pTransferRingInfo->endpointIndex = endpointIndex;
	pTransferRingInfo->endpointId = endpointId;
	pTransferRingInfo->cycle_state = cycle_state;
	pTransferRingInfo->pDevice = pDevice;
	struct xhci_trb linkTrb = {0};
	memset((void*)&linkTrb, 0, sizeof(struct xhci_trb));
	linkTrb.generic.control.type = XHCI_TRB_TYPE_LINK;
	linkTrb.generic.control.tc_bit = 1;
	linkTrb.generic.ptr = pTransferRing_phys;
	xhci_write_transfer(pTransferRingInfo, XHCI_MAX_TRANSFER_TRB_ENTRIES-1, linkTrb);	
	*ppTransferRingInfo = pTransferRingInfo;
	return 0;
}
int xhci_deinit_transfer_ring(struct xhci_transfer_ring_info* pTransferRingInfo){
	if (!pTransferRingInfo)
		return -1;
	if (pTransferRingInfo->pRingBuffer)
		virtualFreePage((uint64_t)pTransferRingInfo->pRingBuffer, 0);
	kfree((void*)pTransferRingInfo);
	return 0;
}
int xhci_alloc_transfer(struct xhci_transfer_ring_info* pTransferRingInfo, struct xhci_trb trb, struct xhci_transfer_desc** ppTransferDesc, xhciTransferCompletionFunc completionFunc){
	if (!pTransferRingInfo||!ppTransferDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	uint64_t trbIndex = pTransferRingInfo->currentEntry;
	if (trbIndex>=XHCI_MAX_TRANSFER_TRB_ENTRIES-1){
		volatile struct xhci_trb* pLinkTrb = (volatile struct xhci_trb*)0x0;
		xhci_get_transfer(pTransferRingInfo, XHCI_MAX_TRANSFER_TRB_ENTRIES-1, &pLinkTrb);
		pLinkTrb->event.control.cycle_bit = pTransferRingInfo->cycle_state ? 1 : 0;
		pTransferRingInfo->currentEntry = 0;
		trbIndex = 0;
		pTransferRingInfo->cycle_state = !pTransferRingInfo->cycle_state;
	}
	xhci_write_transfer(pTransferRingInfo, trbIndex, trb);
	struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;	
	uint8_t port = pTransferRingInfo->pDevice->port;
	uint8_t interfaceId = pTransferRingInfo->interfaceId;
	uint8_t endpointIndex = pTransferRingInfo->endpointIndex;
	if (xhci_get_transfer_desc(pTransferRingInfo->pDevice->deviceContext.slotId, endpointIndex, trbIndex, &pTransferDesc)!=0){
		printf("failed to get transfer desc\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	memset((void*)pTransferDesc, 0, sizeof(struct xhci_transfer_desc));
	pTransferDesc->pTransferRingInfo = pTransferRingInfo;
	pTransferDesc->completionFunc = completionFunc;
	pTransferRingInfo->currentEntry++;
	*ppTransferDesc = pTransferDesc;	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int xhci_get_transfer(struct xhci_transfer_ring_info* pTransferRingInfo, uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry){
	if (!pTransferRingInfo||!ppTrbEntry)
		return -1;
	*ppTrbEntry = pTransferRingInfo->pRingBuffer+trbIndex;
	return 0;
}
int xhci_write_transfer(struct xhci_transfer_ring_info* pTransferRingInfo, uint64_t trbIndex, struct xhci_trb trbEntry){
	if (!pTransferRingInfo)
		return -1;
	unsigned char cycle_state = pTransferRingInfo->cycle_state;
	trbEntry.generic.control.cycle_bit = cycle_state;
	*(pTransferRingInfo->pRingBuffer+trbIndex) = trbEntry;	
	return 0;
}
int xhci_read_transfer(struct xhci_transfer_ring_info* pTransferRingInfo, uint64_t trbIndex, struct xhci_trb* pTrbEntry){
	if (!pTransferRingInfo||!pTrbEntry)
		return -1;
	*pTrbEntry = *(pTransferRingInfo->pRingBuffer+trbIndex);
	return 0;
}
int xhci_ring(uint64_t doorbell_vector){
	volatile uint32_t* pDoorBell = xhciInfo.pDoorBells+doorbell_vector;
	*pDoorBell = 0;
	return 0;
}
int xhci_ring_endpoint(uint64_t slotId, uint8_t endpoint_id, uint8_t stream_id){
	struct xhci_doorbell doorbell = {0};
	memset((void*)&doorbell, 0, sizeof(struct xhci_doorbell));
	doorbell.endpoint_id = endpoint_id;
	doorbell.stream_id = stream_id;
	volatile struct xhci_doorbell* pDoorBell = (volatile struct xhci_doorbell*)(xhciInfo.pDoorBells+slotId);	
	*pDoorBell = doorbell;	
	return 0;
}
int xhci_reset(void){
	xhci_stop();
	struct xhci_usb_cmd cmd = xhciInfo.pOperational->usb_cmd;
	cmd.hc_reset = 1;
	xhciInfo.pOperational->usb_cmd = cmd;
	uint64_t start_us = get_time_us();
	struct xhci_usb_status status = xhciInfo.pOperational->usb_status;
	while (!status.halted||status.controller_not_ready||cmd.hc_reset){
		uint64_t current_us = get_time_us();
		uint64_t elapsed_us = current_us-start_us;
		if (elapsed_us>500000){
			printf("controller reset timed out\r\n");
			return -1;
		}
		status = xhciInfo.pOperational->usb_status;
		cmd = xhciInfo.pOperational->usb_cmd;
	}
	sleep(25);
	xhci_stop();
	sleep(25);
	return 0;
}
int xhci_start(void){
	struct xhci_usb_cmd cmd = xhciInfo.pOperational->usb_cmd;
	cmd.run = 1;
	xhciInfo.pOperational->usb_cmd = cmd;
	struct xhci_usb_status status = {0};
	uint64_t start_us = get_time_us();
	while (1){
		uint64_t elapsed_us = get_time_us()-start_us;
		if (elapsed_us>500000){
			printf("controller start timed out\r\n");
			return -1;
		}
		cmd = xhciInfo.pOperational->usb_cmd;
		if (!cmd.run)
			continue;
		status = xhciInfo.pOperational->usb_status;
		if (status.halted)
			continue;
		return 0;
	}
	return -1;
}
int xhci_stop(void){
	struct xhci_usb_cmd cmd = xhciInfo.pOperational->usb_cmd;
	cmd.run = 0;
	xhciInfo.pOperational->usb_cmd = cmd;
	struct xhci_usb_status status = {0};
	uint64_t start_us = get_time_us();
	while (1){
		uint64_t elapsed_us = get_time_us()-start_us;
		if (elapsed_us>500000){
			printf("controller stop timed out\r\n");
			return -1;
		}
		cmd = xhciInfo.pOperational->usb_cmd;
		if (cmd.run)
			continue;
		status = xhciInfo.pOperational->usb_status;
		if (!status.halted)
			continue;
		return 0;
	}
	return -1;
}
int xhci_is_running(void){
	struct xhci_usb_cmd cmd = xhciInfo.pOperational->usb_cmd;
	struct xhci_usb_status status = xhciInfo.pOperational->usb_status;
	return (cmd.run) ? 0 : -1;
}
int xhci_is_ready(void){
	struct xhci_usb_status status = xhciInfo.pOperational->usb_status;
	return (!status.controller_not_ready) ? 0 : -1;
}
int xhci_get_cmd_ring_base(uint64_t* pBase){
	if (!pBase)
		return -1;
	*pBase = xhciInfo.pCmdRingInfo->pRingBuffer_phys;
	return 0;
}
int xhci_set_cmd_ring_base(uint64_t base){
	uint64_t cmdRingControl = base|(1<<0);
	return xhci_write_qword((uint64_t*)&xhciInfo.pOperational->cmd_ring_ctrl, cmdRingControl);
}
int xhci_get_cmd_ring_running(void){
	union xhci_cmd_ring_ctrl cmdRingControl = xhciInfo.pOperational->cmd_ring_ctrl;
	return (cmdRingControl.cmd_ring_running) ? 0 : -1;
}
int xhci_get_interrupter_base(uint64_t interrupter_id, volatile struct xhci_interrupter** ppBase){
	if (!ppBase)
		return -1;
	volatile struct xhci_interrupter* pBase = xhciInfo.pRuntime->interrupter_list+interrupter_id;
	*ppBase = pBase;
	return 0;
}
int xhci_init_trb_event_list(struct xhci_event_trb_ring_info* pRingInfo){
	if (!pRingInfo)
		return -1;
	memset((void*)pRingInfo, 0, sizeof(struct xhci_event_trb_ring_info));
	if (virtualAllocPage((uint64_t*)&pRingInfo->pRingBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	if (virtualToPhysical((uint64_t)pRingInfo->pRingBuffer, (uint64_t*)&pRingInfo->pRingBuffer_phys)!=0){
		virtualFreePage((uint64_t)pRingInfo->pRingBuffer, 0);
		return -1;
	}	
	pRingInfo->maxEntries = XHCI_MAX_EVENT_TRB_ENTRIES;
	return 0;
}
int xhci_deinit_trb_event_list(struct xhci_event_trb_ring_info* pRingInfo){
	if (!pRingInfo)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pRingBuffer, 0)!=0)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pSegmentTable, 0)!=0)
		return -1;
	memset((void*)&pRingInfo, 0, sizeof(struct xhci_event_trb_ring_info));
	return 0;
}
int xhci_init_interrupter(void){
	volatile struct xhci_interrupter* pInt = (volatile struct xhci_interrupter*)0x0;
	if (xhci_get_interrupter_base(XHCI_DEFAULT_INTERRUPTER_ID, &pInt)!=0){
		printf("failed to get interrupter registers\r\n");
		return -1;
	}
	uint64_t msixVector = 0;
	volatile struct pcie_msix_msg_ctrl* pMsgControl = (volatile struct pcie_msix_msg_ctrl*)0x0;
	volatile struct pcie_msi_msg_ctrl* pMsiMsgControl = (volatile struct pcie_msi_msg_ctrl*)0x0;
	volatile struct pcie_msix_table_entry* pMsixEntry = (volatile struct pcie_msix_table_entry*)0x0;
	if (pcie_msix_get_msg_ctrl(xhciInfo.location, &pMsgControl)!=0){
		printf("failed to get MSIX control\r\n");
		return -1;
	}
	pcie_msi_get_msg_ctrl(xhciInfo.location, &pMsiMsgControl);
	if (pMsiMsgControl){
		printf("disabling MSI\r\n");
		pcie_msi_disable(pMsiMsgControl);
	}
	if (pcie_get_msix_entry(xhciInfo.location, &pMsixEntry, pMsgControl, msixVector)!=0){
		printf("failed to get MSIX table entry\r\n");
		return -1;
	}
	pcie_msix_disable(pMsgControl);
	if (xhci_init_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo)!=0){
		printf("failed to initialize event TRB list\r\n");
		return -1;
	}
	uint64_t lapic_base = 0;
	if (lapic_get_base(&lapic_base)!=0){
		printf("failed to get LAPIC base\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		return -1;
	}
	uint64_t lapic_id = 0;
	if (lapic_get_id(&lapic_id)!=0){
		printf("failed to get lapic ID\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		return -1;
	}
	uint8_t interrupterVector = 0;
	if (idt_get_free_vector(&interrupterVector)!=0){
		printf("failed to get free interrupter vector\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		return -1;
	}
	idt_add_entry(interrupterVector, (uint64_t)xhci_interrupter_isr, 0x8E, 0x0, 1);
	if (pcie_set_msix_entry(xhciInfo.location, pMsgControl, 0, lapic_id, interrupterVector)!=0){
		printf("failed to set MSIX entry\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		return -1;
	}
	volatile struct xhci_segment_table_entry* pSegmentTable = (volatile struct xhci_segment_table_entry*)0x0;
	uint64_t pSegmentTable_phys = 0;
	if (virtualAllocPage((uint64_t*)&pSegmentTable, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate event segment table\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		return -1;
	}
	if (virtualToPhysical((uint64_t)pSegmentTable, &pSegmentTable_phys)!=0){
		printf("failed to get physical address of event ring segment table\r\n");
		virtualFreePage((uint64_t)pSegmentTable, 0);
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		return -1;
	}
	memset((void*)pSegmentTable, 0, PAGE_SIZE);
	struct xhci_segment_table_entry segmentEntry = {0};
	segmentEntry.base = xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys;
	segmentEntry.size = xhciInfo.interrupterInfo.eventRingInfo.maxEntries;
	segmentEntry.reserved0 = 0;
	*pSegmentTable = segmentEntry;
	pInt->table_size = 1;
	struct xhci_interrupter_iman iman = pInt->interrupt_management;
	iman.interrupt_enable = 1;
	iman.interrupt_pending = 1;
	pInt->interrupt_management = iman;
	uint32_t imod = pInt->interrupt_moderation;
	imod = 0;
	pInt->interrupt_moderation = imod;
	struct xhci_usb_cmd usb_cmd = xhciInfo.pOperational->usb_cmd;
	xhci_write_qword((uint64_t*)&pInt->table_base, pSegmentTable_phys);	
	usb_cmd.interrupter_enable = 1;
	xhciInfo.pOperational->usb_cmd = usb_cmd;
	xhciInfo.interrupterInfo.eventRingInfo.pSegmentTable = pSegmentTable;
	xhciInfo.interrupterInfo.eventRingInfo.pSegmentTable_phys = pSegmentTable_phys;
	xhciInfo.interrupterInfo.eventRingInfo.maxSegmentTableEntryCount = XHCI_MAX_EVENT_SEGMENT_TABLE_ENTRIES;
	xhciInfo.interrupterInfo.vector = interrupterVector;
	pcie_msix_enable_entry(xhciInfo.location, pMsgControl, msixVector);
	pcie_msix_enable(pMsgControl);
	xhci_send_ack(0);
	xhci_update_dequeue_trb();
	return 0;
}
int xhci_send_ack(uint64_t interrupter_id){
	volatile struct xhci_interrupter* pInt = (volatile struct xhci_interrupter*)0x0;
	if (xhci_get_interrupter_base(interrupter_id, &pInt)!=0)
		return -1;
	struct xhci_usb_status usb_status = xhciInfo.pOperational->usb_status;
	usb_status.event_int = 1;
	xhciInfo.pOperational->usb_status = usb_status;
	struct xhci_interrupter_iman iman = pInt->interrupt_management;	
	iman.interrupt_pending = 1;
	pInt->interrupt_management = iman;	
	return 0;
}
int xhci_get_dequeue_trb_phys(uint64_t* ppTrbEntry){
	if (!ppTrbEntry)
		return -1;
	*ppTrbEntry = xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase_phys;
	return 0;
}
int xhci_get_dequeue_trb(volatile struct xhci_trb** ppTrbEntry){
	if (!ppTrbEntry)
		return -1;
	*ppTrbEntry = xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase;
	return 0;
}
int xhci_update_dequeue_trb(void){
	volatile struct xhci_interrupter* pInt = (volatile struct xhci_interrupter*)0x0;
	if (xhci_get_interrupter_base(0, &pInt)!=0)
		return -1;
	if (!xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase||(xhciInfo.interrupterInfo.eventRingInfo.dequeueTrb)>xhciInfo.interrupterInfo.eventRingInfo.maxEntries-1){
		xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase_phys = xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys;
		xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase = xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer;
		xhciInfo.interrupterInfo.eventRingInfo.dequeueTrb = 0;
	}
	else{
		xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase_phys+=sizeof(struct xhci_trb);
		xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase++;
	}
	uint64_t dequeue_ring_ptr = xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase_phys;
	dequeue_ring_ptr|=(1<<3);
	xhci_write_qword((uint64_t*)&pInt->dequeue_ring_ptr, dequeue_ring_ptr);
	xhciInfo.interrupterInfo.eventRingInfo.dequeueTrb++;
	return 0;
}
int xhci_get_event_trb(volatile struct xhci_trb** ppTrbEntry){
	if (!ppTrbEntry)
		return -1;
	volatile struct xhci_trb* pTrbEntry = (volatile struct xhci_trb*)0x0;
	if (xhci_get_dequeue_trb(&pTrbEntry)!=0)
		return -1;
	if (!pTrbEntry)
		pTrbEntry = xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer;
	*ppTrbEntry = pTrbEntry;
	return 0;
}
int xhci_get_event_trb_phys(uint64_t* ppTrbEntry){
	if (!ppTrbEntry)
		return -1;
	uint64_t pTrbEntry = 0;
	if (xhci_get_dequeue_trb_phys(&pTrbEntry)!=0)
		return -1;
	if (!pTrbEntry)
		pTrbEntry = xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys;
	*ppTrbEntry = pTrbEntry;
	return 0;
}
int xhci_get_trb_type_name(uint64_t type, const unsigned char** ppName){
	if (!ppName){
		return -1;
	}
	if (type>0x27){
		*ppName = "Invalid type";
		return 0;
	}
	static const unsigned char* mapping[]={
		[XHCI_TRB_TYPE_INVALID]="Invalid type",
		[XHCI_TRB_TYPE_NORMAL]="Normal",
		[XHCI_TRB_TYPE_SETUP]="Setup",
		[XHCI_TRB_TYPE_DATA]="Data",
		[XHCI_TRB_TYPE_STATUS]="Status",
		[XHCI_TRB_TYPE_ISOCH]="Isoch",
		[XHCI_TRB_TYPE_LINK]="Link",
		[XHCI_TRB_TYPE_EVENT]="Event",
		[XHCI_TRB_TYPE_NOP_TRANSFER]="Nop transfer",
		[XHCI_TRB_TYPE_ENABLE_SLOT]="Enable slot",
		[XHCI_TRB_TYPE_DISABLE_SLOT]="Disable slot",
		[XHCI_TRB_TYPE_ADDRESS_DEVICE]="Address device",
		[XHCI_TRB_TYPE_CONFIGURE_ENDPOINT]="Configure endpoint",
		[XHCI_TRB_TYPE_EVALUATE_CONTEXT]="Evaluate context",
		[XHCI_TRB_TYPE_RESET_ENDPOINT]="Reset endpoint",
		[XHCI_TRB_TYPE_STOP_ENDPOINT]="Stop endpoint",
		[XHCI_TRB_TYPE_SET_EVENT_DEQUEUE_PTR]="Set event dequeue pointer",
		[XHCI_TRB_TYPE_RESET_DEVICE]="Reset device",
		[XHCI_TRB_TYPE_FORCE_EVENT]="Force event",
		[XHCI_TRB_TYPE_GET_BANDWIDTH]="Get bandwidth",
		[XHCI_TRB_TYPE_GET_LATENCY_TOLERANCE]="Get latency tolerance",
		[XHCI_TRB_TYPE_GET_PORT_BANDWIDTH]="Get port bandwidth",
		[XHCI_TRB_TYPE_FORCE_HEADER]="Force header",
		[XHCI_TRB_TYPE_NOP_CMD]="Nop",
		[XHCI_TRB_TYPE_GET_EXTENDED_CAP]="Get extended capability",
		[XHCI_TRB_TYPE_SET_EXTENDED_CAP]="Set extended capability",
		[XHCI_EVENT_TRB_TYPE_TRANSFER_EVENT]="Event transfer event",
		[XHCI_EVENT_TRB_TYPE_CMD_COMPLETION]="Command completion",
		[XHCI_EVENT_TRB_TYPE_PORT_STATUS_CHANGE]="Port status change",
		[XHCI_EVENT_TRB_TYPE_BANDWIDTH_REQUEST]="Bandwidth request",
		[XHCI_EVENT_TRB_TYPE_DOORBELL_EVENT]="Doorbell event",
		[XHCI_EVENT_TRB_TYPE_HC_ERROR]="Host controller error",
		[XHCI_EVENT_TRB_TYPE_DEV_NOTIF]="Device notificiation", 
		[XHCI_EVENT_TRB_TYPE_MFINDEX_WRAP]="Microframe index wrap",
	};
	const unsigned char* pName = mapping[type];
	if (!pName)
		pName = "Invalid type";
	*ppName = pName;
	return 0;
}
int xhci_get_error_name(uint64_t error_code, const unsigned char** ppName){
	if (!ppName)
		return -1;
	if (error_code>0x1A){
		*ppName = "Unknown error";
		return 0;
	}
	static const unsigned char* mapping[]={
		[XHCI_COMPLETION_CODE_INVALID]="Unknown error",
		[XHCI_COMPLETION_CODE_SUCCESS]="Success",
		[XHCI_COMPLETION_CODE_DMA_ERROR]="Dma access error",
		[XHCI_COMPLETION_CODE_OVERFLOW]="Overflow", 
		[XHCI_COMPLETION_CODE_BUS_ERROR]="Bus error",
		[XHCI_COMPLETION_CODE_TRB_ERROR]="Trb error",
		[XHCI_COMPLETION_CODE_STALL_ERROR]="Stall error",
		[XHCI_COMPLETION_CODE_RESOURCE_ERROR]="Resource error",
		[XHCI_COMPLETION_CODE_BANDWIDTH_ERROR]="Bandwidth error",
		[XHCI_COMPLETION_CODE_NO_SLOTS_AVAILABLE]="No slots available", 
		[XHCI_COMPLETION_CODE_SLOT_NOT_ENABLED]="Slot not enabled",
		[XHCI_COMPLETION_CODE_ENDPOINT_NOT_ENABLED]="Endpoint not enabled",
		[XHCI_COMPLETION_CODE_SHORT_PACKET]="Short packet",
		[XHCI_COMPLETION_CODE_PARAM_ERROR]="Param error",
		[XHCI_COMPLETION_CODE_CONTEXT_STATE_ERROR]="Context state error",
		[XHCI_COMPLETION_CODE_EVENT_RING_FULL]="Event ring fulll",
	};
	const unsigned char* pName = mapping[error_code];
	if (!pName)
		pName = "Unknown error";
	*ppName = pName;
	return 0;
}
int xhci_interrupter(void){
	volatile struct xhci_trb* pEventTrb = (volatile struct xhci_trb*)0x0;
	if (xhci_get_event_trb(&pEventTrb)!=0){
		printf("failed to get current event TRB\r\n");
		xhci_send_ack(0);
		xhci_update_dequeue_trb();
		return -1;
	}
	struct xhci_trb eventTrb = *pEventTrb;
	uint64_t pTrb_phys = eventTrb.event.trb_ptr;	
	if (!pTrb_phys){
		printf("event TRB not linked to TRB\r\n");
		xhci_send_ack(0);
		xhci_update_dequeue_trb();
		return -1;
	}
	uint64_t pEventTrb_phys = 0;
	if (xhci_get_event_trb_phys(&pEventTrb_phys)!=0){
		printf("failed to get event TRB physical address\r\n");
		xhci_send_ack(0);
		xhci_update_dequeue_trb();
		return -1;
	}
	uint8_t acknowledged = 0;
	switch (eventTrb.event.control.type){	
		case XHCI_EVENT_TRB_TYPE_CMD_COMPLETION:{
			uint64_t trbIndex = (uint64_t)((pTrb_phys-xhciInfo.pCmdRingInfo->pRingBuffer_phys)/sizeof(struct xhci_trb));
			struct xhci_cmd_desc* pCmdDesc = xhciInfo.pCmdRingInfo->pCmdDescList+trbIndex;
			pCmdDesc->eventTrb = eventTrb;
			pCmdDesc->cmdComplete = 1;
			break;
		}
		case XHCI_EVENT_TRB_TYPE_TRANSFER_EVENT:{
			struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;	
			struct xhci_transfer_ring_info* pTransferRingInfo = (struct xhci_transfer_ring_info*)0x0;
			uint8_t slotId = eventTrb.event_transfer.slot_id;
			uint8_t endpointIndex = (eventTrb.event_transfer.endpoint_id==1) ? 0 : eventTrb.event_transfer.endpoint_id;
			if (xhci_get_transfer_ring(slotId, endpointIndex, &pTransferRingInfo)!=0){
				printf("failed to get transfer ring info\r\n");
				break;
			}
			uint64_t trbIndex = (pTrb_phys-pTransferRingInfo->pRingBuffer_phys)/sizeof(struct xhci_trb);
			if (trbIndex>XHCI_MAX_TRANSFER_TRB_ENTRIES){
				printf("invalid TRB index: %d\r\n", trbIndex);
				break;
			}
			if (xhci_get_transfer_desc(slotId, endpointIndex, trbIndex, &pTransferDesc)!=0){
				printf("failed to get transfer desc\r\n");
				break;
			}
			pTransferDesc->eventTrb = eventTrb;
			pTransferDesc->transferComplete = 1;
			if (pTransferDesc->completionFunc){
				pTransferDesc->completionFunc(pTransferDesc);
				xhci_send_ack(0);
				xhci_update_dequeue_trb();
				return 0;
			}
			break;				       
		}
		case XHCI_EVENT_TRB_TYPE_PORT_STATUS_CHANGE:{
			volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
			struct xhci_device* pDevice = (struct xhci_device*)0x0;
			uint8_t portIndex = eventTrb.port_status_change.port_id-1;
			if (xhci_get_port(portIndex, &pPort)!=0){
				printf("failed to get port\r\n");
				break;
			}
			if (xhci_get_device(portIndex, &pDevice)!=0){
				printf("failed to get device\r\n");
				break;
			}
			struct xhci_port_status portStatus = pPort->port_status;
			if (portStatus.connection_status_change&&portStatus.connection_status){
				struct xhci_device* pNewDevice = (struct xhci_device*)0x0;
				xhci_send_ack(0);
				xhci_update_dequeue_trb();
				acknowledged = 1;
				lapic_send_eoi();
				__asm__ volatile("sti");
				if (xhci_init_device(portIndex, &pNewDevice)!=0){
					printf("failed to initialize device\r\n");
					break;
				}
				__asm__ volatile("cli");
				printf("device at port %d connected\r\n", portIndex);
			}
			if (portStatus.connection_status_change&&!portStatus.connection_status){
				xhci_send_ack(0);
				xhci_update_dequeue_trb();
				acknowledged = 1;
				lapic_send_eoi();
				__asm__ volatile("sti");
				if (xhci_deinit_device(pDevice)!=0){
					printf("failed to deinitialize device\r\n");
					break;
				}
				__asm__ volatile("cli");
				printf("device at port %d disconnected\r\n", portIndex);
			}
			if (portStatus.over_current_change){
				xhci_send_ack(0);
				xhci_update_dequeue_trb();
				acknowledged = 1;
				lapic_send_eoi();
				__asm__ volatile("sti");
				if (xhci_deinit_device(pDevice)!=0){
					printf("failed to deintialize device\r\n");
					break;
				}
				__asm__ volatile("cli");
				printf("device at port %d over current\r\n", portIndex);
			}
			portStatus.connection_status_change = 1;
			portStatus.port_reset_change = 1;
			portStatus.port_enable_change = 1;
			portStatus.over_current_change = 1;
			if (portStatus.port_speed>XHCI_PORT_SPEED_LOW)
				portStatus.warm_port_reset_change = 1;
			portStatus.port_link_state_change = 1;
			pPort->port_status = portStatus;
			break;					    
		};
	}
	if (!acknowledged){
		xhci_send_ack(0);
		xhci_update_dequeue_trb();
	}
	return 0;
}
int xhci_dump_interrupter(uint64_t interrupter_id){
	volatile struct xhci_interrupter* pInt = (volatile struct xhci_interrupter*)0x0;
	if (xhci_get_interrupter_base(interrupter_id, &pInt)!=0)
		return -1;
	struct xhci_interrupter_iman iman = pInt->interrupt_management;
	uint64_t event_segment_table_base = 0;
	uint64_t event_segment_table_entry_count = pInt->table_size;
	xhci_read_qword(&pInt->table_base, &event_segment_table_base);
	if (!event_segment_table_base){
		printf("invalid segment table base\r\n");
		return -1;
	}
	volatile uint64_t* pSegmentTable = (volatile uint64_t*)0x0;
	uint64_t segmentTablePages = align_up(event_segment_table_entry_count*sizeof(uint64_t), PAGE_SIZE)/PAGE_SIZE;
	if (virtualGetSpace((uint64_t*)&pSegmentTable, segmentTablePages)!=0){
		return -1;
	}
	if (virtualMapPages(event_segment_table_base, (uint64_t)pSegmentTable, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, segmentTablePages, 1, 0, PAGE_TYPE_MMIO)!=0){
		return -1;
	}
	printf("interrupt enable: %s\r\n", (iman.interrupt_enable) ? "true" : "false");
	printf("interrupt pending: %s\r\n", (iman.interrupt_pending) ? "true" : "false");
	printf("event segment table entry count: %d\r\n", event_segment_table_entry_count);
	for (uint64_t i = 0;i<event_segment_table_entry_count;i++){
		uint64_t segmentEntry_pa = (uint64_t)pSegmentTable[i];
		if (!segmentEntry_pa){
			printf("invalid segment entry\r\n");
			virtualUnmapPages((uint64_t)pSegmentTable, segmentTablePages, 0);
			return -1;
		}
		volatile struct xhci_segment_table_entry* pSegmentEntry = (volatile struct xhci_segment_table_entry*)0x0;
		if (virtualGetSpace((uint64_t*)&pSegmentEntry, 1)!=0)
			return -1;
		if (virtualMapPage(segmentEntry_pa, (uint64_t)pSegmentEntry, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0)
			return -1;
		printf("event ring segment base: %p\r\n", pSegmentEntry->base);
		printf("event ring max TRB count: %d\r\n", pSegmentEntry->size);
		virtualUnmapPage((uint64_t)pSegmentEntry, 0);
	}
	virtualUnmapPages((uint64_t)pSegmentTable, segmentTablePages, 0);
	return 0;
}
int xhci_init_device_context_list(void){
	uint64_t* pDeviceContextList = (uint64_t*)0x0;
	if (virtualAllocPage((uint64_t*)&pDeviceContextList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	memset((void*)pDeviceContextList, 0, PAGE_SIZE);
	uint64_t pDeviceContextList_phys = 0;
	if (virtualToPhysical((uint64_t)pDeviceContextList, &pDeviceContextList_phys)!=0){
		virtualFreePage((uint64_t)pDeviceContextList, 0);
		return -1;
	}
	xhciInfo.deviceContextListInfo.pContextList = pDeviceContextList;
	xhciInfo.deviceContextListInfo.pContextList_phys = pDeviceContextList_phys;
	xhciInfo.deviceContextListInfo.maxDeviceCount = XHCI_MAX_DEVICE_COUNT;
	xhci_write_qword((uint64_t*)&xhciInfo.pOperational->device_context_base_list_ptr, (uint64_t)pDeviceContextList_phys);
	return 0;
}
int xhci_get_driver_cycle_state(unsigned char* pCycleState){
	if (!pCycleState)
		return -1;
	*pCycleState = xhciInfo.pCmdRingInfo->cycle_state;
	return 0;
}
int xhci_get_hc_cycle_state(unsigned char* pCycleState){
	if (!pCycleState)
		return -1;
	union xhci_cmd_ring_ctrl cmdRingControl = xhciInfo.pOperational->cmd_ring_ctrl;
	*pCycleState = cmdRingControl.ring_cycle_state;
	return 0;
}
int xhci_init_scratchpad(void){
	struct xhci_structure_param1 param1 = xhciInfo.pCapabilities->structure_param1;
	uint64_t scratchpadPages = 0;
	return 0;
}
int xhci_get_extended_cap(uint8_t cap_id, volatile struct xhci_extended_cap_hdr** ppCapHeader){
	if (!ppCapHeader)
		return -1;
	volatile struct xhci_extended_cap_hdr* pCurrentHeader = xhciInfo.pExtendedCapList;
	while (1){
		if (pCurrentHeader->cap_id!=cap_id){
			if (!pCurrentHeader->next_offset)
				return -1;
			pCurrentHeader = (volatile struct xhci_extended_cap_hdr*)(((uint64_t)pCurrentHeader)+(pCurrentHeader->next_offset<<2));
			continue;
		}
		*ppCapHeader = pCurrentHeader;
		return 0;
	}
	return -1;
}
int xhci_enable_slot(uint64_t* pSlotId){
	if (!pSlotId)
		return -1;
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	struct xhci_trb cmdTrb = {0};
	memset((void*)&cmdTrb, 0, sizeof(struct xhci_trb));
	cmdTrb.command.control.type = XHCI_TRB_TYPE_ENABLE_SLOT;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, cmdTrb, &pCmdDesc)!=0)
		return -1;
	xhci_start();
	xhci_ring(0);
	while (!pCmdDesc->cmdComplete){};
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name((uint64_t)eventTrb.event.completion_code, &errorName);
		printf("failed to enable slot (%s)\r\n", errorName);
		return -1;
	}
	*pSlotId = (uint64_t)eventTrb.enable_slot_event.slot_id;
	return 0;
}
int xhci_disable_slot(uint64_t slotId){
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	struct xhci_trb cmdTrb = {0};
	memset((void*)&cmdTrb, 0, sizeof(struct xhci_trb));
	cmdTrb.disable_slot_cmd.type = XHCI_TRB_TYPE_DISABLE_SLOT;
	cmdTrb.disable_slot_cmd.slot_id = (uint8_t)slotId;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, cmdTrb, &pCmdDesc)!=0)
		return -1;
	xhci_start();
	xhci_ring(0);
	while (!pCmdDesc->cmdComplete){};
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;	
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name((uint64_t)eventTrb.event.completion_code, &errorName);
		printf("failed to disable slot (%s)\r\n", errorName);
		return -1;
	}
	return 0;
}
KAPI int xhci_init_device(uint8_t port, struct xhci_device** ppDevice){
	if (!ppDevice)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	volatile struct xhci_cap_mmio* pCapRegisters = xhciInfo.pCapabilities;
	struct xhci_cap_param0 param0 = pCapRegisters->cap_param0;
	uint64_t contextSize = param0.context_size ? sizeof(struct xhci_slot_context64) : sizeof(struct xhci_slot_context32);
	if (xhci_reset_port(port)!=0){
		printf("failed to reset PORT%d\r\n", port);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (xhci_enable_port(port)!=0){
		printf("failed to enable PORT%d\r\n", port);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	uint64_t pDeviceContext = 0;	
	uint64_t pDeviceContext_phys = 0;
	if (virtualAllocPage((uint64_t*)&pDeviceContext, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate device context\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (virtualToPhysical((uint64_t)pDeviceContext, &pDeviceContext_phys)!=0){
		printf("failed to get physical address of device context\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	memset((void*)pDeviceContext, 0, PAGE_SIZE);
	volatile struct xhci_input_context32* pInputContext = (volatile struct xhci_input_context32*)pDeviceContext;	
	volatile struct xhci_slot_context32* pSlotContext = (volatile struct xhci_slot_context32*)(pDeviceContext+contextSize);
	volatile struct xhci_endpoint_context32* pControlContext = (volatile struct xhci_endpoint_context32*)0x0;
	pControlContext = (volatile struct xhci_endpoint_context32*)(((uint64_t)pSlotContext)+contextSize);	
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(port, &pDevice)!=0){
		virtualFreePage((uint64_t)pDeviceContext, 0);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	memset((void*)pDevice, 0, sizeof(struct xhci_device));
	pDevice->port = port;
	pDevice->deviceContext.pDeviceContext = pDeviceContext;
	pDevice->deviceContext.pDeviceContext_phys = pDeviceContext_phys;
	struct xhci_transfer_ring_info* pTransferRingInfo = (struct xhci_transfer_ring_info*)0x0;
	uint64_t slotId = 0;
	if (xhci_enable_slot(&slotId)!=0){
		printf("failed to enable slot\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	pDevice->deviceContext.slotId = slotId;
	if (xhci_init_transfer_ring(pDevice, 0, 0, 0x0, &pTransferRingInfo)!=0){
		printf("failed to initialize transfer ring\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_disable_slot(slotId);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	pDevice->pControlTransferRingInfo = pTransferRingInfo;
	uint8_t portSpeed = 0;
	xhci_get_port_speed(port, &portSpeed);
	static uint16_t speedmap[]={
		[XHCI_PORT_SPEED_FULL]=32,
		[XHCI_PORT_SPEED_LOW]=8,
		[XHCI_PORT_SPEED_HIGH]=64,
		[XHCI_PORT_SPEED_SUPER]=512,
	};
	uint16_t initMaxPacketSize = 512;
	if (portSpeed<=4)
		initMaxPacketSize = speedmap[portSpeed];
	pSlotContext->speed = portSpeed;
	pSlotContext->context_entries = 1;
	pSlotContext->root_hub_port_num = port+1;
	pControlContext->error_count = 3;
	pControlContext->type = 4;
	pControlContext->max_packet_size = initMaxPacketSize;
	pControlContext->dequeue_ptr = (uint64_t)pTransferRingInfo->pRingBuffer_phys;
	pControlContext->average_trb_len = 8;
	if (pTransferRingInfo->cycle_state)
		pControlContext->dequeue_ptr|=(1<<0);
	else
		pControlContext->dequeue_ptr&=~(1<<0);
	pControlContext->average_trb_len = initMaxPacketSize;
	pInputContext->add_flags|=(1<<0);
	pInputContext->add_flags|=(1<<1);
	xhciInfo.deviceContextListInfo.pContextList[slotId] = (uint64_t)pDeviceContext_phys;
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	struct xhci_trb eventTrb = {0};
	memset((void*)&eventTrb, 0, sizeof(struct xhci_trb));
	if (xhci_address_device(pDevice, 0, &eventTrb)!=0){
		printf("failed to address device\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	const unsigned char* completionStatusName = "Unknown status";
	xhci_get_error_name(eventTrb.event.completion_code, &completionStatusName);	
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		printf("failed to address device (%s)\r\n", completionStatusName);
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	pSlotContext->speed = portSpeed;
	pSlotContext->context_entries = 1;
	pSlotContext->root_hub_port_num = port+1;
	pControlContext->error_count = 3;
	pControlContext->type = 4;
	pControlContext->dequeue_ptr = (uint64_t)pTransferRingInfo->pRingBuffer_phys;
	pControlContext->average_trb_len = initMaxPacketSize;
	pControlContext->type = XHCI_ENDPOINT_TYPE_CONTROL;
	if (pTransferRingInfo->cycle_state)
		pControlContext->dequeue_ptr|=(1<<0);
	else
		pControlContext->dequeue_ptr&=~(1<<0);
	struct xhci_usb_device_desc deviceDescriptor = {0};	
	struct xhci_usb_config_desc* pConfigDescriptor = (struct xhci_usb_config_desc*)0x0;
	if (virtualAllocPage((uint64_t*)&pConfigDescriptor, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate config descriptor\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	if (xhci_get_descriptor(pDevice, pTransferRingInfo, XHCI_USB_DESC_DEVICE, 0x0, (unsigned char*)&deviceDescriptor, 18, &eventTrb)!=0){
		printf("failed to get descriptor\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return-1;
	}
	if (xhci_get_descriptor(pDevice, pTransferRingInfo, XHCI_USB_DESC_CONFIG, 0x0, (unsigned char*)pConfigDescriptor, sizeof(struct xhci_usb_config_desc), &eventTrb)!=0){
		printf("failed to get config descriptor\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (xhci_set_configuration(pDevice, pTransferRingInfo, pConfigDescriptor->configValue, &eventTrb)!=0){
		printf("failed to set configuration\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct xhci_usb_string_desc manufacturerName = {0};
	struct xhci_usb_string_desc productName = {0};
	struct xhci_usb_string_desc serialName = {0};
	if (xhci_get_descriptor(pDevice, pTransferRingInfo, XHCI_USB_DESC_STRING, deviceDescriptor.manufacturerIndex, (unsigned char*)&manufacturerName, 255, &eventTrb)!=0){
		printf("failed to get manufacturer name\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (xhci_get_descriptor(pDevice, pTransferRingInfo, XHCI_USB_DESC_STRING, deviceDescriptor.productIndex, (unsigned char*)&productName, 255, &eventTrb)!=0){
		printf("failed to get product name\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return-1;
	}
	if (xhci_get_descriptor(pDevice, pTransferRingInfo, XHCI_USB_DESC_STRING, deviceDescriptor.serialIndex, (unsigned char*)&serialName, 255, &eventTrb)!=0){
		printf("failed to get serial name\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	lprintf(L"manufacturer name: %s\r\n", manufacturerName.string);
	lprintf(L"product name: %s\r\n", productName.string);
	pDevice->deviceContext.pInputContext = pInputContext;
	pControlContext->max_packet_size = deviceDescriptor.maxPacketSize;
	pInputContext->add_flags = (1<<0);	
	pInputContext->drop_flags = 0x0;
	if (xhci_evaluate_context(pDevice, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to evaluate context (%s)\r\n", errorName);
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	uint64_t configOffset = 0;
	uint64_t configTotalLength = pConfigDescriptor->totalLength-pConfigDescriptor->header.length;
	if (configTotalLength>2048){
		printf("config entry table too long!\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct xhci_interface_desc* pInterfaceDescList = (struct xhci_interface_desc*)0x0;
	if (virtualAllocPage((uint64_t*)&pInterfaceDescList, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate interface descriptor list\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	uint32_t add_flags = (1<<0);
	pDevice->pConfigDescriptor = pConfigDescriptor;
	pDevice->pInterfaceDescList = pInterfaceDescList;
	pDevice->maxInterfaceDescCount = PAGE_SIZE/sizeof(struct xhci_interface_desc);
	struct xhci_interface_desc* pCurrentInterface = (struct xhci_interface_desc*)0x0;
	while (configOffset<configTotalLength){
		struct xhci_usb_desc_header* pDescHeader = (struct xhci_usb_desc_header*)(pConfigDescriptor->data+configOffset);
		switch (pDescHeader->descriptorType){
			case XHCI_USB_DESC_INTERFACE:{
				struct xhci_usb_interface_desc* pDesc = (struct xhci_usb_interface_desc*)pDescHeader;
				struct xhci_interface_desc interfaceDesc = {0};
				struct xhci_endpoint_desc* pEndpointDescList = (struct xhci_endpoint_desc*)0x0;
				if (virtualAllocPage((uint64_t*)&pEndpointDescList, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
					printf("failed to allocate endpoint descriptor\r\n");
					xhci_disable_slot(slotId);
					virtualFreePage((uint64_t)pDeviceContext, 0);
					virtualFreePage((uint64_t)pInterfaceDescList, 0);
					virtualFreePage((uint64_t)pConfigDescriptor, 0);
					xhci_deinit_transfer_ring(pTransferRingInfo);
					mutex_unlock_isr_safe(&mutex);
					return -1;
				}
				memset((void*)&interfaceDesc, 0, sizeof(struct xhci_interface_desc));
				interfaceDesc.usbInterfaceDesc = *pDesc;
				interfaceDesc.pEndpointDescList = pEndpointDescList;
				interfaceDesc.endpointCount = 0;
				*(pInterfaceDescList+pDevice->interfaceCount) = interfaceDesc;
				pCurrentInterface = pInterfaceDescList+pDevice->interfaceCount;
				pDevice->interfaceCount++;	
				break;
			}
			case XHCI_USB_DESC_ENDPOINT:{
				if (!pCurrentInterface){
					printf("no device interface before endpoint descriptor!\r\n");
					xhci_disable_slot(slotId);
					virtualFreePage((uint64_t)pDeviceContext, 0);
					virtualFreePage((uint64_t)pInterfaceDescList, 0);
					virtualFreePage((uint64_t)pConfigDescriptor, 0);
					xhci_deinit_transfer_ring(pTransferRingInfo);
					mutex_unlock_isr_safe(&mutex);
					return -1;
				}
				struct xhci_usb_endpoint_desc* pDesc = (struct xhci_usb_endpoint_desc*)pDescHeader;
				uint8_t endpointDirection = pDesc->endpointAddress&(1<<7) ? 1 : 0;
				uint8_t endpointIndex = XHCI_EP_ID_INDEX(pDesc->endpointAddress);
				uint8_t transferType = (uint8_t)(pDesc->attributes&0x3);
				uint8_t endpointType = 0x0;
				switch (transferType){
					case XHCI_TRANSFER_TYPE_CONTROL:{
						endpointType = XHCI_ENDPOINT_TYPE_CONTROL;			
						break;
					};	
					case XHCI_TRANSFER_TYPE_ISOCH:{
						endpointType = (endpointDirection) ? XHCI_ENDPOINT_TYPE_ISOCH_IN : XHCI_ENDPOINT_TYPE_ISOCH_OUT;		      
						break;			      
					};
					case XHCI_TRANSFER_TYPE_BULK:{
						endpointType = (endpointDirection) ? XHCI_ENDPOINT_TYPE_BULK_IN : XHCI_ENDPOINT_TYPE_BULK_OUT;
						break;			     
					};
					case XHCI_TRANSFER_TYPE_INT:{
						endpointType = (endpointDirection) ? XHCI_ENDPOINT_TYPE_INT_IN : XHCI_ENDPOINT_TYPE_INT_OUT;
						break;			    
					};
				}
				if (!endpointIndex||endpointIndex>30){
					printf("invalid endpoint index\r\n");
					xhci_disable_slot(slotId);
					virtualFreePage((uint64_t)pDeviceContext, 0);
					virtualFreePage((uint64_t)pInterfaceDescList, 0);
					virtualFreePage((uint64_t)pConfigDescriptor, 0);
					xhci_deinit_transfer_ring(pTransferRingInfo);
					mutex_unlock_isr_safe(&mutex);
					return -1;
				}
				struct xhci_transfer_ring_info* pEndpointTransferRingInfo = (struct xhci_transfer_ring_info*)0x0;
				if (xhci_init_transfer_ring(pDevice, pCurrentInterface->usbInterfaceDesc.interfaceNumber, endpointIndex, pCurrentInterface->endpointCount, &pEndpointTransferRingInfo)!=0){
					printf("failed to initialize transfer ring\r\n");
					xhci_disable_slot(slotId);
					virtualFreePage((uint64_t)pDeviceContext, 0);
					virtualFreePage((uint64_t)pInterfaceDescList, 0);
					virtualFreePage((uint64_t)pConfigDescriptor, 0);
					xhci_deinit_transfer_ring(pTransferRingInfo);
					mutex_unlock_isr_safe(&mutex);
					return -1;
				}
				struct xhci_endpoint_desc* pEndpointDesc = (struct xhci_endpoint_desc*)(pCurrentInterface->pEndpointDescList+pCurrentInterface->endpointCount);
				struct xhci_endpoint_context32* pEndpointContext = (struct xhci_endpoint_context32*)(((uint64_t)pControlContext)+(contextSize*(endpointIndex-1)));
				pEndpointContext->dequeue_ptr = (uint64_t)pEndpointTransferRingInfo->pRingBuffer_phys;
				if (pEndpointTransferRingInfo->cycle_state)
					pEndpointContext->dequeue_ptr|=(1<<0);
				else
					pEndpointContext->dequeue_ptr&=~(1<<0);
				pEndpointContext->state = 0x0;
				pEndpointContext->type = endpointType;
				pEndpointContext->error_count = 3;
				pEndpointContext->max_packet_size = pDesc->maxPacketSize;
				pEndpointContext->average_trb_len = pDesc->maxPacketSize;
				pEndpointContext->max_esit_payload_low = (uint16_t)(pDesc->maxPacketSize&0xFFFF);
				pEndpointContext->max_esit_payload_high = (uint16_t)((pDesc->maxPacketSize>>16)&0xFFFF);
				pEndpointDesc->pEndpointContext = pEndpointContext;
				pEndpointDesc->endpointIndex = endpointIndex;
				pEndpointDesc->endpointDirection = endpointDirection;
				pEndpointDesc->pTransferRingInfo = pEndpointTransferRingInfo;
				if (endpointIndex+1>pSlotContext->context_entries){
					pSlotContext->context_entries = endpointIndex+1;
				}
				add_flags|=(1<<(endpointIndex));
				pCurrentInterface->endpointCount++;
				break;
			}
			case XHCI_USB_DESC_HID:{
				if (!pCurrentInterface){
					printf("no interface descriptor before HID descriptor!\r\n");
					xhci_disable_slot(slotId);
					virtualFreePage((uint64_t)pDeviceContext, 0);
					virtualFreePage((uint64_t)pInterfaceDescList, 0);
					virtualFreePage((uint64_t)pConfigDescriptor, 0);
					xhci_deinit_transfer_ring(pTransferRingInfo);
					mutex_unlock(&mutex);
					return -1;
				}
				struct xhci_usb_hid_desc* pDesc = (struct xhci_usb_hid_desc*)pDescHeader;
				pCurrentInterface->pHidDescriptor = pDesc;
				break;		       
			}
		}
		configOffset+=pDescHeader->length;
	}
	pInputContext->drop_flags = 0x0;
	pInputContext->add_flags = add_flags;
	if (xhci_configure_endpoint(pDevice, &eventTrb)!=0){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to configure endpoint (%s)\r\n", errorName);
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		virtualFreePage((uint64_t)pInterfaceDescList, 0);
		virtualFreePage((uint64_t)pConfigDescriptor, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	for (uint64_t i = 0;i<pDevice->interfaceCount;i++){
		struct xhci_interface_desc* pInterfaceDesc = pDevice->pInterfaceDescList+i;
		uint8_t interfaceClass = pInterfaceDesc->usbInterfaceDesc.interfaceClass;
		uint8_t interfaceSubClass = pInterfaceDesc->usbInterfaceDesc.interfaceSubClass;
		printf("interface %d\r\n", i);
		pInterfaceDesc->driverId = 0xFFFFFFFFFFFFFFFF;
		if (xhci_set_protocol(pDevice, pTransferRingInfo, i, 0, &eventTrb)!=0){
			printf("failed to set protocol\r\n");
			xhci_disable_slot(slotId);
			virtualFreePage((uint64_t)pDeviceContext, 0);
			virtualFreePage((uint64_t)pInterfaceDescList, 0);
			virtualFreePage((uint64_t)pConfigDescriptor, 0);
			xhci_deinit_transfer_ring(pTransferRingInfo);
			mutex_unlock_isr_safe(&mutex);
			return -1;
	 	}
		for (uint64_t endpoint_id = 0;endpoint_id<pInterfaceDesc->endpointCount;endpoint_id++){
			struct xhci_endpoint_desc* pEndpointDesc = pInterfaceDesc->pEndpointDescList+endpoint_id;
			struct xhci_endpoint_context32* pEndpointContext = pEndpointDesc->pEndpointContext;
			printf("    endpoint type: 0x%x\r\n", pEndpointContext->type);
			printf("    endpoint state: 0x%x\r\n", pEndpointContext->state);
		}
		struct usb_driver_desc* pCurrentDriverDesc = (struct usb_driver_desc*)0x0;
		while (!usb_get_next_driver_desc(pCurrentDriverDesc, &pCurrentDriverDesc)){
			if (pCurrentDriverDesc->interfaceClass!=interfaceClass||pCurrentDriverDesc->interfaceSubClass!=interfaceSubClass)
				continue;
			if (usb_register_interface(port, i, pCurrentDriverDesc->driverId)!=0){
				printf("failed to register port %d interface %d with driver with ID%d\r\n", port, i, pCurrentDriverDesc->driverId);
				continue;
			}
			pInterfaceDesc->driverId = pCurrentDriverDesc->driverId;
			break;
		}
		if (pInterfaceDesc->driverId!=0xFFFFFFFFFFFFFFFF)
			continue;
		pDevice->unresolvedInterfaceCount++;
	}
	if (xhciInfo.pLastDevice){
		if (xhciInfo.pLastDevice->port<port){
			xhciInfo.pLastDevice->pFlink = pDevice;
			pDevice->pBlink = xhciInfo.pLastDevice;
			xhciInfo.pLastDevice = pDevice;
		}
	}	
	if (!xhciInfo.pFirstDevice){
		xhciInfo.pFirstDevice = pDevice;
		xhciInfo.pLastDevice = pDevice;
	}
	*ppDevice = pDevice;	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int xhci_deinit_device(struct xhci_device* pDevice){
	if (!pDevice)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct xhci_device_context_desc* pContextDesc = &pDevice->deviceContext;
	for (uint64_t i = 0;i<pDevice->interfaceCount;i++){
		struct xhci_interface_desc* pInterfaceDesc = pDevice->pInterfaceDescList+i;
		if (!pInterfaceDesc->pEndpointDescList)
			continue;
		if (pInterfaceDesc->driverId!=0xFFFFFFFFFFFFFFFF)
			usb_unregister_interface(pDevice->port, i, pInterfaceDesc->driverId);
		for (uint64_t endpoint_index = 0;endpoint_index<pInterfaceDesc->endpointCount;endpoint_index++){
			struct xhci_endpoint_desc* pEndpointDesc = pInterfaceDesc->pEndpointDescList+endpoint_index;
			if (pEndpointDesc->pTransferRingInfo)
				xhci_deinit_transfer_ring(pEndpointDesc->pTransferRingInfo);
		}
		virtualFreePage((uint64_t)pInterfaceDesc->pEndpointDescList, 0);
	}	
	if (pDevice->pConfigDescriptor)
		virtualFreePage((uint64_t)pDevice->pConfigDescriptor, 0);	
	if (pDevice->pInterfaceDescList)
		virtualFreePage((uint64_t)pDevice->pInterfaceDescList, 0);	
	if (pContextDesc->pDeviceContext)
		virtualFreePage((uint64_t)pContextDesc->pDeviceContext, 0);
	if (pContextDesc->slotId)
		xhci_disable_slot(pContextDesc->slotId);
	if (pDevice->pBlink)
		pDevice->pBlink->pFlink = pDevice->pFlink;
	if (pDevice->pFlink)
		pDevice->pFlink->pBlink = pDevice->pBlink;
	if (pDevice==xhciInfo.pLastDevice)
		xhciInfo.pLastDevice = pDevice->pBlink;
	if (pDevice==xhciInfo.pFirstDevice)
		xhciInfo.pFirstDevice = pDevice->pFlink;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int xhci_get_endpoint_context(struct xhci_device_context_desc* pContextDesc, uint64_t endpoint_index, volatile struct xhci_endpoint_context32** ppEndPointContext){
	if (!pContextDesc||!ppEndPointContext)
		return -1;
	if (pContextDesc->shortContext){
		volatile struct xhci_device_context32* pDeviceContext = (volatile struct xhci_device_context32*)pContextDesc->pDeviceContext;
		*ppEndPointContext = pDeviceContext->endpointList+endpoint_index;
		return 0;
	}	
	volatile struct xhci_device_context64* pDeviceContext = (volatile struct xhci_device_context64*)pContextDesc->pDeviceContext;
	*ppEndPointContext = (volatile struct xhci_endpoint_context32*)(pDeviceContext->endpointList+endpoint_index);
	return 0;
}
KAPI int xhci_address_device(struct xhci_device* pDevice, uint8_t block_set_address, struct xhci_trb* pEventTrb){
	if (!pDevice)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	struct xhci_trb cmdTrb = {0};
	memset((void*)&cmdTrb, 0, sizeof(struct xhci_trb));
	cmdTrb.address_device_cmd.type = XHCI_TRB_TYPE_ADDRESS_DEVICE;	
	cmdTrb.address_device_cmd.input_context_ptr = pDevice->deviceContext.pDeviceContext_phys;
	cmdTrb.address_device_cmd.slotId = pDevice->deviceContext.slotId;
	cmdTrb.address_device_cmd.block_set_address = block_set_address ? 1 : 0;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, cmdTrb, &pCmdDesc)!=0){
		printf("failed to push address device TRB to cmd ring\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	xhci_start();
	xhci_ring(0);
	while (!pCmdDesc->cmdComplete){};
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to address device (%s)\r\n", errorName);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int xhci_evaluate_context(struct xhci_device* pDevice, struct xhci_trb* pEventTrb){
	if (!pDevice)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	struct xhci_trb cmdTrb = {0};
	memset((void*)&cmdTrb, 0, sizeof(struct xhci_trb));
	cmdTrb.evaluate_context_cmd.type = XHCI_TRB_TYPE_EVALUATE_CONTEXT;
	cmdTrb.evaluate_context_cmd.slot_id = pDevice->deviceContext.slotId;
	cmdTrb.evaluate_context_cmd.input_context_ptr = (uint64_t)pDevice->deviceContext.pDeviceContext_phys;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, cmdTrb, &pCmdDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	xhci_start();
	xhci_ring(0);
	while (!pCmdDesc->cmdComplete){};
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		printf("failed to evaluate context\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int xhci_get_descriptor(struct xhci_device* pDevice, struct xhci_transfer_ring_info* pTransferRingInfo, uint8_t type, uint8_t index, unsigned char* pBuffer, uint64_t len, struct xhci_trb* pEventTrb){
	if (!pDevice||!pTransferRingInfo||!pBuffer)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	uint64_t pBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)pBuffer, &pBuffer_phys)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	struct xhci_trb setupTrb = {0};
	memset((void*)&setupTrb, 0, sizeof(struct xhci_trb));
	setupTrb.setup_stage_cmd.type = XHCI_TRB_TYPE_SETUP;
	setupTrb.setup_stage_cmd.requestType.request_target = XHCI_REQUEST_TARGET_DEVICE;
	setupTrb.setup_stage_cmd.requestType.request_direction = XHCI_REQUEST_TRANSFER_DIRECTION_D2H;
	setupTrb.setup_stage_cmd.request = 0x06;
	setupTrb.setup_stage_cmd.value = ((uint16_t)type<<8)|index;
	setupTrb.setup_stage_cmd.length = len;
	setupTrb.setup_stage_cmd.trb_transfer_len = sizeof(uint64_t);
	setupTrb.setup_stage_cmd.transfer_type = 0x03;
	setupTrb.setup_stage_cmd.immediate_data = 1;
	setupTrb.setup_stage_cmd.chain_bit = 1;
	setupTrb.setup_stage_cmd.ioc = 1;
	struct xhci_trb dataTrb = {0};
	memset((void*)&dataTrb, 0, sizeof(struct xhci_trb));
	dataTrb.data_stage_cmd.type = XHCI_TRB_TYPE_DATA;	
	dataTrb.data_stage_cmd.buffer_phys = pBuffer_phys;
	dataTrb.data_stage_cmd.trb_transfer_len = len;
	dataTrb.data_stage_cmd.dir = 1;
	struct xhci_trb statusTrb = {0};
	memset((void*)&statusTrb, 0, sizeof(struct xhci_trb));
	statusTrb.status_stage_cmd.type = XHCI_TRB_TYPE_STATUS;
	statusTrb.status_stage_cmd.ioc = 1;
	struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;
	if (xhci_alloc_transfer(pTransferRingInfo, setupTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		printf("failed to allocate transfer setup TRB\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	if (xhci_alloc_transfer(pTransferRingInfo, dataTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		printf("failed to allocate transfer data TRB\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (xhci_alloc_transfer(pTransferRingInfo, statusTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		printf("failed to allocate transfer status TRB\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	xhci_start();
	xhci_ring_endpoint(pDevice->deviceContext.slotId, 1, 0);
	while (!pTransferDesc->transferComplete){};
	struct xhci_trb eventTrb = pTransferDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		printf("failed to get descriptor\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int xhci_configure_endpoint(struct xhci_device* pDevice, struct xhci_trb* pEventTrb){
	if (!pDevice)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	struct xhci_trb cmdTrb = {0};
	memset((void*)&cmdTrb, 0, sizeof(struct xhci_trb));
	cmdTrb.configure_endpoint_cmd.type = XHCI_TRB_TYPE_CONFIGURE_ENDPOINT;
	cmdTrb.configure_endpoint_cmd.input_context_ptr = (uint64_t)pDevice->deviceContext.pDeviceContext_phys;
	cmdTrb.configure_endpoint_cmd.slot_id = pDevice->deviceContext.slotId;	
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, cmdTrb, &pCmdDesc)!=0){
		printf("failed to allocate configure endpoint command\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	xhci_start();
	xhci_ring(0);
	while (!pCmdDesc->cmdComplete){};
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int xhci_set_configuration(struct xhci_device* pDevice, struct xhci_transfer_ring_info* pTransferRingInfo, uint8_t configValue, struct xhci_trb* pEventTrb){
	if (!pDevice||!pTransferRingInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;
	struct xhci_trb setupTrb = {0};
	memset((void*)&setupTrb, 0, sizeof(struct xhci_trb));
	setupTrb.setup_stage_cmd.type = XHCI_TRB_TYPE_SETUP;
	setupTrb.setup_stage_cmd.requestType.request_target = XHCI_REQUEST_TARGET_DEVICE;
	setupTrb.setup_stage_cmd.requestType.request_direction = XHCI_REQUEST_TRANSFER_DIRECTION_H2D;
	setupTrb.setup_stage_cmd.request = 0x09;	
	setupTrb.setup_stage_cmd.value = (uint16_t)configValue;
	setupTrb.setup_stage_cmd.immediate_data = 1;
	setupTrb.setup_stage_cmd.chain_bit = 0;
	setupTrb.setup_stage_cmd.ioc = 1;
	setupTrb.setup_stage_cmd.trb_transfer_len = 8;
	struct xhci_trb statusTrb = {0};
	memset((void*)&statusTrb, 0, sizeof(struct xhci_trb));
	statusTrb.status_stage_cmd.type = XHCI_TRB_TYPE_STATUS;
	statusTrb.status_stage_cmd.ioc = 1;
	statusTrb.status_stage_cmd.dir = 1;
	if (xhci_alloc_transfer(pTransferRingInfo, setupTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		printf("failed to allocate setup transfer TRB\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (xhci_alloc_transfer(pTransferRingInfo, statusTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		printf("failed to allocate status transfer TRB\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	xhci_start();
	xhci_ring_endpoint(pDevice->deviceContext.slotId, 1, 0);
	while (!pTransferDesc->transferComplete){};
	struct xhci_trb eventTrb = pTransferDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int xhci_get_interface_desc(struct xhci_device* pDevice, uint64_t interfaceId, struct xhci_interface_desc** ppInterfaceDesc){
	if (!pDevice||!ppInterfaceDesc)
		return -1;
	struct xhci_interface_desc* pInterfaceDesc = pDevice->pInterfaceDescList+interfaceId;
	*ppInterfaceDesc = pInterfaceDesc;	
	return 0;
}
KAPI int xhci_get_endpoint_desc(struct xhci_device* pDevice, uint64_t interfaceId, uint64_t endpointId, struct xhci_endpoint_desc** ppEndpointDesc){
	if (!pDevice||!ppEndpointDesc)
		return -1;
	struct xhci_interface_desc* pInterfaceDesc = (struct xhci_interface_desc*)0x0;
	if (xhci_get_interface_desc(pDevice, interfaceId, &pInterfaceDesc)!=0)
		return -1;
	struct xhci_endpoint_desc* pEndpointDesc = pInterfaceDesc->pEndpointDescList+endpointId;
	*ppEndpointDesc = pEndpointDesc;
	return 0;
}
KAPI int xhci_get_endpoint_transfer_ring(struct xhci_device* pDevice, uint64_t interfaceId, uint64_t endpointId, struct xhci_transfer_ring_info** ppTransferRingInfo){
	if (!pDevice||!ppTransferRingInfo)
		return -1;
	struct xhci_interface_desc* pInterfaceDesc = (struct xhci_interface_desc*)0x0;
	if (xhci_get_interface_desc(pDevice, interfaceId, &pInterfaceDesc)!=0)
		return -1;
	struct xhci_endpoint_desc* pEndpointDesc = pInterfaceDesc->pEndpointDescList+endpointId;
	struct xhci_transfer_ring_info* pTransferRingInfo = pEndpointDesc->pTransferRingInfo;
	*ppTransferRingInfo = pTransferRingInfo;
	return 0;
}
KAPI int xhci_set_protocol(struct xhci_device* pDevice, struct xhci_transfer_ring_info* pTransferRingInfo, uint64_t interfaceId, uint16_t protocolId, struct xhci_trb* pEventTrb){
	if (!pDevice||!pTransferRingInfo)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct xhci_interface_desc* pInterfaceDesc = (struct xhci_interface_desc*)0x0;
	if (xhci_get_interface_desc(pDevice, interfaceId, &pInterfaceDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;	
	}
	uint16_t interfaceNumber = pInterfaceDesc->usbInterfaceDesc.interfaceNumber;
	struct xhci_trb setupTrb = {0};
	memset((void*)&setupTrb, 0, sizeof(struct xhci_trb));
	setupTrb.setup_stage_cmd.type = XHCI_TRB_TYPE_SETUP;
	setupTrb.setup_stage_cmd.requestType.request_target = XHCI_REQUEST_TARGET_INTERFACE;
	setupTrb.setup_stage_cmd.requestType.request_type = XHCI_REQUEST_TYPE_STANDARD;
	setupTrb.setup_stage_cmd.requestType.request_direction = XHCI_REQUEST_TRANSFER_DIRECTION_H2D;
	setupTrb.setup_stage_cmd.request = XHCI_REQUEST_TYPE_SET_PROTOCOL;
	setupTrb.setup_stage_cmd.value = protocolId;
	setupTrb.setup_stage_cmd.index = interfaceId;
	setupTrb.setup_stage_cmd.trb_transfer_len = 8;
	setupTrb.setup_stage_cmd.immediate_data = 1;
	setupTrb.setup_stage_cmd.chain_bit = 1;
	setupTrb.setup_stage_cmd.ioc = 1;
	struct xhci_trb statusTrb = {0};
	memset((void*)&statusTrb, 0, sizeof(struct xhci_trb));
	statusTrb.status_stage_cmd.type = XHCI_TRB_TYPE_STATUS;
	statusTrb.status_stage_cmd.ioc = 1;
	statusTrb.status_stage_cmd.dir = 1;
	struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;
	if (xhci_alloc_transfer(pTransferRingInfo, setupTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	if (xhci_alloc_transfer(pTransferRingInfo, statusTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	xhci_start();
	xhci_ring_endpoint(pDevice->deviceContext.slotId, 1, 0);
	while (!pTransferDesc->transferComplete){};
	struct xhci_trb eventTrb = pTransferDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		mutex_unlock_isr_safe(&mutex);
		return -1;	
	}
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
KAPI int xhci_clear_feature(struct xhci_device* pDevice, uint8_t endpointIndex, uint8_t featureId, struct xhci_trb* pEventTrb){
	if (!pDevice)
		return -1;
	uint8_t endpointNumber = (endpointIndex/2)|((endpointIndex%2)<<7);
	struct xhci_trb setupTrb = {0};
	memset((void*)&setupTrb, 0, sizeof(struct xhci_trb));
	setupTrb.setup_stage_cmd.type = XHCI_TRB_TYPE_SETUP;
	setupTrb.setup_stage_cmd.requestType.request_target = XHCI_REQUEST_TARGET_ENDPOINT;
	setupTrb.setup_stage_cmd.requestType.request_direction = XHCI_REQUEST_TRANSFER_DIRECTION_H2D;
	setupTrb.setup_stage_cmd.request = 0x01;	
	setupTrb.setup_stage_cmd.value = featureId;
	setupTrb.setup_stage_cmd.index = endpointNumber;	
	setupTrb.setup_stage_cmd.immediate_data = 1;
	setupTrb.setup_stage_cmd.ioc = 1;
	setupTrb.setup_stage_cmd.trb_transfer_len = 0x08;
	struct xhci_trb statusTrb = {0};
	memset((void*)&statusTrb, 0, sizeof(struct xhci_trb));
	statusTrb.status_stage_cmd.type = XHCI_TRB_TYPE_STATUS;
	statusTrb.status_stage_cmd.ioc = 1;
	struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;
	if (xhci_alloc_transfer(pDevice->pControlTransferRingInfo, setupTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		printf("failed to allocate setup transfer TRB\r\n");
		return -1;
	}	
	if (xhci_alloc_transfer(pDevice->pControlTransferRingInfo, statusTrb, &pTransferDesc, (xhciTransferCompletionFunc)0x0)!=0){
		printf("failed to allocate status transfer TRB\r\n");
		return -1;
	}
	xhci_start();
	xhci_ring_endpoint(pDevice->deviceContext.slotId, 1, 0);
	while (!pTransferDesc->transferComplete){};	
	struct xhci_trb eventTrb = pTransferDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		printf("failed to clear feature\r\n");
		return -1;
	}
	return 0;
}
KAPI int xhci_reset_endpoint(struct xhci_device* pDevice, uint8_t interfaceId, uint8_t endpointId, struct xhci_trb* pEventTrb){
	if (!pDevice)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct xhci_endpoint_desc* pEndpointDesc = (struct xhci_endpoint_desc*)0x0;
	if (xhci_get_endpoint_desc(pDevice, interfaceId, endpointId, &pEndpointDesc)!=0){
		printf("failed to get endpoint descriptor\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	struct xhci_trb cmdTrb = {0};
	memset((void*)&cmdTrb, 0, sizeof(struct xhci_trb));
	cmdTrb.reset_endpoint_cmd.type = XHCI_TRB_TYPE_RESET_ENDPOINT;
	cmdTrb.reset_endpoint_cmd.input_context_base = pDevice->deviceContext.pDeviceContext_phys;
	cmdTrb.reset_endpoint_cmd.endpoint_id = endpointId+1;	
	cmdTrb.reset_endpoint_cmd.slot_id = pDevice->deviceContext.slotId;
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, cmdTrb, &pCmdDesc)!=0){
		printf("failed to push reset endpoint cmd TRB\r\n");
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	xhci_start();
	xhci_ring(0);
	while (!pCmdDesc->cmdComplete){};
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	mutex_unlock_isr_safe(&mutex);	
	return 0;
}
KAPI int xhci_send_usb_packet(struct xhci_device* pDevice, struct xhci_usb_packet_request request, struct xhci_trb* pEventTrb){
	if (!pDevice)
		return -1;
	uint64_t pBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)request.pBuffer, &pBuffer_phys)!=0){
		return -1;	
	}
	uint64_t physicalPageCount = request.bufferSize/PAGE_SIZE;
	if (physicalPageCount>255)
		return -1;
	struct xhci_endpoint_desc* pEndpointDesc = (struct xhci_endpoint_desc*)0x0;
	if (xhci_get_endpoint_desc(pDevice, request.interfaceId, request.endpointId, &pEndpointDesc)!=0){
		printf("failed to get endpoint descriptor\r\n");
		return -1;
	}
	struct xhci_transfer_ring_info* pTransferRingInfo = (struct xhci_transfer_ring_info*)0x0;
	if (xhci_get_endpoint_transfer_ring(pDevice, request.interfaceId, request.endpointId, &pTransferRingInfo)!=0){
		printf("failed to get endpoint transfer ring\r\n");
		return -1;
	}
	if (!pTransferRingInfo){
		printf("no transfer ring linked with endpoint! port: %d interface ID: %d endpoint ID: %d\r\n", pDevice->port, request.interfaceId, request.endpointId);
		return -1;
	}
	uint8_t endpointIndex = pEndpointDesc->endpointIndex;
	struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;
	struct xhci_trb normalTrb = {0};
	memset((void*)&normalTrb, 0, sizeof(struct xhci_trb));
	normalTrb.normal_transfer.type = XHCI_TRB_TYPE_NORMAL;
	normalTrb.normal_transfer.buffer_phys = pBuffer_phys;
	normalTrb.normal_transfer.transfer_len = (physicalPageCount>1) ? PAGE_SIZE : request.bufferSize;	
	normalTrb.normal_transfer.ioc = (physicalPageCount>1) ? 0 : 1;
	normalTrb.normal_transfer.isp = 1;
	normalTrb.normal_transfer.chain_bit = (request.chainAllowed&&(physicalPageCount>1)) ? 1 : 0;
	normalTrb.normal_transfer.dir = pEndpointDesc->endpointDirection ? 1 : 0;
	if (xhci_alloc_transfer(pTransferRingInfo, normalTrb, &pTransferDesc, (physicalPageCount>1) ? (xhciTransferCompletionFunc)0x0 : request.completionFunc)!=0){
		printf("failed to allocate transfer TRB\r\n");
		return -1;
	}
	for (uint64_t i = 0;i<physicalPageCount-1&&physicalPageCount>1&&request.chainAllowed;i++){
		unsigned char* pVirtualPage = request.pBuffer+PAGE_SIZE+(i*PAGE_SIZE);
		if (virtualToPhysical((uint64_t)pVirtualPage, &pBuffer_phys)!=0){
			printf("failed to get physical address of page\r\n");
			return -1;
		}
		normalTrb.normal_transfer.ioc = (i==physicalPageCount-2) ? 1 : 0;
		normalTrb.normal_transfer.chain_bit = (i==physicalPageCount-2) ? 0 : 1;
		normalTrb.normal_transfer.buffer_phys = pBuffer_phys;
		normalTrb.normal_transfer.transfer_len = (i==physicalPageCount-2&&(request.bufferSize%PAGE_SIZE)) ? (request.bufferSize%PAGE_SIZE) : PAGE_SIZE;
		if (xhci_alloc_transfer(pTransferRingInfo, normalTrb, &pTransferDesc, (i==physicalPageCount-2) ? request.completionFunc : (xhciTransferCompletionFunc)0x0)!=0){
			printf("failed to allocate transfer\r\n");
			return -1;
		}
	}	
	xhci_start();
	xhci_ring_endpoint(pDevice->deviceContext.slotId, endpointIndex, 0);
	if (request.completionFunc){
		return 0;
	}
	while (!pTransferDesc->transferComplete){};	
	struct xhci_trb eventTrb = pTransferDesc->eventTrb;
	if (pEventTrb)
		*pEventTrb = eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		return -1;
	}	
	return 0;
}
int xhci_subsystem_register_function(struct pcie_location location){
	uint8_t progIf = 0;
	if (pcie_get_progif(location, &progIf)!=0)
		return -1;
	if (progIf!=PCIE_PROGIF_XHC)
		return -1;	
	xhciLocation = location;
	if (xhci_init()!=0)
		return -1;
	return 0;
}
int xhci_subsystem_unregister_function(struct pcie_location location){

	return 0;
}
