#ifndef _PCIE_SUBSYSTEM
#define _PCIE_SUBSYSTEM
#include "subsystem/subsystem.h"
#include "drivers/pcie.h"
typedef int(*pcieDriverRegisterFunction)(struct pcie_location location);
typedef int(*pcieDriverUnregisterFunction)(struct pcie_location location);
struct pcie_driver_vtable{
	pcieDriverRegisterFunction registerFunction;
	pcieDriverUnregisterFunction unregisterFunction;
};
struct pcie_driver_desc{
	struct pcie_driver_vtable vtable;
	uint16_t class;
	uint16_t subClass;
	struct pcie_driver_desc* pFlink;
	struct pcie_driver_desc* pBlink;
};
struct pcie_bus_desc{
	uint8_t bus;
	uint64_t deviceCount;
};
struct pcie_device_desc{
	struct pcie_location location;
	uint64_t functionCount;
	uint64_t unresolvedCount;
};
struct pcie_function_desc{
	struct pcie_location location;
	uint64_t driverId;
	uint8_t resolved;
};
struct pcie_bus_desc_list_info{
	struct pcie_bus_desc* pBusDescList;
	uint64_t busDescListSize;
};
struct pcie_device_desc_list_info{
	struct pcie_device_desc* pDeviceDescList;
	uint64_t deviceDescListSize;
};
struct pcie_function_desc_list_info{
	struct pcie_function_desc* pFunctionDescList;
	uint64_t functionDescListSize;
};
int pcie_subsystem_init(void);
int pcie_subsystem_init_bus_desc_list(void);
int pcie_subsystem_deinit_bus_desc_list(void);
int pcie_subsystem_init_device_desc_list(void);
int pcie_subsystem_deinit_device_desc_list(void);
int pcie_subsystem_init_function_desc_list(void);
int pcie_subsystem_deinit_function_desc_list(void);
int pcie_subsystem_get_bus_desc(uint8_t bus, struct pcie_bus_desc** ppBusDesc);
int pcie_subsystem_get_device_desc(struct pcie_location location, struct pcie_device_desc** ppDeviceDesc);
int pcie_subsystem_get_function_desc(struct pcie_location location, struct pcie_function_desc** ppFunctionDesc);
int pcie_subsystem_get_driver_desc(uint64_t driverId, struct pcie_driver_desc** ppDriverDesc);
int pcie_subsystem_driver_register(struct pcie_driver_vtable vtable, uint16_t class, uint16_t subClass, uint64_t* pDriverId);
int pcie_subsystem_driver_unregister(uint64_t driverId);
int pcie_subsystem_resolve_function_drivers(uint64_t driverId);
#endif
