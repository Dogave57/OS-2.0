#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "drivers/pcie.h"
#include "drivers/timer.h"
#include "drivers/usb/xhci.h"
struct xhci_info xhciInfo = {0};
int xhci_init(void){
	if (xhci_get_info(&xhciInfo)!=0)
		return -1;
	printf("capability registers: %p\r\n", (uint64_t)xhciInfo.pCapabilities);
	printf("operational registers: %p\r\n", (uint64_t)xhciInfo.pOperational);
	printf("XHCI controller bus: %d, dev: %d, func: %d\r\n", xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func);
	uint32_t pcie_cmd_reg = 0;
	pcie_read_dword(xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func, 0x4, &pcie_cmd_reg);
	pcie_cmd_reg|=(1<<0);
	pcie_cmd_reg|=(1<<1);
	pcie_write_dword(xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func, 0x4, pcie_cmd_reg);
	if (xhci_init_trb_list(&xhciInfo.cmdRingInfo)!=0){
		printf("failed to initialize cmd ring TRB list\r\n");
		return -1;
	}
	printf("XHCI controller virtual base: %p\r\n", (uint64_t)xhciInfo.pBaseMmio);
	return 0;
	printf("current run bit: %d\r\n", xhciInfo.pOperational->usb_cmd.hc_reset);
	xhciInfo.pOperational->usb_cmd.hc_reset = 1;
	printf("current run bit: %d\r\n", xhciInfo.pOperational->usb_cmd.hc_reset);
	while (xhciInfo.pOperational->usb_status.controller_not_ready||!xhciInfo.pOperational->usb_status.halted){};
	printf("old page size: %d\r\n", xhciInfo.pOperational->page_size);
	xhciInfo.pOperational->page_size = 0;
	printf("new page size: %d\r\n", xhciInfo.pOperational->page_size);
	xhciInfo.pOperational->usb_cmd.run = 0;
	while (xhciInfo.pOperational->usb_cmd.run){};
	xhciInfo.pOperational->usb_cmd.hc_reset = 1;
	while (xhciInfo.pOperational->usb_cmd.hc_reset||!xhciInfo.pOperational->usb_status.halted){};
	printf("halted: %d\r\n", xhciInfo.pOperational->usb_status.halted);
	printf("old page size: %d\r\n", xhciInfo.pOperational->page_size);
	xhciInfo.pOperational->page_size = 0;
	xhciInfo.pOperational->cmd_ring_ctrl.ring_base = xhciInfo.cmdRingInfo.pRingBuffer_phys;
	uint8_t portCount = 0;
	if (xhci_get_port_count(&portCount)!=0)
		return -1;
	printf("port count: %d\r\n", portCount);
	printf("cmd ring base: %p\r\n", (uint64_t)xhciInfo.pOperational->cmd_ring_ctrl.ring_base);
	struct xhci_trb trb = {0};
	uint64_t trbIndex = 0;
	memset((void*)&trb, 0, sizeof(struct xhci_trb));
	trb.command.control.type = XHCI_TRB_TYPE_NORMAL;
	if (xhci_alloc_trb(&xhciInfo.cmdRingInfo, trb, &trbIndex)!=0){
		printf("failed to allocate TRB entry\r\n");
		return -1;
	}
	struct xhci_trb linkTrb = {0};
	uint64_t linkTrbIndex = 0;
	memset((void*)&linkTrb, 0, sizeof(struct xhci_trb));
	linkTrb.generic.ptr = (uint64_t)xhciInfo.cmdRingInfo.pRingBuffer_phys;
	linkTrb.generic.control.link_trb = 1;
	linkTrb.generic.control.type = XHCI_TRB_TYPE_LINK;
	if (xhci_alloc_trb(&xhciInfo.cmdRingInfo, linkTrb, &linkTrbIndex)!=0){
		printf("failed to allocate link TRB entry\r\n");
		return -1;
	}
	printf("cmd ring base: %p\r\n", (uint64_t)xhciInfo.pOperational->cmd_ring_ctrl.ring_base);
	xhci_ring(0);
	xhciInfo.pOperational->usb_cmd.run = 1;
	while (xhciInfo.pOperational->usb_cmd.run){
		printf("running...\r\n");
		sleep(50);
	}
	printf("cmd ring base: %p\r\n", (uint64_t)xhciInfo.pOperational->cmd_ring_ctrl.ring_base);
	for (uint8_t i = 0;i<portCount;i++){
		if (xhci_device_exists(i)!=0)
			continue;
		printf("XHCI controlled USB device at port %d\r\n", i);
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
	if (pcie_get_bar(pInfo->location.bus, pInfo->location.dev, pInfo->location.func, 0, &pInfo->pBaseMmio_physical)!=0)
		return -1;
	uint64_t baseMmioPages = 64;
	if (virtualGetSpace((uint64_t*)&pInfo->pBaseMmio, baseMmioPages)!=0)
		return -1;
	if (virtualMapPages(pInfo->pBaseMmio_physical, pInfo->pBaseMmio, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, baseMmioPages, 1, 0, PAGE_TYPE_MMIO)!=0){
		return -1;
	}
	pInfo->pCapabilities = (volatile struct xhci_cap_mmio*)pInfo->pBaseMmio;
	pInfo->pOperational = (volatile struct xhci_operational_mmio*)(pInfo->pBaseMmio+(uint64_t)pInfo->pCapabilities->cap_len);
	pInfo->pRuntime = (volatile struct xhci_runtime_mmio*)(pInfo->pBaseMmio+pInfo->pCapabilities->runtime_register_offset);
	pInfo->pDoorBells = (volatile uint32_t*)(pInfo->pBaseMmio+pInfo->pCapabilities->doorbell_offset);
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
				if (pcie_get_class(bus, dev, func, &class_id)!=0)
					continue;
				if (class_id!=PCIE_CLASS_USB_HCI)
					continue;
				if (pcie_get_subclass(bus, dev, func, &subclass_id)!=0)
					continue;
				if (subclass_id!=PCIE_SUBCLASS_USB)
					continue;
				if (pcie_get_progif(bus, dev, func, &progif_id)!=0)
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
int xhci_device_exists(uint8_t port){
	volatile struct xhci_port* pPort = (volatile struct xhci_port*)0x0;
	if (xhci_get_port(port, &pPort)!=0)
		return -1;
	return (pPort->port_status&(1<<0)) ? 0 : -1;
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
	uint8_t portCount = (uint8_t)((pCap->structure_params[0]>>24)&0xFF);
	*pPortCount = portCount;
	return 0;
}
int xhci_init_trb_list(struct xhci_trb_ring_info* pRingInfo){
	if (!pRingInfo)
		return -1;
	volatile struct xhci_trb* pRingBuffer = (volatile struct xhci_trb*)0x0;
	if (virtualAllocPage((uint64_t*)&pRingBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	memset((uint64_t*)pRingInfo, 0, sizeof(struct xhci_trb_ring_info));
	pRingInfo->pRingBuffer = pRingBuffer;
	pRingInfo->maxEntries = XHCI_MAX_CMD_TRB_ENTRIES;
	pRingInfo->entryCount = 0;
	uint64_t entryListSize = sizeof(uint64_t)*pRingInfo->maxEntries;
	if (virtualAlloc((uint64_t*)&pRingInfo->pEntryList, entryListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate entry list\r\n");
		virtualFreePage((uint64_t)pRingBuffer, entryListSize);
		return -1;
	}
	if (virtualAlloc((uint64_t*)&pRingInfo->pFreeList, entryListSize, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to allocate free entry list\r\n");
		virtualFree((uint64_t)pRingInfo->pFreeList, entryListSize);
		virtualFreePage((uint64_t)pRingBuffer, 0);
		return -1;
	}
	if (virtualToPhysical((uint64_t)pRingBuffer, &pRingInfo->pRingBuffer_phys)!=0){
		printf("failed to get physical address of TRB ring buffer\r\n");
		virtualFree((uint64_t)pRingInfo->pFreeList, entryListSize);
		virtualFree((uint64_t)pRingInfo->pEntryList, entryListSize);
		virtualFreePage((uint64_t)pRingBuffer, 0);
		return -1;
	}
	printf("ring buffer va: %p\r\n", (uint64_t)pRingInfo->pRingBuffer);
	printf("ring buffer pa: %p\r\n", (uint64_t)pRingInfo->pRingBuffer_phys);
	pRingInfo->entryListSize = entryListSize;
	for (uint64_t i = pRingInfo->maxEntries-1;i;i--){
		if (xhci_free_trb(pRingInfo, i)!=0){
			printf("failed to add TRB entry to free list\r\n");
			virtualFree((uint64_t)pRingInfo->pFreeList, entryListSize);
			virtualFree((uint64_t)pRingInfo->pEntryList, entryListSize);
			virtualFreePage((uint64_t)pRingBuffer, 0);
			return -1;
		}
	}
	return 0;
}
int xhci_deinit_trb_list(struct xhci_trb_ring_info* pRingInfo){
	if (!pRingInfo)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pRingBuffer, 0)!=0)
		return -1;
	if (virtualFree((uint64_t)pRingInfo->pEntryList, pRingInfo->entryListSize)!=0){
		return -1;
	}
	if (virtualFree((uint64_t)pRingInfo->pFreeList, pRingInfo->entryListSize)!=0)
		return -1;
	return 0;
}
int xhci_alloc_trb(struct xhci_trb_ring_info* pRingInfo, struct xhci_trb trb, uint64_t* pTrbIndex){
	if (!pRingInfo||!pTrbIndex)
		return -1;
	if (!pRingInfo->freeEntryCount)
		return -1;
	volatile struct xhci_trb* pTrbList = pRingInfo->pRingBuffer;
	uint64_t trbIndex = pRingInfo->pFreeList[pRingInfo->freeEntryCount-1];
	volatile struct xhci_trb* pTrb = pTrbList+trbIndex;
	pRingInfo->freeEntryCount--;
	*pTrb = trb;
	*pTrbIndex = trbIndex;
	return 0;
}
int xhci_free_trb(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex){
	if (!pRingInfo)
		return -1;
	pRingInfo->pFreeList[pRingInfo->freeEntryCount] = trbIndex;
	pRingInfo->freeEntryCount++;
	return 0;
}
int xhci_get_trb(uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry){
	if (!ppTrbEntry)
		return -1;
	*ppTrbEntry = xhciInfo.cmdRingInfo.pRingBuffer+trbIndex;
	return 0;
}
int xhci_ring(uint64_t doorbell_vector){
	volatile uint32_t* pDoorBell = xhciInfo.pDoorBells+doorbell_vector;
	*pDoorBell = 1;
	return 0;
}
