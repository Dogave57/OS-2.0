#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "cpu/mutex.h"
#include "drivers/pcie.h"
#include "subsystem/pcie.h"
#include "drivers/dma-device.h"
static struct dma_device_driver_info driverInfo = {0};
int dma_device_driver_init(void){
	uint64_t driverId = 0;
	struct pcie_driver_vtable driverVtable = {0};
	memset((void*)&driverVtable, 0, sizeof(struct pcie_driver_vtable));
	driverVtable.registerFunction = (pcieDriverRegisterFunction)dma_device_register;
	driverVtable.unregisterFunction = (pcieDriverUnregisterFunction)dma_device_unregister;
	if (pcie_subsystem_driver_register(driverVtable, DMA_DEVICE_PCIE_CLASS_ID, DMA_DEVICE_PCIE_SUBCLASS_ID, &driverId)!=0){
		printf("failed to register custom bulk DMA host controller driver\r\n");
		return -1;
	}
	driverInfo.driverId = driverId;
	return 0;
}
int dma_device_register(struct pcie_location location){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	uint16_t vendorId = 0;
	uint16_t deviceId = 0;
	if (pcie_get_vendor_id(location, &vendorId)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (vendorId!=DMA_DEVICE_PCIE_VENDOR_ID){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pcie_get_device_id(location, &deviceId)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (deviceId!=DMA_DEVICE_PCIE_DEVICE_ID){
		mutex_unlock(&mutex);
		return -1;
	}
	printf("custom bulk DMA host controller at bus: %d, device: %d, function: %d connected to PCIe interface\r\n", location.bus, location.dev, location.func);
	struct dma_device_base_registers* pBaseRegisters = (struct dma_device_base_registers*)0x0;
	uint64_t pBaseRegisters_phys = 0;
	if (pcie_get_bar(location, 0, &pBaseRegisters_phys)!=0){
		printf("failed to get custom bulk DMA host controller BAR0\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	printf("custom bulk DMA host controller BAR0: %d\r\n", pBaseRegisters_phys);
	if (virtualGetSpace((uint64_t*)&pBaseRegisters, PAGE_SIZE)!=0){
		printf("failed to get virtual addressing space for custom bulk DMA host controller BAR0\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (virtualMapPage((uint64_t)pBaseRegisters_phys, (uint64_t)pBaseRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map custom bulk DMA host controller BAR0 physical page\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int dma_device_unregister(struct pcie_location location){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	printf("custom bulk DMA host controller at bus: %d, device: %d, function: %d disconnected from PCIe interface\r\n", location.bus, location.dev, location.func);
	mutex_unlock(&mutex);
	return 0;
}
