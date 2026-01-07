#ifndef _VIRTIO_GPU
#define _VIRTIO_GPU
#include "drivers/pcie.h"
#include "drivers/virtio.h"
#define VIRTIO_GPU_DEVICE_ID 0x1050
struct virtio_gpu_base_mmio{
	
}__attribute__((packed));
struct virtio_gpu_info{
	struct pcie_location location;
	volatile struct virtio_gpu_base_mmio* pBaseMmio;
};
int virtio_gpu_init(void);
int virtio_gpu_get_info(struct virtio_gpu_info* pInfo);
#endif
