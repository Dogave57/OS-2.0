#ifndef _VIRTIO_GPU
#define _VIRTIO_GPU
#include "drivers/pcie.h"
#include "drivers/virtio.h"
#define VIRTIO_GPU_DEVICE_ID 0x1050
#define VIRTIO_BLOCK_BASE_REGISTERS (0x01)
#define VIRTIO_BLOCK_NOTIFY_REGISTERS (0x02)
#define VIRTIO_BLOCK_INTERRUPT_REGISTERS (0x03)
#define VIRTIO_BLOCK_DEVICE_REGISTERS (0x04)
#define VIRTIO_BLOCK_PCIE_REGISTERS (0x05)
struct virtio_gpu_pcie_config_cap{
	struct pcie_cap_link link;
	uint8_t cap_size;
	uint8_t config_type;
	uint8_t bar;
	uint8_t padding0[3];
	uint32_t bar_offset;
	uint32_t register_block_size;
}__attribute__((packed));
struct virtio_gpu_base_registers{
	
}__attribute__((packed));
struct virtio_gpu_interrupt_registers{

}__attribute__((packed));
struct virtio_gpu_device_registers{

}__attribute__((packed));
struct virtio_gpu_pcie_registers{

}__attribute__((packed));
struct virtio_gpu_info{
	struct pcie_location location;
	volatile struct virtio_gpu_base_registers* pBaseRegisters;
	volatile uint32_t* pNotifyRegisters;
	volatile struct virtio_gpu_interrupt_registers* pInterruptRegisters;
	volatile struct virtio_gpu_device_registers* pDeviceRegisters;
	volatile struct virtio_gpu_pcie_registers* pPcieRegisters;
};
int virtio_gpu_init(void);
int virtio_gpu_get_info(struct virtio_gpu_info* pInfo);
#endif
