#ifndef _DMA_DEVICE
#define _DMA_DEVICE
#include "drivers/pcie.h"
#define DMA_DEVICE_PCIE_VENDOR_ID (0xABCD)
#define DMA_DEVICE_PCIE_DEVICE_ID (0xFEEE)
#define DMA_DEVICE_PCIE_CLASS_ID (0x07)
#define DMA_DEVICE_PCIE_SUBCLASS_ID (0x00)
#define DMA_DEVICE_SIGNATURE (0xDEADBEEF)
struct dma_device_base_registers{
	volatile uint32_t signature;
}__attribute__((packed));
struct dma_device_driver_info{
	uint64_t driverId;
};
int dma_device_driver_init(void);
int dma_device_register(struct pcie_location location);
int dma_device_unregister(struct pcie_location location);
#endif
