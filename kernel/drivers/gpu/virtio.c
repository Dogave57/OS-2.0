#include "stdlib/stdlib.h"
#include "subsystem/subsystem.h"
#include "mem/vmm.h"
#include "drivers/pcie.h"
#include "drivers/virtio.h"
#include "drivers/gpu/virtio.h"
struct virtio_gpu_info gpuInfo = {0};
int virtio_gpu_init(void){
	if (virtio_gpu_get_info(&gpuInfo)!=0){
		printf("failed to get virtio GPU driver info\r\n");
		return -1;
	}	
	printf("virtual I/O GPU MMIO physical base: %p\r\n", (uint64_t)gpuInfo.pBaseMmio);
	uint64_t virtualSpace = 0;
	if (virtualGetSpace(&virtualSpace, 1)!=0){
		printf("failed to get virtual space for virtual I/O GPU base MMIO\r\n");
		return -1;
	}
	return 0;
	printf("virtual I/O GPU virtual MMIO base: %p\r\n", virtualSpace);
	if (virtualMapPage(((uint64_t)gpuInfo.pBaseMmio), (uint64_t)virtualSpace, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_NORMAL)!=0){
		printf("failed to map virtual I/O GPU base MMIO page\r\n");
		return -1;
	}
	gpuInfo.pBaseMmio = (volatile struct virtio_gpu_base_mmio*)virtualSpace;
	printf("VIRTIO GPU driver bus: %d, dev: %d, func: %d\r\n", gpuInfo.location.bus, gpuInfo.location.dev, gpuInfo.location.func);
	return 0;
}
int virtio_gpu_get_info(struct virtio_gpu_info* pInfo){
	if (!pInfo)
		return -1;
	if (gpuInfo.pBaseMmio){
		*pInfo = gpuInfo;
		return 0;
	}
	if (pcie_get_device_by_id(VIRTIO_VENDOR_ID, VIRTIO_GPU_DEVICE_ID, &gpuInfo.location.bus, &gpuInfo.location.dev, &gpuInfo.location.func)!=0){
		printf("failed to get virtio GPU controller PCIe location\r\n");
		return -1;
	}
	if (pcie_get_bar(gpuInfo.location.bus, gpuInfo.location.dev, gpuInfo.location.func, 0, (uint64_t*)&gpuInfo.pBaseMmio)!=0){
		printf("failed to get virtual I/O GPU BAR0\r\n");
		return -1;
	}
	if (!gpuInfo.pBaseMmio){
		printf("failed to get virtual I/O GPU BAR0 MMIO base\r\n");
		return -1;
	}
	*pInfo = gpuInfo;
	return 0;
}
