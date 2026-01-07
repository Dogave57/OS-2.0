#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "drivers/pcie.h"
#include "drivers/usb/xhci.h"
struct xhci_info xhciInfo = {0};
int xhci_init(void){
	if (xhci_get_info(&xhciInfo)!=0)
		return -1;
	printf("capability registers: %p\r\n", (uint64_t)xhciInfo.pCapabilities);
	printf("operational registers: %p\r\n", (uint64_t)xhciInfo.pOperational);
	printf("XHCI controller bus: %d, dev: %d, func: %d\r\n", xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func);
	xhciInfo.pOperational->page_size = 0;
	if (xhci_init_cmd_list()!=0){
		printf("failed to initialize XHCI TRB ring buffer\r\n");
		return -1;
	}
	uint8_t portCount = 0;
	if (xhci_get_port_count(&portCount)!=0)
		return -1;
	printf("port count: %d\r\n", portCount);
	xhci_ring(0);
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
	uint64_t baseMmioPages = 32;
	if (virtualGetSpace((uint64_t*)&pInfo->pBaseMmio, baseMmioPages)!=0)
		return -1;
	if (virtualMapPages(pInfo->pBaseMmio_physical, pInfo->pBaseMmio, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, baseMmioPages, 1, 0, PAGE_TYPE_MMIO)!=0){
		return -1;
	}
	pInfo->pCapabilities = (volatile struct xhci_cap_mmio*)pInfo->pBaseMmio;
	pInfo->pOperational = (volatile struct xhci_operational_mmio*)(pInfo->pBaseMmio+pInfo->pCapabilities->cap_len);
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
int xhci_init_cmd_list(void){
	volatile struct xhci_trb* pRingBuffer = (volatile struct xhci_trb*)0x0;
	if (virtualAllocPage((uint64_t*)&pRingBuffer, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0)
		return -1;
	xhciInfo.cmdRingInfo.pRingBuffer = pRingBuffer;
	xhciInfo.cmdRingInfo.maxEntries = XHCI_MAX_CMD_TRB_ENTRIES;
	xhciInfo.cmdRingInfo.entryCount = 0;
	if (virtualToPhysical((uint64_t)pRingBuffer, &xhciInfo.cmdRingInfo.pRingBuffer_phys)!=0)
		return -1;
	return 0;
}
int xhci_push_cmd(struct xhci_trb trb, uint64_t* pTrbIndex){
	if (!pTrbIndex)
		return -1;
	volatile struct xhci_trb* pTrbList = xhciInfo.cmdRingInfo.pRingBuffer;

	return 0;
}
int xhci_get_cmd(uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry){
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
