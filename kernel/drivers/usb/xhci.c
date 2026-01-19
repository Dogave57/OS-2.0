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
	if (xhci_init_scratchpad()!=0){
		printf("failed to initialize scratchpad pages\r\n");
		return -1;
	}
	if (xhci_init_cmd_list(&xhciInfo.cmdRingInfo)!=0){
		printf("failed to initialize cmd TRB ring list\r\n");
		return -1;
	}	
	if (xhci_init_device_context_list()!=0){
		printf("failed to initialize device context ptr list\r\n");
		return -1;
	}
	printf("initializing INT0 and ER\r\n");
	if (xhci_init_interrupter()!=0){
		printf("failed to initialize XHCI interrupter\r\n");
		return -1;
	}
	printf("done\r\n");
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
	if (xhci_alloc_cmd(&xhciInfo.cmdRingInfo, trb, &pCmdDesc)!=0){
		printf("failed to allocate TRB entry\r\n");
		return -1;
	}
	struct xhci_cmd_desc* pNewCmdDesc = (struct xhci_cmd_desc*)0x0;
	if (xhci_alloc_cmd(&xhciInfo.cmdRingInfo, trb, &pNewCmdDesc)!=0){
		printf("failed to allocate new TRB entry\r\n");
		return -1;
	}
	xhci_start();
	printf("running TRBs\r\n");
	xhci_ring(0);
	uint64_t time_us = get_time_us();
	while (!pCmdDesc->pEventTrb){};
	const unsigned char* pEventTrbName = (const unsigned char*)0x0;
	if (xhci_get_trb_type_name(pCmdDesc->pEventTrb->event.control.type, &pEventTrbName)!=0){
		printf("failed to get event TRB type name\r\n");
		return -1;
	}
	printf("event TRB type: %s\r\n", pEventTrbName);
	printf("event TRB: %p\r\n", (uint64_t)pCmdDesc->pEventTrb);
	while (!pNewCmdDesc->pEventTrb){};
	if (xhci_get_trb_type_name(pCmdDesc->pEventTrb->event.control.type, &pEventTrbName)!=0){
		printf("failed to get event TRB type name\r\n");
		return -1;
	}
	printf("new event TRB type: %s\r\n", pEventTrbName);
	printf("new event TRB: %p\r\n", (uint64_t)pNewCmdDesc->pEventTrb);
	while (!xhci_get_cmd_ring_running()){};
	uint64_t elapsed_us = get_time_us()-time_us;
	printf("done running TRBs in %dus\r\n", elapsed_us);
	struct xhci_usb_status usb_status = xhciInfo.pOperational->usb_status;
	if (usb_status.host_system_error)
		printf("host system error\r\n");
	if (usb_status.host_controller_error)
		printf("host controller error\r\n");
	for (uint8_t i = 0;i<portCount;i++){
		if (xhci_device_exists(i)!=0)
			continue;
		printf("XHCI controlled USB device at port %d\r\n", i);
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
	struct xhci_structure_param0 param0 = pCap->structure_param0;	
	uint8_t portCount = param0.max_ports;
	*pPortCount = portCount;
	return 0;
}
int xhci_init_cmd_list(struct xhci_trb_ring_info* pRingInfo){
	if (!pRingInfo)
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
	memset((uint64_t*)pRingInfo, 0, sizeof(struct xhci_trb_ring_info));
	pRingInfo->cycle_state = 1;
	pRingInfo->pRingBuffer = pRingBuffer;
	pRingInfo->maxEntries = XHCI_MAX_CMD_TRB_ENTRIES;
	pRingInfo->pCmdDescList = pCmdDescList;
	pRingInfo->cmdDescListSize = cmdListDescSize;
	uint64_t ringBufferSize = pRingInfo->maxEntries*sizeof(struct xhci_trb);
	pRingInfo->ringBufferSize = ringBufferSize;
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
		xhci_deinit_cmd_list(pRingInfo);
		return -1;
	}
	xhci_set_cmd_ring_base((uint64_t)pRingInfo->pRingBuffer_phys);
	return 0;
}
int xhci_deinit_cmd_list(struct xhci_trb_ring_info* pRingInfo){
	if (!pRingInfo)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pRingBuffer, 0)!=0)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pCmdDescList, 0)!=0)
		return -1;
	return 0;
}
int xhci_alloc_cmd(struct xhci_trb_ring_info* pRingInfo, struct xhci_trb trb, struct xhci_cmd_desc** ppCmdDesc){
	if (!pRingInfo||!ppCmdDesc)
		return -1;
	uint64_t trbIndex = pRingInfo->currentEntry;
	if (trbIndex>XHCI_MAX_CMD_TRB_ENTRIES-1){
		pRingInfo->cycle_state!=pRingInfo->cycle_state;
		trbIndex = 0;
	}
	xhci_write_cmd(pRingInfo, trbIndex, trb);
	volatile struct xhci_trb* pCmdTrb = pRingInfo->pRingBuffer+trbIndex;
	uint64_t pCmdTrb_phys = pRingInfo->pRingBuffer_phys+(trbIndex*sizeof(struct xhci_trb));	
	struct xhci_cmd_desc* pCmdDesc = pRingInfo->pCmdDescList+trbIndex;
	memset((void*)pCmdDesc, 0, sizeof(struct xhci_cmd_desc));
	pCmdDesc->pCmdTrb = pCmdTrb;
	pCmdDesc->pCmdTrb_phys = pCmdTrb_phys;
	pCmdDesc->trbIndex = trbIndex;
	*ppCmdDesc = pCmdDesc;
	pRingInfo->currentEntry++;	
	return 0;
}
int xhci_get_cmd(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry){
	if (!pRingInfo||!ppTrbEntry)
		return -1;
	*ppTrbEntry = xhciInfo.cmdRingInfo.pRingBuffer+trbIndex;
	return 0;
}
int xhci_write_cmd(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb trbEntry){
	if (!pRingInfo)
		return -1;
	unsigned char cycle_state = 0;
	if (xhci_get_driver_cycle_state(&cycle_state)!=0)
		return -1;
	trbEntry.generic.control.cycle_bit = cycle_state;
	*(pRingInfo->pRingBuffer+trbIndex) = trbEntry;
	return 0;
}
int xhci_read_cmd(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb* pTrbEntry){
	if (!pRingInfo||!pTrbEntry)
		return -1;
	*pTrbEntry = *(pRingInfo->pRingBuffer+trbIndex);
	return 0;
}
int xhci_ring(uint64_t doorbell_vector){
	volatile uint32_t* pDoorBell = xhciInfo.pDoorBells+doorbell_vector;
	*pDoorBell = 0;
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
	*pBase = xhciInfo.cmdRingInfo.pRingBuffer_phys;
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
	printf("event TRB ring buffer PA: %p\r\n", xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys);
	printf("cmd ring base: %p\r\n", xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys);
	struct xhci_segment_table_entry segmentEntry = {0};
	segmentEntry.base = xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys;
	segmentEntry.size = xhciInfo.interrupterInfo.eventRingInfo.maxEntries;
	segmentEntry.reserved0 = 0;
	*pSegmentTable = segmentEntry;
	printf("segment table PA: %p\r\n", pSegmentTable_phys);
	pInt->table_size = 1;
	struct xhci_interrupter_iman iman = pInt->interrupt_management;
	iman.interrupt_enable = 1;
	iman.interrupt_pending = 1;
	pInt->interrupt_management = iman;
	uint32_t imod = pInt->interrupt_moderation;
	imod = 0;
	pInt->interrupt_moderation = imod;
	printf("interrupter vector: %d\r\n", interrupterVector);
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
	printf("interrupter vector: %d\r\n", interrupterVector);
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
	if (!xhciInfo.interrupterInfo.eventRingInfo.dequeueTrbBase||(xhciInfo.interrupterInfo.eventRingInfo.dequeueTrb)>=xhciInfo.interrupterInfo.eventRingInfo.maxEntries-1){
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
	if (!ppName||type>0x27)
		return -1;
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
		return -1;
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
	uint64_t pCmdTrb_phys = pEventTrb->event.trb_ptr;	
	if (!pCmdTrb_phys){
		printf("event TRB not linked to cmd TRB\r\n");
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
	uint64_t trbIndex = (uint64_t)((pCmdTrb_phys-xhciInfo.cmdRingInfo.pRingBuffer_phys)/sizeof(struct xhci_trb));
	struct xhci_cmd_desc* pCmdDesc = xhciInfo.cmdRingInfo.pCmdDescList+trbIndex;
	pCmdDesc->pEventTrb = pEventTrb;
	pCmdDesc->pEventTrb_phys = pEventTrb_phys;
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
	xhciInfo.deviceContextListInfo.pContextList = pDeviceContextList;
	xhciInfo.deviceContextListInfo.maxDeviceCount = XHCI_MAX_DEVICE_COUNT;
	xhci_write_qword((uint64_t*)&xhciInfo.pOperational->device_context_base_list_ptr, (uint64_t)pDeviceContextList);
	return 0;
}
int xhci_get_driver_cycle_state(unsigned char* pCycleState){
	if (!pCycleState)
		return -1;
	*pCycleState = xhciInfo.cmdRingInfo.cycle_state;
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
