#include "stdlib/stdlib.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/mutex.h"
#include "drivers/pcie.h"
#include "subsystem/subsystem.h"
#include "subsystem/pcie.h"
static struct subsystem_desc* pDriverSubsystem = (struct subsystem_desc*)0x0;
static struct pcie_driver_desc* pFirstDriverDesc = (struct pcie_driver_desc*)0x0;
static struct pcie_driver_desc* pLastDriverDesc = (struct pcie_driver_desc*)0x0;
static struct pcie_bus_desc_list_info busDescListInfo = {0};
static struct pcie_device_desc_list_info deviceDescListInfo = {0};
static struct pcie_function_desc_list_info functionDescListInfo = {0};
int pcie_subsystem_init(void){
	if (pcie_subsystem_init_bus_desc_list()!=0)
		return -1;
	if (pcie_subsystem_init_device_desc_list()!=0)
		return -1;
	if (pcie_subsystem_init_function_desc_list()!=0)
		return -1;
	if (subsystem_init(&pDriverSubsystem, MEM_KB*64)!=0)
		return -1;
	struct pcie_info pcieInfo = {0};
	if (pcie_get_info(&pcieInfo)!=0){
		pcie_subsystem_deinit_bus_desc_list();
		pcie_subsystem_deinit_device_desc_list();
		pcie_subsystem_deinit_function_desc_list();
		subsystem_deinit(pDriverSubsystem);
		return -1;
	}
	for (uint8_t bus = pcieInfo.startBus;bus<pcieInfo.endBus;bus++){
		struct pcie_bus_desc* pBusDesc = (struct pcie_bus_desc*)0x0;
		if (pcie_subsystem_get_bus_desc(bus, &pBusDesc)!=0){
			pcie_subsystem_deinit_bus_desc_list();
			pcie_subsystem_deinit_device_desc_list();
			pcie_subsystem_deinit_function_desc_list();
			subsystem_deinit(pDriverSubsystem);
			return -1;
		}
		for (uint8_t dev = 0;dev<32;dev++){
			struct pcie_device_desc* pDeviceDesc = (struct pcie_device_desc*)0x0;
			struct pcie_location location = {0};
			memset((void*)&location, 0, sizeof(struct pcie_location));
			location.bus = bus;
			location.dev = dev;
			if (pcie_function_exists(location)!=0)
				continue;
			if (pcie_subsystem_get_device_desc(location, &pDeviceDesc)!=0){
				pcie_subsystem_deinit_bus_desc_list();
				pcie_subsystem_deinit_device_desc_list();
				pcie_subsystem_deinit_function_desc_list();
				subsystem_deinit(pDriverSubsystem);
				return -1;
			}
			pBusDesc->deviceCount++;
			for (uint8_t func = 0;func<8;func++){
				struct pcie_function_desc* pFunctionDesc = (struct pcie_function_desc*)0x0;
				location.dev = dev;
				location.func = func;
				if (pcie_function_exists(location)!=0)
					continue;
				if (pcie_subsystem_get_function_desc(location, &pFunctionDesc)!=0){
					pcie_subsystem_deinit_bus_desc_list();
					pcie_subsystem_deinit_device_desc_list();
					pcie_subsystem_deinit_function_desc_list();
					subsystem_deinit(pDriverSubsystem);
					return -1;
				}
				pDeviceDesc->unresolvedCount++;
			}
		}
	}	
	return 0;
}
int pcie_subsystem_init_bus_desc_list(void){
	uint64_t busDescListSize = PCIE_MAX_BUS_COUNT*sizeof(struct pcie_bus_desc);
	struct pcie_bus_desc* pBusDescList = (struct pcie_bus_desc*)0x0;
	if (virtualAlloc((uint64_t*)&pBusDescList, busDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	busDescListInfo.pBusDescList = pBusDescList;
	busDescListInfo.busDescListSize = busDescListSize;
	return 0;
}
int pcie_subsystem_deinit_bus_desc_list(void){
	if (!busDescListInfo.pBusDescList||!busDescListInfo.busDescListSize)
		return -1;
	if (virtualFree((uint64_t)busDescListInfo.pBusDescList, busDescListInfo.busDescListSize)!=0)
		return -1;
	return 0;
}
int pcie_subsystem_init_device_desc_list(void){
	uint64_t deviceDescListSize = PCIE_MAX_DEVICE_COUNT*sizeof(struct pcie_device_desc);
	struct pcie_device_desc* pDeviceDescList = (struct pcie_device_desc*)0x0;
	if (virtualAlloc((uint64_t*)&pDeviceDescList, deviceDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0){
		return -1;
	}	
	deviceDescListInfo.pDeviceDescList = pDeviceDescList;
	deviceDescListInfo.deviceDescListSize = deviceDescListSize;
	return 0;
}
int pcie_subsystem_deinit_device_desc_list(void){
	if (!deviceDescListInfo.pDeviceDescList||!deviceDescListInfo.deviceDescListSize)
		return -1;
	if (virtualFree((uint64_t)deviceDescListInfo.pDeviceDescList, deviceDescListInfo.deviceDescListSize)!=0)
		return -1;
	return 0;
}
int pcie_subsystem_init_function_desc_list(void){
	uint64_t functionDescListSize = PCIE_MAX_FUNCTION_COUNT*sizeof(struct pcie_function_desc);	
	struct pcie_function_desc* pFunctionDescList = (struct pcie_function_desc*)0x0;
	if (virtualAlloc((uint64_t*)&pFunctionDescList, functionDescListSize, PTE_RW|PTE_NX, MAP_FLAG_LAZY, PAGE_TYPE_NORMAL)!=0)
		return -1;
	functionDescListInfo.pFunctionDescList = pFunctionDescList;
	functionDescListInfo.functionDescListSize = functionDescListSize;	
	return 0;
}
int pcie_subsystem_deinit_function_desc_list(void){
	if (!functionDescListInfo.pFunctionDescList||!functionDescListInfo.functionDescListSize)
		return -1;	
	if (virtualFree((uint64_t)functionDescListInfo.pFunctionDescList, functionDescListInfo.functionDescListSize)!=0)
		return -1;	
	return 0;
}
int pcie_subsystem_get_bus_desc(uint8_t bus, struct pcie_bus_desc** ppBusDesc){
	if (!ppBusDesc)
		return -1;
	struct pcie_bus_desc* pBusDesc = busDescListInfo.pBusDescList+(uint64_t)bus;
	*ppBusDesc = pBusDesc;	
	return 0;
}
int pcie_subsystem_get_device_desc(struct pcie_location location, struct pcie_device_desc** ppDeviceDesc){
	if (!ppDeviceDesc)
		return -1;
	struct pcie_device_desc* pDeviceDesc = deviceDescListInfo.pDeviceDescList+(PCIE_MAX_BUS_COUNT*location.bus)+location.dev;
	*ppDeviceDesc = pDeviceDesc;
	return 0;
}
int pcie_subsystem_get_function_desc(struct pcie_location location, struct pcie_function_desc** ppFunctionDesc){
	if (!ppFunctionDesc)
		return -1;
	struct pcie_function_desc* pFunctionDesc = functionDescListInfo.pFunctionDescList+(32*8*location.bus)+(8*location.dev)+location.func;
	*ppFunctionDesc = pFunctionDesc;
	return 0;
}
int pcie_subsystem_get_driver_desc(uint64_t driverId, struct pcie_driver_desc** ppDriverDesc){
	if (!ppDriverDesc)
		return -1;
	struct pcie_driver_desc* pDriverDesc = (struct pcie_driver_desc*)0x0;
	if (subsystem_get_entry(pDriverSubsystem, driverId, (uint64_t*)&pDriverDesc)!=0)
		return -1;
	*ppDriverDesc = pDriverDesc;
	return 0;
}
int pcie_subsystem_driver_register(struct pcie_driver_vtable vtable, uint16_t class, uint16_t subClass, uint64_t* pDriverId){
	if (!pDriverId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct pcie_driver_desc* pDriverDesc = (struct pcie_driver_desc*)kmalloc(sizeof(struct pcie_driver_desc));
	if (!pDriverDesc){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	memset((void*)pDriverDesc, 0, sizeof(struct pcie_driver_desc));
	pDriverDesc->vtable = vtable;
	pDriverDesc->class = class;
	pDriverDesc->subClass = subClass;
	if (!pFirstDriverDesc)
		pFirstDriverDesc = pDriverDesc;
	if (pLastDriverDesc){
		pDriverDesc->pBlink = pLastDriverDesc;
		pDriverDesc->pFlink = pDriverDesc;
	}
	pLastDriverDesc = pDriverDesc;
	uint64_t driverId = 0;
	if (subsystem_alloc_entry(pDriverSubsystem, (unsigned char*)pDriverDesc, &driverId)!=0){
		kfree((void*)pDriverDesc);	
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	if (pcie_subsystem_resolve_function_drivers(driverId)!=0){
		printf("failed to attempt to resolve unresolved functions with driver with ID %d\r\n", driverId);
		subsystem_free_entry(pDriverSubsystem, driverId);
		kfree((void*)pDriverDesc);
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}
	*pDriverId = driverId;
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int pcie_subsystem_driver_unregister(uint64_t driverId){
	static struct mutex_t mutex = {0};
	mutex_lock_isr_safe(&mutex);
	struct pcie_driver_desc* pDriverDesc = (struct pcie_driver_desc*)0x0;
	if (pcie_subsystem_get_driver_desc(driverId, &pDriverDesc)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	if (pDriverDesc==pFirstDriverDesc)
		pFirstDriverDesc = pDriverDesc->pFlink;
	if (pDriverDesc==pLastDriverDesc)
		pLastDriverDesc = pDriverDesc->pBlink;
	if (pDriverDesc->pBlink)
		pDriverDesc->pBlink->pFlink = pDriverDesc->pFlink;
	if (pDriverDesc->pFlink)
		pDriverDesc->pFlink->pBlink = pDriverDesc->pBlink;
	if (subsystem_free_entry(pDriverSubsystem, driverId)!=0){
		mutex_unlock_isr_safe(&mutex);
		return -1;
	}	
	mutex_unlock_isr_safe(&mutex);
	return 0;
}
int pcie_subsystem_resolve_function_drivers(uint64_t driverId){
	struct pcie_driver_desc* pDriverDesc = (struct pcie_driver_desc*)0x0;
	if (pcie_subsystem_get_driver_desc(driverId, &pDriverDesc)!=0)
		return -1;
	struct pcie_info pcieInfo = {0};
	if (pcie_get_info(&pcieInfo)!=0)
		return -1;
	for (uint8_t bus = pcieInfo.startBus;bus<pcieInfo.endBus;bus++){
		for (uint8_t dev = 0;dev<32;dev++){
			struct pcie_location location = {0};
			memset((void*)&location, 0, sizeof(struct pcie_location));
			location.bus = bus;
			location.dev = dev;
			if (pcie_function_exists(location)!=0)
				continue;
			struct pcie_device_desc* pDeviceDesc = (struct pcie_device_desc*)0x0;
			if (pcie_subsystem_get_device_desc(location, &pDeviceDesc)!=0){
				continue;
			}
			for (uint8_t func = 0;func<8&&pDeviceDesc->unresolvedCount;func++){
				location.func = func;
				struct pcie_function_desc* pFunctionDesc = (struct pcie_function_desc*)0x0;
				if (pcie_subsystem_get_function_desc(location, &pFunctionDesc)!=0){
					continue;
				}
				if (pFunctionDesc->resolved)
					continue;
				uint8_t class = 0;
				uint8_t subClass = 0;
				if (pcie_get_class(location, &class)!=0)
					continue;
				if (class!=pDriverDesc->class)
					continue;
				if (pcie_get_subclass(location, &subClass)!=0)
					continue;
				if (subClass!=pDriverDesc->subClass)
					continue;
				if (pDriverDesc->vtable.registerFunction(location)!=0){
					printf("failed to resolve PCIe function at bus %d, device %d, function %d with driver with ID: %d\r\n", location.bus, location.dev, location.func, driverId);
					continue;
				}
				pFunctionDesc->resolved = 1;
				pFunctionDesc->driverId = driverId;
				pDeviceDesc->unresolvedCount--;
			}
		}
	}	
	return 0;
}
