#include "stdlib/stdlib.h"
#include "align.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/idt.h"
#include "cpu/interrupt.h"
#include "drivers/pcie.h"
#include "drivers/apic.h"
#include "drivers/timer.h"
#include "drivers/usb/xhci.h"
struct xhci_info xhciInfo = {0};
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
	printf("XHC virtual base: %p\r\n", (uint64_t)xhciInfo.pBaseMmio);
	printf("XHC physical base: %p\r\n", (uint64_t)xhciInfo.pBaseMmio_physical);
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
	volatile struct xhci_usb_legacy_support* pLegacySupport = (volatile struct xhci_usb_legacy_support*)0x0;
	if (xhci_get_extended_cap(1, (volatile struct xhci_extended_cap_hdr**)&pLegacySupport)!=0){
		printf("failed to get USB legacy support extended cap header\r\n");
	}
	uint8_t portCount = 0;
	if (xhci_get_port_count(&portCount)!=0)
		return -1;
	printf("port count: %d\r\n", portCount);
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
	printf("completion status: %s\r\n", completionStatusName);
	while (!pCmdDesc->cmdComplete){};	
	xhci_get_error_name(pCmdDesc->eventTrb.event.completion_code, &completionStatusName);
	printf("completion status: %s\r\n", completionStatusName);
	uint64_t elapsed_us = get_time_us()-time_us;
	struct xhci_usb_status usb_status = xhciInfo.pOperational->usb_status;
	if (usb_status.host_system_error)
		printf("host system error\r\n");
	if (usb_status.host_controller_error)
		printf("host controller error\r\n");
	for (uint8_t i = 0;i<portCount;i++){
		if (xhci_device_exists(i)!=0)
			continue;
		if (xhci_reset_port(i)!=0){
			printf("failed to reset port %d\r\n", i);
			continue;
		}
		sleep(50);
		printf("XHCI controlled USB device at port %d\r\n", i);
		struct xhci_device* pDevice = (struct xhci_device*)0x0;
		if (xhci_init_device(i, &pDevice)!=0){
			printf("failed to initialize device at port %d\r\n", i);
			continue;
		}
		printf("successfully initialized device at port %d\r\n", i);
	}
	while (1){};
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
	struct pcie_info pcieInfo = {0};
	if (pcie_get_info(&pcieInfo)!=0)
		return -1;
	for (uint8_t bus = pcieInfo.startBus;bus<pcieInfo.endBus;bus++){
		for (uint8_t dev = 0;dev<32;dev++){
			for (uint8_t func = 0;func<8;func++){
				uint8_t class_id = 0;
				uint8_t subclass_id = 0;
				uint8_t progif_id = 0;
				struct pcie_location location = {0};
				location.bus = bus;
				location.dev = dev;
				location.func = func;
				if (pcie_get_class(location, &class_id)!=0)
					continue;
				if (class_id!=PCIE_CLASS_USB_HCI)
					continue;
				if (pcie_get_subclass(location, &subclass_id)!=0)
					continue;
				if (subclass_id!=PCIE_SUBCLASS_USB)
					continue;
				if (pcie_get_progif(location, &progif_id)!=0)
					continue;
				if (progif_id!=PCIE_PROGIF_XHCI)
					continue;
				pLocation->bus = bus;
				pLocation->dev = dev;
				pLocation->func = func;
				return 0;
			}
		}
	}
	return -1;
}
int xhci_read_qword(volatile uint64_t* pQword, uint64_t* pValue){
	if (!pQword||!pValue)
		return -1;
	uint32_t low = *(volatile uint32_t*)pQword;
	uint32_t high = *(((volatile uint32_t*)pQword)+1);
	*(uint32_t*)pValue = low;
	*(((uint32_t*)pValue)+1) = high;
	return 0;
}
int xhci_write_qword(volatile uint64_t* pQword, uint64_t value){
	if (!pQword)
		return -1;
	uint32_t low = (uint32_t)(value&0xFFFFFFFF);
	uint32_t high = (uint32_t)((value>>32)&0xFFFFFFFF);
	*(uint32_t*)pQword = low;
	*(((uint32_t*)pQword)+1) = high;
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
	while (status.port_reset){
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
	sleep(5);	
	return 0;
}
int xhci_disable_port(uint8_t port){
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port(port, &pPort)!=0)
		return -1;
	struct xhci_port_status status = pPort->port_status;
	status.enable = 0;
	pPort->port_status = status;
	sleep(5);
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
	uint64_t listSize = XHCI_MAX_SLOT_COUNT*sizeof(struct xhci_device);
	struct xhci_device* pDeviceDescList = (struct xhci_device*)0x0;
	if (virtualAlloc((uint64_t*)&pDeviceDescList, listSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
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
	uint64_t trbIndex = pRingInfo->currentEntry;
	if (trbIndex>=XHCI_MAX_CMD_TRB_ENTRIES-1){
		pRingInfo->cycle_state = !pRingInfo->cycle_state;
		trbIndex = 0;
		pRingInfo->currentEntry = 0;
		volatile struct xhci_trb* pLinkTrb = (volatile struct xhci_trb*)0x0;
		xhci_get_cmd(pRingInfo, XHCI_MAX_CMD_TRB_ENTRIES-1, &pLinkTrb);
		pLinkTrb->command.control.cycle_bit = pRingInfo->cycle_state;
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
	uint64_t listSize = sizeof(struct xhci_transfer_desc)*XHCI_MAX_SLOT_COUNT*XHCI_MAX_ENDPOINT_COUNT*255;
	struct xhci_transfer_desc* pTransferDescList = (struct xhci_transfer_desc*)0x0;
	if (virtualAlloc((uint64_t*)&pTransferDescList, listSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	xhciInfo.pTransferDescList = pTransferDescList;
	return 0;
}
int xhci_get_transfer_ring(uint8_t slotId, uint8_t endpoint, struct xhci_transfer_ring_info** ppTransferRing){
	if (!ppTransferRing)
		return -1;
	uint64_t transferRingOffset = ((uint64_t)slotId*XHCI_MAX_ENDPOINT_COUNT)+(uint64_t)endpoint;
	struct xhci_transfer_ring_info* pTransferRing = xhciInfo.pTransferRingList+transferRingOffset;
	*ppTransferRing = pTransferRing;	
	return 0;
}
int xhci_get_transfer_desc(uint8_t slotId, uint8_t endpoint, uint64_t trbIndex, struct xhci_transfer_desc** ppTransferDesc){
	if (!ppTransferDesc)
		return -1;
	uint64_t transferDescOffset = ((uint64_t)slotId*XHCI_MAX_ENDPOINT_COUNT)+(uint64_t)endpoint+trbIndex;
	struct xhci_transfer_desc* pTransferDesc = xhciInfo.pTransferDescList+transferDescOffset;
	*ppTransferDesc = pTransferDesc;
	return 0;
}
int xhci_init_transfer_ring(struct xhci_device* pDevice, uint8_t endpoint, struct xhci_transfer_ring_info** ppTransferRingInfo){
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
	struct xhci_trb linkTrb = {0};
	memset((void*)&linkTrb, 0, sizeof(struct xhci_trb));
	linkTrb.generic.control.tc_bit = 1;
	linkTrb.generic.control.cycle_bit = cycle_state;
	linkTrb.generic.ptr = pTransferRing_phys;
	*(pTransferRing+XHCI_MAX_TRANFER_TRB_ENTRIES-1) = linkTrb;
	struct xhci_transfer_ring_info* pTransferRingInfo = (struct xhci_transfer_ring_info*)0x0;
	if (xhci_get_transfer_ring(pDevice->deviceContext.slotId, endpoint, &pTransferRingInfo)!=0){
		printf("failed to get transfer ring\r\n");
		virtualFreePage((uint64_t)pTransferRing, 0);
		return -1;
	}
	memset((void*)pTransferRingInfo, 0, sizeof(struct xhci_transfer_ring_info));
	pTransferRingInfo->pRingBuffer = pTransferRing;
	pTransferRingInfo->pRingBuffer_phys = pTransferRing_phys;
	pTransferRingInfo->ringBufferSize = XHCI_MAX_TRANFER_TRB_ENTRIES;
	pTransferRingInfo->pDevice = pDevice;
	pTransferRingInfo->endpoint = endpoint;
	pTransferRingInfo->cycle_state = cycle_state;
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
int xhci_alloc_transfer(struct xhci_transfer_ring_info* pTransferRingInfo, struct xhci_trb trb, struct xhci_transfer_desc** ppTransferDesc){
	if (!pTransferRingInfo||!ppTransferDesc)
		return -1;
	uint64_t trbIndex = pTransferRingInfo->currentEntry;
	if (trbIndex>=XHCI_MAX_TRANFER_TRB_ENTRIES-1){
		pTransferRingInfo->cycle_state = !pTransferRingInfo->cycle_state;
		struct xhci_trb linkTrb = {0};
		memset((void*)&linkTrb, 0, sizeof(struct xhci_trb));
		linkTrb.generic.control.type = XHCI_TRB_TYPE_LINK;
		linkTrb.generic.control.tc_bit = 1;
		linkTrb.generic.control.cycle_bit = pTransferRingInfo->cycle_state;
		linkTrb.generic.ptr = pTransferRingInfo->pRingBuffer_phys;
		pTransferRingInfo->currentEntry = 0;
		trbIndex = 0;
	}
	xhci_write_transfer(pTransferRingInfo, trbIndex, trb);
	struct xhci_transfer_desc* pTransferDesc = (struct xhci_transfer_desc*)0x0;	
	uint8_t port = pTransferRingInfo->pDevice->port;
	uint8_t endpoint = pTransferRingInfo->endpoint;
	if (xhci_get_transfer_desc(pTransferRingInfo->pDevice->deviceContext.slotId, endpoint, trbIndex, &pTransferDesc)!=0){
		printf("failed to get transfer desc\r\n");
		return -1;
	}
	pTransferRingInfo->currentEntry++;
	*ppTransferDesc = pTransferDesc;	
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
		status.host_controller_error = 0;
		status.host_system_error = 0;
		xhciInfo.pOperational->usb_status = status;
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
	idt_add_entry(interrupterVector, (uint64_t)xhci_interrupter_isr, 0x8E, 0x0);
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
		[XHCI_TRB_TYPE_CONFIG_ENDPOINT]="Configure endpoint",
		[XHCI_TRB_TYPE_UPDATE_CONTEXT]="Update context",
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
			uint8_t endpointId = eventTrb.event_transfer.endpoint_id/2;
			if (xhci_get_transfer_ring(slotId, endpointId, &pTransferRingInfo)!=0){
				printf("failed to get transfer ring info\r\n");
				break;
			}
			uint64_t trbIndex = (pTrb_phys-pTransferRingInfo->pRingBuffer_phys)/sizeof(struct xhci_trb);
			if (xhci_get_transfer_desc(slotId, endpointId, trbIndex, &pTransferDesc)!=0){
				printf("failed to get transfer desc\r\n");
				break;
			}
			pTransferDesc->eventTrb = eventTrb;
			pTransferDesc->transferComplete = 1;
			break;				       
		}
	}
	const unsigned char* pTrbTypeName = (const unsigned char*)0x0;
	if (xhci_get_trb_type_name(pEventTrb->event.control.type, &pTrbTypeName)!=0){
		printf("failed to get event TRB type name\r\n");
		xhci_send_ack(0);
		xhci_update_dequeue_trb();
		return -1;
	}
	xhci_send_ack(0);
	xhci_update_dequeue_trb();
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
int xhci_init_device(uint8_t port, struct xhci_device** ppDevice){
	if (!ppDevice)
		return -1;
	volatile struct xhci_cap_mmio* pCapRegisters = xhciInfo.pCapabilities;
	struct xhci_cap_param0 param0 = pCapRegisters->cap_param0;
	uint64_t contextSize = param0.context_size ? sizeof(struct xhci_slot_context64) : sizeof(struct xhci_slot_context32);
	printf("XHC has %s contexts\r\n", param0.context_size ? "long" : "short");	
	if (xhci_enable_port(port)!=0){
		printf("failed to enable PORT%d\r\n", port);
		return -1;
	}
	uint64_t pDeviceContext = 0;	
	uint64_t pDeviceContext_phys = 0;
	if (virtualAllocPage((uint64_t*)&pDeviceContext, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate device context\r\n");
		return -1;
	}
	if (virtualToPhysical((uint64_t)pDeviceContext, &pDeviceContext_phys)!=0){
		printf("failed to get physical address of device context\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		return -1;
	}
	memset((void*)pDeviceContext, 0, PAGE_SIZE);
	volatile struct xhci_input_context32* pInputContext = (volatile struct xhci_input_context32*)pDeviceContext;	
	volatile struct xhci_slot_context32* pSlotContext = (volatile struct xhci_slot_context32*)(pDeviceContext+contextSize);
	volatile struct xhci_endpoint_context32* pControlContext = (volatile struct xhci_endpoint_context32*)0x0;
	pControlContext = (volatile struct xhci_endpoint_context32*)(((uint64_t)pSlotContext)+contextSize);	
	struct xhci_device device = {0};
	device.port = port;
	device.deviceContext.pDeviceContext = pDeviceContext;
	device.deviceContext.pDeviceContext_phys = pDeviceContext_phys;
	struct xhci_transfer_ring_info* pTransferRingInfo = (struct xhci_transfer_ring_info*)0x0;
	uint64_t slotId = 0;
	if (xhci_enable_slot(&slotId)!=0){
		printf("failed to enable slot\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		return -1;
	}	
	device.deviceContext.slotId = slotId;
	if (xhci_init_transfer_ring(&device, 0, &pTransferRingInfo)!=0){
		printf("failed to initialize transfer ring\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_disable_slot(slotId);
		return -1;
	}	
	uint8_t portSpeed = 0;
	xhci_get_port_speed(port, &portSpeed);
	printf("port speed: %d\r\n", portSpeed);
	pSlotContext->speed = portSpeed;
	pSlotContext->context_entries = 1;
	pSlotContext->root_hub_port_num = port+1;
	pControlContext->error_count = 3;
	pControlContext->type = 4;
	pControlContext->max_packet_size = 8;
	pControlContext->dequeue_ptr = (uint64_t)pTransferRingInfo->pRingBuffer_phys;
	pControlContext->average_trb_len = 8;
	if (pTransferRingInfo->cycle_state)
		pControlContext->dequeue_ptr|=(1<<0);
	else
		pControlContext->dequeue_ptr&=~(1<<0);
	pControlContext->average_trb_len = 8;
	pInputContext->add_flags|=(1<<0);
	pInputContext->add_flags|=(1<<1);
	xhciInfo.deviceContextListInfo.pContextList[slotId] = (uint64_t)pDeviceContext_phys;
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	if (xhci_address_device(&device, 1, &pCmdDesc)!=0){
		printf("failed to address device\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		return -1;
	}	
	pSlotContext->speed = portSpeed;
	pSlotContext->context_entries = 1;
	pSlotContext->root_hub_port_num = port+1;
	pControlContext->error_count = 3;
	pControlContext->type = 4;
	pControlContext->max_packet_size = 8;
	pControlContext->dequeue_ptr = (uint64_t)pTransferRingInfo->pRingBuffer_phys;
	if (pTransferRingInfo->cycle_state)
		pControlContext->dequeue_ptr|=(1<<0);
	else
		pControlContext->dequeue_ptr&=~(1<<0);
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;
	const unsigned char* completionStatusName = "Unknown status";
	xhci_get_error_name(eventTrb.event.completion_code, &completionStatusName);	
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		printf("failed to address device (%s)\r\n", completionStatusName);
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		return -1;
	}
	struct xhci_usb_device_desc deviceDescriptor = {0};	
	if (xhci_get_descriptor(&device, pTransferRingInfo, 0x1, 0x0, (unsigned char*)&deviceDescriptor, 18, &eventTrb)!=0){
		printf("failed to get descriptor\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		return-1;
	}
	struct xhci_usb_string_desc manufacturerName = {0};
	struct xhci_usb_string_desc productName = {0};
	struct xhci_usb_string_desc serialName = {0};
	if (xhci_get_descriptor(&device, pTransferRingInfo, 0x3, deviceDescriptor.manufacturerIndex, (unsigned char*)&manufacturerName, 255, &eventTrb)!=0){
		printf("failed to get manufacturer name\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		return -1;
	}
	if (xhci_get_descriptor(&device, pTransferRingInfo, 0x3, deviceDescriptor.productIndex, (unsigned char*)&productName, 255, &eventTrb)!=0){
		printf("failed to get product name\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		return-1;
	}
	if (xhci_get_descriptor(&device, pTransferRingInfo, 0x3, deviceDescriptor.serialIndex, (unsigned char*)&serialName, 255, &eventTrb)!=0){
		printf("failed to get serial name\r\n");
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		return -1;
	}
	printf("class: 0x%x\r\n", deviceDescriptor.class);
	printf("subclass: 0x%x\r\n", deviceDescriptor.subclass);
	printf("vendor ID: 0x%x\r\n", deviceDescriptor.vendorId);
	printf("product ID: 0x%x\r\n", deviceDescriptor.productId);
	lprintf(L"manufacturer name: %s\r\n", manufacturerName.string);
	lprintf(L"product name: %s\r\n", productName.string);
	lprintf(L"serial name: %s\r\n", serialName.string);
	struct xhci_device* pDevice = (struct xhci_device*)0x0;
	if (xhci_get_device(port, &pDevice)!=0){
		printf("failed to get device descriptor\r\n");
		xhci_disable_slot(slotId);
		virtualFreePage((uint64_t)pDeviceContext, 0);
		xhci_deinit_transfer_ring(pTransferRingInfo);
		return -1;
	}	
	*pDevice = device;
	*ppDevice = pDevice;	
	return 0;
}
int xhci_deinit_device(struct xhci_device* pDevice){
	if (!pDevice)
		return -1;
	struct xhci_device_context_desc* pContextDesc = &pDevice->deviceContext;
	if (pContextDesc->pDeviceContext)
		virtualFreePage((uint64_t)pContextDesc->pDeviceContext, 0);
	if (pContextDesc->pInputContext)
		virtualFreePage((uint64_t)pContextDesc->pInputContext, 0);
	if (pContextDesc->slotId)
		xhci_disable_slot(pContextDesc->slotId);
	return 0;
}
int xhci_get_endpoint_context(struct xhci_device_context_desc* pContextDesc, uint64_t endpoint_index, volatile struct xhci_endpoint_context32** ppEndPointContext){
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
int xhci_address_device(struct xhci_device* pDevice, uint8_t block_set_address, struct xhci_cmd_desc** ppCmdDesc){
	if (!pDevice)
		return -1;
	struct xhci_cmd_desc* pCmdDesc = (struct xhci_cmd_desc*)0x0;
	struct xhci_trb cmdTrb = {0};
	memset((void*)&cmdTrb, 0, sizeof(struct xhci_trb));
	cmdTrb.address_device_cmd.type = XHCI_TRB_TYPE_ADDRESS_DEVICE;	
	cmdTrb.address_device_cmd.input_context_ptr = pDevice->deviceContext.pDeviceContext_phys;
	cmdTrb.address_device_cmd.slotId = pDevice->deviceContext.slotId;
	cmdTrb.address_device_cmd.block_set_address = block_set_address ? 1 : 0;
	if (xhci_alloc_cmd(xhciInfo.pCmdRingInfo, cmdTrb, &pCmdDesc)!=0){
		printf("failed to push addrss device TRB to cmd ring\r\n");
		return -1;
	}	
	xhci_start();
	xhci_ring(0);
	while (!pCmdDesc->cmdComplete||!xhci_get_cmd_ring_running()){};
	struct xhci_trb eventTrb = pCmdDesc->eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		const unsigned char* errorName = "Unknown error";
		xhci_get_error_name(eventTrb.event.completion_code, &errorName);
		printf("failed to address device (%s)\r\n", errorName);
		return -1;
	}
	*ppCmdDesc = pCmdDesc;
	return 0;
}
int xhci_get_descriptor(struct xhci_device* pDevice, struct xhci_transfer_ring_info* pTransferRingInfo, uint8_t type, uint8_t index, unsigned char* pBuffer, uint64_t len, struct xhci_trb* pEventTrb){
	if (!pDevice||!pTransferRingInfo||!pBuffer)
		return -1;
	uint64_t pBuffer_phys = 0;
	if (virtualToPhysical((uint64_t)pBuffer, &pBuffer_phys)!=0)
		return -1;
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
	if (xhci_alloc_transfer(pTransferRingInfo, setupTrb, &pTransferDesc)!=0){
		printf("failed to allocate transfer setup TRB\r\n");
		return -1;
	}	
	if (xhci_alloc_transfer(pTransferRingInfo, dataTrb, &pTransferDesc)!=0){
		printf("failed to allocate transfer data TRB\r\n");
		return -1;
	}
	if (xhci_alloc_transfer(pTransferRingInfo, statusTrb, &pTransferDesc)!=0){
		printf("failed to allocate transfer status TRB\r\n");
		return -1;
	}	
	xhci_start();
	xhci_ring_endpoint(pDevice->deviceContext.slotId, 1, 0);
	while (!pTransferDesc->transferComplete){};
	struct xhci_trb eventTrb = pTransferDesc->eventTrb;
	if (eventTrb.event.completion_code!=XHCI_COMPLETION_CODE_SUCCESS){
		printf("failed to get descriptor\r\n");
		return -1;
	}
	if (pEventTrb)
		*pEventTrb = eventTrb;	
	return 0;
}
