#include "stdlib/stdlib.h"
#include "subsystem/subsystem.h"
#include "mem/vmm.h"
#include "drivers/pcie.h"
#include "drivers/virtio.h"
#include "drivers/gpu/virtio.h"
struct virtio_gpu_info gpuInfo = {0};
int virtio_gpu_init(void){
	if (virtio_gpu_get_info(&gpuInfo)!=0){
		return -1;
	}	
	if (virtualMapPage(0x0, (uint64_t)gpuInfo.pBaseMmio, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_NORMAL)!=0){
		return -1;
	}
	*(uint64_t*)gpuInfo.pBaseMmio = 67;
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
	if (pcie_get_device_by_id(VIRTIO_VENDOR_ID, VIRTIO_GPU_DEVICE_ID, &gpuInfo.location)!=0){
		return -1;
	}
	if (pcie_get_bar(gpuInfo.location, 4, (uint64_t*)&gpuInfo.pBaseMmio)!=0){
		return -1;
	}
	if (!gpuInfo.pBaseMmio){
		return -1;
	}
	*pInfo = gpuInfo;
	return 0;
}
