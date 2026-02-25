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
	if (virtualMapPage((uint64_t)gpuInfo.pBaseRegisters, (uint64_t)gpuInfo.pBaseRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map base registers\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)gpuInfo.pNotifyRegisters, (uint64_t)gpuInfo.pNotifyRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map notify registers\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)gpuInfo.pInterruptRegisters, (uint64_t)gpuInfo.pInterruptRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map interrupt registers\r\n");
		return -1;
	}	
	if (virtualMapPage((uint64_t)gpuInfo.pDeviceRegisters, (uint64_t)gpuInfo.pDeviceRegisters, PTE_RW|PTE_NX|PTE_PCD|PTE_PWT, 1, 0, PAGE_TYPE_MMIO)!=0){
		printf("failed to map device registers\r\n");
		return -1;
	}
	printf("VIRTIO GPU driver bus: %d, dev: %d, func: %d\r\n", gpuInfo.location.bus, gpuInfo.location.dev, gpuInfo.location.func);
	return 0;
}
int virtio_gpu_get_info(struct virtio_gpu_info* pInfo){
	if (!pInfo)
		return -1;
	if (gpuInfo.pBaseRegisters){
		*pInfo = gpuInfo;
		return 0;
	}
	if (pcie_get_function_by_id(VIRTIO_VENDOR_ID, VIRTIO_GPU_DEVICE_ID, &gpuInfo.location)!=0){
		return -1;
	}
	struct pcie_cap_link* pCurrentCapLink = (struct pcie_cap_link*)0x0;
	if (pcie_get_first_cap_ptr(gpuInfo.location, &pCurrentCapLink)!=0){
		printf("failed to get first PCIe capability link\r\n");
		return -1;
	}	
	while (1){
		if (pCurrentCapLink->cap_id!=0x09){
			if (pcie_get_next_cap_ptr(gpuInfo.location, pCurrentCapLink, &pCurrentCapLink)!=0)
				break;
		}
		struct virtio_gpu_pcie_config_cap* pConfigCap = (struct virtio_gpu_pcie_config_cap*)pCurrentCapLink;
		uint64_t blockBase = 0;
		if (pcie_get_bar(gpuInfo.location, pConfigCap->bar, &blockBase)!=0){
			printf("failed to get BAR%d for virtual I/O GPU register block\r\n");
			return -1;
		}	
		blockBase+=pConfigCap->bar_offset;	
		switch (pConfigCap->config_type){
			case VIRTIO_BLOCK_BASE_REGISTERS:{
				gpuInfo.pBaseRegisters = (volatile struct virtio_gpu_base_registers*)blockBase;			 
				break;
			}
			case VIRTIO_BLOCK_NOTIFY_REGISTERS:{
				gpuInfo.pNotifyRegisters = (volatile uint32_t*)blockBase;
				break;				   
			}
			case VIRTIO_BLOCK_INTERRUPT_REGISTERS:{
				gpuInfo.pInterruptRegisters = (volatile struct virtio_gpu_interrupt_registers*)blockBase;
				break;				      
			}
			case VIRTIO_BLOCK_DEVICE_REGISTERS:{
				gpuInfo.pDeviceRegisters = (volatile struct virtio_gpu_device_registers*)blockBase;
				break;				
			}
			case VIRTIO_BLOCK_PCIE_REGISTERS:{
				gpuInfo.pPcieRegisters = (volatile struct virtio_gpu_pcie_registers*)blockBase;
				break;				 
			}
		}	
		if (pcie_get_next_cap_ptr(gpuInfo.location, pCurrentCapLink, &pCurrentCapLink)!=0)
			break;
	}
	if (!gpuInfo.pBaseRegisters||!gpuInfo.pNotifyRegisters||!gpuInfo.pInterruptRegisters||!gpuInfo.pDeviceRegisters||!gpuInfo.pPcieRegisters){
		return -1;
	}
	*pInfo = gpuInfo;
	return 0;
}
int virtio_gpu_reset(void){

	return 0;
}
