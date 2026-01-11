#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "cpu/idt.h"
#include "cpu/interrupt.h"
#include "drivers/pcie.h"
#include "drivers/apic.h"
#include "drivers/timer.h"
#include "drivers/usb/xhci.h"
struct xhci_info xhciInfo = {0};
int xhci_init(void){
	if (xhci_get_info(&xhciInfo)!=0)
		return -1;
	printf("XHCI controller bus: %d, dev: %d, func: %d\r\n", xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func);
	uint32_t pcie_cmd_reg = 0;
	pcie_read_dword(xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func, 0x4, &pcie_cmd_reg);
	pcie_cmd_reg|=(1<<0);
	pcie_cmd_reg|=(1<<1);
	pcie_cmd_reg|=(1<<2);
	pcie_cmd_reg|=(1<<10);
	pcie_write_dword(xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func, 0x4, pcie_cmd_reg);
	printf("XHCI controller virtual base: %p\r\n", (uint64_t)xhciInfo.pBaseMmio);
	if (xhci_reset()!=0){
		printf("failed to reset XHCI controller\r\n");
		return -1;
	}
	if (xhci_init_trb_list(&xhciInfo.cmdRingInfo)!=0){
		printf("failed to initialize cmd TRB ring list\r\n");
		return -1;
	}	
	if (xhci_init_device_context_list()!=0){
		printf("failed to initialize device context ptr list\r\n");
		return -1;
	}
	if (xhci_init_interrupter()!=0){
		printf("failed to initialize XHCI interrupter\r\n");
		return -1;
	}
	xhci_stop();
	uint8_t portCount = 0;
	if (xhci_get_port_count(&portCount)!=0)
		return -1;
	printf("port count: %d\r\n", portCount);
	struct xhci_trb trb = {0};
	uint64_t trbIndex = 0;
	memset((void*)&trb, 0, sizeof(struct xhci_trb));
	trb.command.control.type = XHCI_TRB_TYPE_NOP_CMD;
	trb.command.control.ioc = 1;
	trb.command.control.cycle_bit = xhciInfo.cmdRingInfo.cycle_state;
	trb.command.control.init_target = 0;
	if (xhci_alloc_trb(&xhciInfo.cmdRingInfo, trb, &trbIndex)!=0){
		printf("failed to allocate TRB entry\r\n");
		return -1;
	}
	uint64_t cmdRingBase = 0;
	xhci_get_cmd_ring_base(&cmdRingBase);
	printf("cmd ring base: %p\r\n", cmdRingBase);
	xhci_start();
	printf("running TRBs\r\n");
	xhci_ring(0);
	while (!xhci_get_cmd_ring_running()){};
	printf("done running TRBs\r\n");
	if (xhciInfo.pOperational->usb_status.host_system_error)
		printf("host system error\r\n");
	if (xhciInfo.pOperational->usb_status.host_controller_error)
		printf("host controller error\r\n");
	xhci_get_cmd_ring_base(&cmdRingBase);
	printf("cmd ring base: %p\r\n", cmdRingBase);
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
	if (pcie_get_bar(pInfo->location.bus, pInfo->location.dev, pInfo->location.func, 0, &pInfo->pBaseMmio_physical)!=0)
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
	printf("doorbell offset: %d\r\n", doorbellOffset);
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
int xhci_read_qword(volatile uint64_t* pQword, uint64_t* pValue){
	if (!pQword||!pValue)
		return -1;
	uint32_t low = *(volatile uint32_t*)pQword;
	uint32_t high = *(((volatile uint32_t*)pQword)+1);
	*pValue = *pQword;
	return 0;
}
int xhci_write_qword(volatile uint64_t* pQword, uint64_t value){
	if (!pQword)
		return -1;
	uint32_t low = (uint32_t)(value&0xFFFFFFFF);
	uint32_t high = (uint32_t)((value>>32)&0xFFFFFFFF);
	*pQword = value;
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
	uint64_t ringBufferSize = pRingInfo->maxEntries*sizeof(struct xhci_trb);
	pRingInfo->ringBufferSize = ringBufferSize;
	if (virtualToPhysical((uint64_t)pRingBuffer, &pRingInfo->pRingBuffer_phys)!=0){
		printf("failed to get physical address of TRB ring buffer\r\n");
		virtualFreePage((uint64_t)pRingBuffer, 0);
		return -1;
	}
	memset((void*)pRingBuffer, 0, ringBufferSize);
	struct xhci_trb linkTrb = {0};
	memset((void*)&linkTrb, 0, sizeof(struct xhci_trb));
	linkTrb.generic.ptr = (uint64_t)pRingInfo->pRingBuffer_phys;
	linkTrb.generic.control.tc_bit = 1;
	linkTrb.generic.control.type = XHCI_TRB_TYPE_LINK;
	if (xhci_write_trb(pRingInfo, XHCI_MAX_CMD_TRB_ENTRIES-1, linkTrb)!=0){
		xhci_deinit_trb_list(pRingInfo);
		return -1;
	}
	printf("ring buffer physical address: %p\r\n", pRingInfo->pRingBuffer_phys);
	xhci_set_cmd_ring_base((uint64_t)pRingInfo->pRingBuffer_phys);
	return 0;
}
int xhci_deinit_trb_list(struct xhci_trb_ring_info* pRingInfo){
	if (!pRingInfo)
		return -1;
	if (virtualFreePage((uint64_t)pRingInfo->pRingBuffer, 0)!=0)
		return -1;
	return 0;
}
int xhci_alloc_trb(struct xhci_trb_ring_info* pRingInfo, struct xhci_trb trb, uint64_t* pTrbIndex){
	if (!pRingInfo||!pTrbIndex)
		return -1;
	uint64_t trbIndex = pRingInfo->currentEntry;
	if (trbIndex>XHCI_MAX_CMD_TRB_ENTRIES-1)
		trbIndex = 0;
	volatile struct xhci_trb* pTrbEntry = (volatile struct xhci_trb*)0x0;
	xhci_get_trb(pRingInfo, trbIndex, &pTrbEntry);
	uint64_t startIndex = trbIndex;
	while (pTrbEntry->generic.control.cycle_bit!=pRingInfo->cycle_state){
		xhci_get_trb(pRingInfo, trbIndex, &pTrbEntry);
		trbIndex++;
		if (trbIndex==startIndex)
			return -1;
	}
	xhci_write_trb(pRingInfo, trbIndex, trb);
	*pTrbIndex = trbIndex;
	return 0;
}
int xhci_get_trb(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry){
	if (!pRingInfo||!ppTrbEntry)
		return -1;
	*ppTrbEntry = xhciInfo.cmdRingInfo.pRingBuffer+trbIndex;
	return 0;
}
int xhci_write_trb(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb trbEntry){
	if (!pRingInfo)
		return -1;
	unsigned char cycle_state = 0;
	if (xhci_get_driver_cycle_state(&cycle_state)!=0)
		return -1;
	trbEntry.generic.control.cycle_bit = cycle_state;
	*(pRingInfo->pRingBuffer+trbIndex) = trbEntry;
	return 0;
}
int xhci_read_trb(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb* pTrbEntry){
	if (!pRingInfo||!pTrbEntry)
		return -1;
	*pTrbEntry = *(pRingInfo->pRingBuffer+trbIndex);
	return 0;
}
int xhci_ring(uint64_t doorbell_vector){
	volatile uint32_t* pDoorBell = xhciInfo.pDoorBells+doorbell_vector;
	*pDoorBell = 0;
	sleep(5);
	return 0;
}
int xhci_reset(void){
	struct xhci_usb_cmd cmd = xhciInfo.pOperational->usb_cmd;
	cmd.hc_reset = 1;
	xhciInfo.pOperational->usb_cmd = cmd;
	uint64_t start_us = get_time_us();
	struct xhci_usb_status status = xhciInfo.pOperational->usb_status;
	while (!status.halted||status.controller_not_ready){
		uint64_t current_us = get_time_us();
		uint64_t elapsed_us = current_us-start_us;
		if (elapsed_us>500000){
			printf("controller reset timed out\r\n");
			return -1;
		}
		status = xhciInfo.pOperational->usb_status;
	}
	sleep(250);
	xhci_stop();
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
	union xhci_cmd_ring_ctrl cmdRingControl = {0};
	xhci_read_qword((uint64_t*)&xhciInfo.pOperational->cmd_ring_ctrl, (uint64_t*)&cmdRingControl);
	printf("value: %p\r\n", *(uint64_t*)&cmdRingControl);
	*pBase = cmdRingControl.base;
	return 0;
}
int xhci_set_cmd_ring_base(uint64_t base){
	union xhci_cmd_ring_ctrl cmdRingControl = {0};
	xhci_read_qword((uint64_t*)&xhciInfo.pOperational->cmd_ring_ctrl, (uint64_t*)&cmdRingControl);
	cmdRingControl.base = base;
	printf("base: %p\r\n", cmdRingControl.base);
	return xhci_write_qword((uint64_t*)&xhciInfo.pOperational->cmd_ring_ctrl, *(uint64_t*)&cmdRingControl);
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
	memset((void*)&pRingInfo, 0, sizeof(struct xhci_event_trb_ring_info));
	return 0;
}
int xhci_init_interrupter(void){
	volatile struct xhci_interrupter* pInt = (volatile struct xhci_interrupter*)0x0;
	if (xhci_get_interrupter_base(XHCI_DEFAULT_INTERRUPTER_ID, &pInt)!=0){
		printf("failed to get interrupter registers\r\n");
		return -1;
	}
	volatile struct pcie_msix_msg_ctrl* pMsgControl = (volatile struct pcie_msix_msg_ctrl*)0x0;
	if (pcie_get_cap_ptr(xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func, 0x11, (struct pcie_cap_link**)&pMsgControl)!=0){
		printf("failed to get MSIX capatability\r\n");
		return -1;
	}
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
	volatile struct xhci_segment_table_entry* pSegmentTableEntry = (volatile struct xhci_segment_table_entry*)0x0;
	if (virtualAllocPage((uint64_t*)&pSegmentTableEntry, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to allocate segment table entry\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		return -1;
	}
	uint64_t pSegmentTableEntry_phys = 0;
	if (virtualToPhysical((uint64_t)pSegmentTableEntry, (uint64_t*)&pSegmentTableEntry_phys)!=0){
		printf("failed to get physical address of segment table entry\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		virtualFreePage((uint64_t)pSegmentTableEntry, 0);
		return -1;
	}
	xhciInfo.interrupterInfo.eventRingInfo.pSegmentTableEntry = pSegmentTableEntry;
	xhciInfo.interrupterInfo.eventRingInfo.pSegmentTableEntry_phys = pSegmentTableEntry_phys;
	pSegmentTableEntry->base = (uint64_t)xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys;
	pSegmentTableEntry->size = XHCI_MAX_CMD_TRB_ENTRIES;
	pSegmentTableEntry->reserved0 = 0;
	volatile struct pcie_msix_table_entry* pMsixTableBase = (volatile struct pcie_msix_table_entry*)0x0;
	if (pcie_msix_get_table_base(xhciInfo.location.bus, xhciInfo.location.dev, xhciInfo.location.func, pMsgControl, &pMsixTableBase)!=0){
		printf("failed to get MSIX table base\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		virtualFreePage((uint64_t)pSegmentTableEntry, 0);
		return -1;
	}
	if (virtualMapPage((uint64_t)pMsixTableBase, (uint64_t)pMsixTableBase, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map MSIX table\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		virtualFreePage((uint64_t)pSegmentTableEntry, 0);
		return -1;
	}
	volatile struct pcie_msix_table_entry* pMsixEntry = pMsixTableBase;
	if (pcie_set_msix_entry(pMsixEntry, lapic_id, interrupterVector)!=0){
		printf("failed to set MSIX entry\r\n");
		xhci_deinit_trb_event_list(&xhciInfo.interrupterInfo.eventRingInfo);
		virtualFreePage((uint64_t)pSegmentTableEntry, 0);
		return -1;
	}
	idt_add_entry(interrupterVector, (uint64_t)xhci_interrupter_isr, 0x8E, 0x0);
	printf("event ring segment table base: %p\r\n", (uint64_t)xhciInfo.interrupterInfo.eventRingInfo.pSegmentTableEntry_phys);
	xhci_write_qword((volatile uint64_t*)&pInt->event_ring_segment_table_base, xhciInfo.interrupterInfo.eventRingInfo.pSegmentTableEntry_phys);
	struct xhci_interrupter_segment_table_size tableSize = {0};
	tableSize.reserved0 = 0;
	tableSize.table_size = 1;
	pInt->event_ring_segment_table_size = tableSize;
	uint32_t interrupt_moderation = pInt->interrupt_moderation;
	*(uint16_t*)&interrupt_moderation = 0x0;
	pInt->interrupt_moderation = interrupt_moderation;
	uint32_t interrupt_management = pInt->interrupt_management;
	interrupt_management = (interrupt_management&~0x2)|0x2;
	pInt->interrupt_management = interrupt_management;
	union xhci_cmd_ring_ctrl cmdRingControl = {0};
	xhci_read_qword((volatile uint64_t*)&xhciInfo.pOperational->cmd_ring_ctrl, (uint64_t*)&cmdRingControl);
	unsigned char cycle_state = 0;
	xhci_get_driver_cycle_state(&cycle_state);
	cmdRingControl.ring_cycle_state = cycle_state;
	xhci_write_qword((volatile uint64_t*)&xhciInfo.pOperational->cmd_ring_ctrl, *(uint64_t*)&cmdRingControl);
	struct xhci_usb_cmd usb_cmd = xhciInfo.pOperational->usb_cmd;
	usb_cmd.interrupter_enable = 1;
	xhciInfo.pOperational->usb_cmd = usb_cmd;
	struct xhci_dequeue_ring_ptr dequeue_ring_ptr = {0};
	xhci_read_qword((volatile uint64_t*)&pInt->dequeue_ring_ptr, (uint64_t*)&dequeue_ring_ptr);
	dequeue_ring_ptr.event_handler_busy = 0;
	dequeue_ring_ptr.event_ring_dequeue_ptr = (uint64_t)xhciInfo.interrupterInfo.eventRingInfo.pRingBuffer_phys;
	xhci_write_qword((volatile uint64_t*)&pInt->dequeue_ring_ptr, *(uint64_t*)&dequeue_ring_ptr);
	pMsgControl->msix_enable = 1;
	pMsgControl->vector_mask = 0;
	return 0;
}
int xhci_send_ack(uint64_t interrupter_id){
	volatile struct xhci_interrupter* pInt = (volatile struct xhci_interrupter*)0x0;
	if (xhci_get_interrupter_base(interrupter_id, &pInt)!=0)
		return -1;
	uint32_t iman = pInt->interrupt_management;
	iman|=(1<<0);
	pInt->interrupt_management = iman;	
	return 0;
}
int xhci_interrupter(void){
	printf("XHCI interrupter\r\n");
	xhci_send_ack(0);
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
