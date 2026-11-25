#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "align.h"
#include "cpu/idt.h"
#include "cpu/mutex.h"
#include "stdlib/stdlib.h"
#include "subsystem/pcie.h"
#include "subsystem/audio.h"
#include "drivers/apic.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "drivers/audio/intel-hda.h"
static struct intel_hda_driver_info driverInfo = {0};
int intel_hda_driver_init(void){
	struct pcie_driver_vtable pcieDriverVtable = {0};
	memset((void*)&pcieDriverVtable, 0x00, sizeof(struct pcie_driver_vtable));
	pcieDriverVtable.registerFunction = (pcieDriverRegisterFunction)intel_hda_driver_pcie_register;
	pcieDriverVtable.unregisterFunction = (pcieDriverUnregisterFunction)intel_hda_driver_pcie_unregister;
	uint64_t pcieDriverId = 0x00;
	if (pcie_subsystem_driver_register(pcieDriverVtable, PCIE_CLASS_MULTI_MEDIA_DEVICE, 0x03, &pcieDriverId)!=0){
		printf("failed to register Intel HDA protocol specification driver with PCIe subsystem\r\n");
		return -1;
	}
	struct audio_subsystem_driver_vtable driverVtable = {0};
	memset((void*)&driverVtable, 0x00, sizeof(struct audio_subsystem_driver_vtable));
	uint64_t driverId = 0x00;
	if (audio_subsystem_driver_register(driverVtable, &driverId)!=0){
		printf("failed to register Intel HDA protocol specification driver with audio subsystem\r\n");
		return -1;
	}
	driverInfo.pcieDriverId = pcieDriverId;
	driverInfo.driverId = driverId;
	return 0x00;
}
int intel_hda_driver_deinit(void){
	if (pcie_subsystem_driver_unregister(driverInfo.pcieDriverId)!=0){
		printf("failed to unregister Intel HDA protocol specification driver with PCIe subsystem\r\n");
		audio_subsystem_driver_unregister(driverInfo.driverId);
		return -1;
	}
	if (audio_subsystem_driver_unregister(driverInfo.driverId)!=0){
		printf("failed to unregister Intel HDA protocol specification driver with audio subsystem\r\n");
		return -1;
	}
	return 0x00;
}
int intel_hda_driver_reset(struct intel_hda_controller_desc* pControllerDesc){
	if (!pControllerDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct intel_hda_base_registers* pBaseRegisters = pControllerDesc->pBaseRegisters;
	struct intel_hda_global_control globalControl = pBaseRegisters->global_control;
	globalControl.reset = 0x00;
	pBaseRegisters->global_control = globalControl;
	sleep(5);
	globalControl = pBaseRegisters->global_control;
	if (globalControl.reset){
		printf("failed to reset Intel HDA protocol specification audio host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	globalControl.reset = 0x01;
	pBaseRegisters->global_control = globalControl;
	sleep(5);
	globalControl = pBaseRegisters->global_control;
	if (!globalControl.reset){
		printf("failed to reset Intel HDA protocol specification audio host controller\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0x00;
}
int intel_hda_driver_corb_run(struct intel_hda_controller_desc* pControllerDesc){
	if (!pControllerDesc)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct intel_hda_base_registers* pBaseRegisters = pControllerDesc->pBaseRegisters;
	struct intel_hda_corb_control corbControl = pBaseRegisters->corb_control;
	corbControl.corb_dma_enable = 0x00;
	pBaseRegisters->corb_control = corbControl;
	while (1){
		corbControl = pBaseRegisters->corb_control;
		if (corbControl.corb_dma_enable)
			continue;
		break;
	}
	uint16_t responseInterruptCount = (uint16_t)(pControllerDesc->currentResponseIndex+(pControllerDesc->commandPushCount-pControllerDesc->lastCommandPushCount));
	responseInterruptCount = responseInterruptCount%pControllerDesc->responseDescCount;
	pBaseRegisters->response_interrupt_count = responseInterruptCount;
	corbControl = pBaseRegisters->corb_control;
	corbControl.corb_dma_enable = 0x01;
	pBaseRegisters->corb_control = corbControl;
	while (1){
		corbControl = pBaseRegisters->corb_control;
		if (!corbControl.corb_dma_enable)
			continue;
		break;
	}
	pBaseRegisters->corb_write_index = (uint16_t)pControllerDesc->currentCommandIndex;
	pControllerDesc->lastCommandPushCount = pControllerDesc->commandPushCount;
	mutex_unlock(&mutex);
	return 0x00;
}
int intel_hda_driver_corb_running(struct intel_hda_controller_desc* pControllerDesc){
	if (!pControllerDesc)
		return -1;
	struct intel_hda_base_registers* pBaseRegisters = pControllerDesc->pBaseRegisters;
	uint16_t corbReadIndex = pBaseRegisters->corb_read_index;
	return (corbReadIndex!=pControllerDesc->currentCommandIndex) ? 0x00 : -1;
}
int intel_hda_driver_command_push(struct intel_hda_controller_desc* pControllerDesc, struct intel_hda_command_desc commandDesc){
	if (!pControllerDesc)
		return -1;
	struct intel_hda_command_desc* pCommandDesc = pControllerDesc->pCommandDescList+pControllerDesc->nextCommandIndex;
	*pCommandDesc = commandDesc;
	pControllerDesc->currentCommandIndex = pControllerDesc->nextCommandIndex;
	pControllerDesc->nextCommandIndex = (pControllerDesc->nextCommandIndex+0x01)%pControllerDesc->commandDescCount;
	pControllerDesc->commandPushCount++;
	return 0x00;
}
int intel_hda_driver_interrupt(uint8_t vector){
	serial_print(0x00, "Intel HDA protocol specification host controller RIRB physical page data ISR trigger\r\n");
	return 0x00;
}
int intel_hda_driver_pcie_register(struct pcie_location location){
	uint16_t vendorId = 0x00;
	if (pcie_get_vendor_id(location, &vendorId)!=0){
		printf("failed to get audio host controller WORD vendor identifier\r\n");
		return -1;
	}
	if (vendorId!=0x8086)
		return -1;
	uint16_t deviceId = 0x00;
	if (pcie_get_device_id(location, &deviceId)!=0){
		printf("failed to get Intel HDA protocol specification host controller WORD device identifier\r\n");
		return -1;
	}
	struct pcie_function_desc* pFunctionDesc = (struct pcie_function_desc*)0x00;
	if (pcie_subsystem_get_function_desc(location, &pFunctionDesc)!=0){
		printf("failed to get PCIe subsystem Intel HDA protocol specification host controller routine descriptor\r\n");
		return -1;
	}
	struct intel_hda_controller_desc* pControllerDesc = (struct intel_hda_controller_desc*)0x00;
	if (heap_alloc(0x01, (unsigned char**)&pControllerDesc, sizeof(struct intel_hda_controller_desc))!=0){
		printf("failed to allocate Intel HDA protocol specification host controller descriptor\r\n");
		return -1;
	}
	memset((void*)pControllerDesc, 0x00, sizeof(struct intel_hda_controller_desc));
	printf("Intel HDA protocol specification host controller bus: %d, device: %d, routine: %d\r\n", location.bus, location.dev, location.func);
	printf("Intel HDA protocol specification host controller device identifier: 0x%x\r\n", deviceId);
	uint64_t pBaseRegisters_phys = 0x00;
	if (pcie_get_bar(location, 0x00, &pBaseRegisters_phys)!=0){
		printf("failed to get Intel HDA protocol specification host controller BAR0 QWORD physical address\r\n");
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	struct intel_hda_base_registers* pBaseRegisters = (struct intel_hda_base_registers*)0x00;
	uint64_t baseRegistersSize = PAGE_SIZE*0x04;
	if (virtualGetSpace((uint64_t*)&pBaseRegisters, baseRegistersSize)!=0){
		printf("failed to get Intel HDA protocol specification base registers virtual addressing volatile segment\r\n");
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	if (virtualMapPages(pBaseRegisters_phys, (uint64_t)pBaseRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, align_up(baseRegistersSize, PAGE_SIZE)/PAGE_SIZE, 0x01, 0x00)!=0){
		printf("failed to map Intel HDA protocol specification base register physical pages\r\n");
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	pControllerDesc->pcieLocation = location;
	pControllerDesc->pBaseRegisters_phys = pBaseRegisters_phys;
	pControllerDesc->pBaseRegisters = pBaseRegisters;
	if (intel_hda_driver_reset(pControllerDesc)!=0){
		printf("failed to reset Intel HDA protocol specification host controller\r\n");
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	uint64_t maxCommandDescCount = 0x00;
	uint64_t maxResponseDescCount = 0x00;
	struct intel_hda_corb_size corbSize = pBaseRegisters->corb_size;
	struct intel_hda_rirb_size rirbSize = pBaseRegisters->rirb_size;
	static const uint64_t maxDescriptorCountList[0x03]={
		[0x00]=0x100,
		[0x01]=0x100,
		[0x02]=0x02,
	};
	static const uint8_t descriptorCountList[0x03]={
		[0x00]=(1<<0),
		[0x01]=(1<<1),
		[0x02]=0x00,
	};
	for (uint64_t i = 0x00;i<0x03;i++){
		if (!(corbSize.corb_size_cap&(1<<(i+0x01))))
			continue;
		maxCommandDescCount = *(maxDescriptorCountList+i);
		if (0x00&&!(corbSize.corb_size_cap&~(1<<(i+0x01))))
			break;
		corbSize.corb_size = *(descriptorCountList+i);
		pBaseRegisters->corb_size = corbSize;
		break;
	}
	for (uint64_t i = 0x00;i<0x03;i++){
		if (!(rirbSize.rirb_size_cap&(1<<(i+0x01))))
			continue;
		maxResponseDescCount = *(maxDescriptorCountList+i);
		if (0x00&&!(rirbSize.rirb_size_cap&~(1<<(i+0x01))))
			break;
		rirbSize.rirb_size = *(descriptorCountList+i);
		pBaseRegisters->rirb_size = rirbSize;
		break;
	}
	printf("Intel HDA protocol specification host controller max command descriptor count: %d\r\n", maxCommandDescCount);
	printf("Intel HDA protocol specification host controller max response descriptor count: %d\r\n", maxResponseDescCount);
	struct intel_hda_command_desc* pCommandDescList = (struct intel_hda_command_desc*)0x00;
	uint64_t pCommandDescList_phys = 0x00;
	uint64_t commandDescCount = maxCommandDescCount;
	uint64_t commandDescListSize = sizeof(struct intel_hda_command_desc)*commandDescCount;
	if (virtualAllocPage((uint64_t*)&pCommandDescList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0x00)!=0){
		printf("failed to allocate Intel HDA protocol specification host controller command descriptor list physical volatile page\r\n");
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	if (virtualToPhysical((uint64_t)pCommandDescList, &pCommandDescList_phys)!=0){
		printf("failed to get Intel HDA protocol specification host controller command descriptor list QWORD physical volatile address\r\n");
		virtualFreePage((uint64_t)pCommandDescList, 0x00);
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	struct intel_hda_response_desc* pResponseDescList = (struct intel_hda_response_desc*)0x00;
	uint64_t pResponseDescList_phys = 0x00;
	uint64_t responseDescCount = maxResponseDescCount;
	uint64_t responseDescListSize = sizeof(struct intel_hda_response_desc)*responseDescCount;
	if (virtualAllocPage((uint64_t*)&pResponseDescList, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 0x00)!=0){
		printf("failed to allocate Intel HDA protocol specification host controller response descriptor list physical volatile page\r\n");
		virtualFreePage((uint64_t)pCommandDescList, 0x00);
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	if (virtualToPhysical((uint64_t)pResponseDescList, &pResponseDescList_phys)!=0){
		printf("failed to get Intel HDA protocol specification host controller response descriptor list QWORD physical volatile address\r\n");
		virtualFreePage((uint64_t)pCommandDescList, 0x00);
		virtualFreePage((uint64_t)pResponseDescList, 0x00);
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	memset_64((void*)pResponseDescList, 0x00, responseDescListSize);
	struct intel_hda_interrupt_desc* pInterruptDescList = (struct intel_hda_interrupt_desc*)0x00;
	uint64_t interruptDescListSize = sizeof(struct intel_hda_interrupt_desc)*0x100;
	if (virtualAlloc((uint64_t*)&pInterruptDescList, interruptDescListSize, PTE_RW|PTE_NX, 0x00)!=0){
		printf("failed to allocate Intel HDA protocol specification host controller ISR descriptor list physical pages\r\n");
		virtualFreePage((uint64_t)pCommandDescList, 0x00);
		virtualFreePage((uint64_t)pResponseDescList, 0x00);
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	memset_64((void*)pInterruptDescList, 0x00, interruptDescListSize);
	pBaseRegisters->corb_base = pCommandDescList_phys;
	pBaseRegisters->rirb_base = pResponseDescList_phys;
	pControllerDesc->pCommandDescList = pCommandDescList;
	pControllerDesc->pCommandDescList_phys = pCommandDescList_phys;
	pControllerDesc->commandDescListSize = commandDescListSize;
	pControllerDesc->commandDescCount = commandDescCount;
	pControllerDesc->currentCommandIndex = 0x00;
	pControllerDesc->nextCommandIndex = 0x01;
	pControllerDesc->commandPushCount = 0x00;
	pControllerDesc->lastCommandPushCount = 0x00;
	pControllerDesc->pResponseDescList = pResponseDescList;
	pControllerDesc->pResponseDescList_phys = pResponseDescList_phys;
	pControllerDesc->responseDescListSize = responseDescListSize;
	pControllerDesc->responseDescCount = responseDescCount;
	pControllerDesc->currentResponseIndex = 0x00;
	pControllerDesc->lastResponseWriteIndex = 0x00;
	pControllerDesc->pInterruptDescList = pInterruptDescList;
	pControllerDesc->interruptDescListSize = interruptDescListSize;
	uint64_t lapicId = 0x00;
	if (lapic_get_id(&lapicId)!=0){
		printf("failed to get QWORD LAPIC identifier\r\n");
		virtualFreePage((uint64_t)pCommandDescList, 0x00);
		virtualFreePage((uint64_t)pResponseDescList, 0x00);
		virtualFree((uint64_t)pInterruptDescList, interruptDescListSize);
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	uint8_t isrVector = 0x00;
	if (idt_get_free_vector(&isrVector)!=0){
		printf("failed to get free IDT physical page data entry descriptor\r\n");
		virtualFreePage((uint64_t)pCommandDescList, 0x00);
		virtualFreePage((uint64_t)pResponseDescList, 0x00);
		virtualFree((uint64_t)pInterruptDescList, interruptDescListSize);
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	if (idt_add_entry(isrVector, (uint64_t)intel_hda_driver_isr, 0x8E, 0x00, 0x00)!=0){
		printf("failed to add IDT physical page data entry descriptor\r\n");
		virtualFreePage((uint64_t)pCommandDescList, 0x00);
		virtualFreePage((uint64_t)pResponseDescList, 0x00);
		virtualFree((uint64_t)pInterruptDescList, interruptDescListSize);
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	struct pcie_msi_msg_ctrl* pMsiMsgControl = (struct pcie_msi_msg_ctrl*)0x00;
	struct pcie_msix_msg_ctrl* pMsixMsgControl = (struct pcie_msix_msg_ctrl*)0x00;
	if (!pcie_msix_get_msg_ctrl(location, &pMsixMsgControl)){
		if (pcie_msix_enable(pMsixMsgControl)!=0){
			printf("failed to enable Intel HDA protocol specification host controller MSI-X protocol specification ISR handling\r\n");
			virtualFreePage((uint64_t)pCommandDescList, 0x00);
			virtualFreePage((uint64_t)pResponseDescList, 0x00);
			virtualFree((uint64_t)pInterruptDescList, interruptDescListSize);
			heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
			return -1;
		}
		pcie_set_msix_entry(location, pMsixMsgControl, 0x00, lapicId, isrVector);
		pcie_msix_enable_entry(location, pMsixMsgControl, 0x00);
		printf("Intel HDA protocol specification host controller follows MSI-X ISR specification\r\n");
	}
	if (!pMsixMsgControl&&!pcie_msi_get_msg_ctrl(location, &pMsiMsgControl)){
		if (pcie_msi_enable(pMsiMsgControl)!=0){
			printf("failed to enable Intel HDA protocol specification host controller MSI protocol specification ISR handling\r\n");
			virtualFreePage((uint64_t)pCommandDescList, 0x00);
			virtualFreePage((uint64_t)pResponseDescList, 0x00);
			virtualFree((uint64_t)pInterruptDescList, interruptDescListSize);
			heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
			return -1;
		}
		pcie_set_msi_entry(location, pMsiMsgControl, 0x00, lapicId, isrVector);
		printf("Intel HDA protocol specification host controller follows MSI ISR specification\r\n");
	}
	if (!pMsiMsgControl&&!pMsixMsgControl){
		printf("Intel HDA protocol specification host controller lacks support in terms of MSI/MSI-X protocol specifications\r\n");
		heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
		return -1;
	}
	struct intel_hda_interrupt_control interruptControl = pBaseRegisters->interrupt_control;
	interruptControl.stream_interrupt_enable = 0x00;
	interruptControl.controller_interrupt_enable = 0x01;
	interruptControl.global_interrupt_enable = 0x01;
	pBaseRegisters->interrupt_control = interruptControl;
	uint16_t corbReadIndex = pBaseRegisters->corb_read_index;
	corbReadIndex|=(1<<15);
	pBaseRegisters->corb_read_index = corbReadIndex;
	while (1){
		corbReadIndex = pBaseRegisters->corb_read_index;
		if (!(corbReadIndex&(1<<15)))
			continue;
		corbReadIndex&=~(1<<15);
		pBaseRegisters->corb_read_index = corbReadIndex;
		break;
	}
	while (1){
		corbReadIndex = pBaseRegisters->corb_read_index;
		if (corbReadIndex&(1<<15))
			continue;
		break;
	}
	pBaseRegisters->corb_write_index = 0x00;
	struct intel_hda_corb_control corbControl = pBaseRegisters->corb_control;
	corbControl.corb_dma_enable = 0x01;
	pBaseRegisters->corb_control = corbControl;
	while (1){
		corbControl = pBaseRegisters->corb_control;
		if (!corbControl.corb_dma_enable)
			continue;
		break;
	}
	struct intel_hda_rirb_control rirbControl = pBaseRegisters->rirb_control;
	rirbControl.rirb_interrupt_enable = 0x01;
	rirbControl.rirb_dma_enable = 0x01;
	pBaseRegisters->rirb_control = rirbControl;
	uint16_t wakeStatus = pBaseRegisters->wake_status;
	for (uint64_t i = 0x00;i<0x10;i++){
		if (!(wakeStatus&(1<<i)))
			continue;
		printf("Intel HDA protocol specification host controller active codec in which contains identifier: %d\r\n", i);
		struct intel_hda_command_desc commandDesc = {0};
		memset((void*)&commandDesc, 0x00, sizeof(struct intel_hda_command_desc));
		commandDesc.payload_short.payload = 0x02;
		commandDesc.payload_short.verb_id = 0xF00;
		commandDesc.payload_short.node_id = 0x00;
		commandDesc.payload_short.codec_address = i;
		for (uint64_t i = 0x00;i<0x04;i++){
			for (uint64_t i = 0x00;i<0x04;i++){
				commandDesc.payload_short.payload = (i%0x02) ? 0x02 : 0x00;
				if (intel_hda_driver_command_push(pControllerDesc, commandDesc)!=0){
					printf("failed to push Intel HDA protocol specification host controller CORB physical page data DWORD command descriptor\r\n");
					virtualFreePage((uint64_t)pCommandDescList, 0x00);
					virtualFreePage((uint64_t)pResponseDescList, 0x00);
					virtualFree((uint64_t)pInterruptDescList, interruptDescListSize);
					return -1;
				}
			}
			if (intel_hda_driver_corb_run(pControllerDesc)!=0){
				printf("failed to run Intel HDA protocol specification host controller CORB physical page data\r\n");
				return -1;
			}
			while (!intel_hda_driver_corb_running(pControllerDesc)){};
			break;
		}
	}
	pFunctionDesc->driverReserve = (uint64_t)pControllerDesc;
	return 0x00;
}
int intel_hda_driver_pcie_unregister(struct pcie_location location){
	struct pcie_function_desc* pFunctionDesc = (struct pcie_function_desc*)0x00;
	if (pcie_subsystem_get_function_desc(location, &pFunctionDesc)!=0){
		printf("failed to get PCIe subsystem Intel HDA protocol specification host controller routine descriptor\r\n");
		return -1;
	}
	struct intel_hda_controller_desc* pControllerDesc = (struct intel_hda_controller_desc*)pFunctionDesc->driverReserve;
	if (!pControllerDesc){
		printf("invalid PCIe subsystem Intel HDA protocol specification host controller routin descriptor QWORD driver reserve\r\n");
		return -1;
	}
	virtualFreePage((uint64_t)pControllerDesc->pCommandDescList, 0x00);
	virtualFreePage((uint64_t)pControllerDesc->pResponseDescList, 0x00);
	virtualFree((uint64_t)pControllerDesc->pInterruptDescList, pControllerDesc->interruptDescListSize);
	heap_free(0x01, (unsigned char*)pControllerDesc, sizeof(struct intel_hda_controller_desc));
	return 0x00;
}
