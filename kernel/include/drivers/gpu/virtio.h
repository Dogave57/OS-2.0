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
#define VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_LEGACY (0x00)
#define VIRTIO_GPU_DEVICE_FEATURE_SELECTOR_MODERN (0x01)
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO (0x0100)
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D (0x0101)
#define VIRTIO_GPU_CMD_RESOURCE_FREE (0x0102)
#define VIRTIO_GPU_CMD_SET_SCANOUT (0x0103)
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH (0x0104)
#define VIRTIO_GPU_CMD_TRANSFER_H2D (0x0105)
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_PAGES (0x0106)
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_PAGES (0x0107)
#define VIRTIO_GPU_CMD_GET_CAPSET_INFO (0x0108)
#define VIRTIO_GPU_CMD_GET_CAPSET (0x0109)
#define VIRTIO_GPU_CMD_GET_MONITOR_IDENT_INFO (0x010A)
#define VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID (0x010B)
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB (0x010C)
#define VIRTIO_GPU_CMD_SET_SCANOUT_BLOB (0x010D)

#define VIRTIO_GPU_CMD3D_CONTEXT_CREATE (0x0200)
#define VIRTIO_GPU_CMD3D_CONTEXT_DESTROY (0x0201)
#define VIRTIO_GPU_CMD3D_CONTEXT_ATTACH_RESOURCE (0x0202)
#define VIRTIO_GPU_CMD3D_CONTEXT_DETACH_RESOURCE (0x0203)
#define VIRTIO_GPU_CMD3D_RESOURCE_CREATE_3D (0x0204)
#define VIRTIO_GPU_CMD3D_RESOURCE_TRANSFER_D2H (0x0205)
#define VIRTIO_GPU_CMD3D_RESOURCE_TRANSFER_H2D (0x0206)
#define VIRTIO_GPU_CMD3D_SUBMIT_3D (0x0207)
#define VIRTIO_GPU_CMD3D_RESOURCE_MAP_BLOB (0x0208)
#define VIRTIO_GPU_CMD3D_RESOURCE_UNMAP_BLOB (0x0209)

#define VIRTIO_GPU_CMD_UPDATE_CURSOR (0x0300)
#define VIRTIO_GPU_CMD_MOVE_CURSOR (0x0301)

#define VIRTIO_GPU_RESPONSE_OK_NODATA (0x1100)
#define VIRTIO_GPU_RESPONSE_OK_DISPLAY_INFO (0x1101)
#define VIRTIO_GPU_RESPONSE_OK_CAPSET_INFO (0x1102)
#define VIRTIO_GPU_RESPONSE_OK_CAPSET (0x1103)
#define VIRTIO_GPU_RESPONSE_OK_MONITOR_IDENT (0x1104)
#define VIRTIO_GPU_RESPONSE_OK_RESOURCE_PLANE_INFO (0x1105)
#define VIRTIO_GPU_RESPONSE_OK_MAP_INFO (0x1106)

#define VIRTIO_GPU_RESPONSE_ERROR_UNKNOWN (0x1200)
#define VIRTIO_GPU_RESPONSE_ERROR_OUT_OF_VRAM (0x1201)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_SCANOUT (0x1202)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_RESOURCE_ID (0x1203)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_CONTEXT_ID (0x1204)
#define VIRTIO_GPU_RESPONSE_ERROR_INVALID_PARAMETER (0x1205)

#define VIRTIO_GPU_MAX_MEMORY_DESC_COUNT (256)
#define VIRTIO_GPU_MAX_COMMAND_COUNT (128)
#define VIRTIO_GPU_MAX_RESPONSE_COUNT (128)
struct virtio_gpu_pcie_config_cap{
	struct pcie_cap_link link;
	uint8_t cap_size;
	uint8_t config_type;
	uint8_t bar;
	uint8_t padding0[3];
	uint32_t bar_offset;
	uint32_t register_block_size;
}__attribute__((packed));
struct virtio_gpu_pcie_config_cap_notify{
	struct virtio_gpu_pcie_config_cap header;
	uint32_t notify_offset_multiplier;
}__attribute__((packed));
struct virtio_gpu_features_legacy{
	uint32_t virgl_support:1;
	uint32_t edid_metadata_support:1;
	uint32_t resource_uuid_support:1;
	uint32_t host_memory_blob_support:1;
	uint32_t context_init_support:1;
	uint32_t reserved0:19;
	uint32_t notify_on_empty:1;
	uint32_t reserved1:2;
	uint32_t legacy_descriptor_layout:1;
	uint32_t indirect_descriptor_support:1;
	uint32_t interupt_suppression_support:1;
	uint32_t reserved2:2;
}__attribute__((packed));
struct virtio_gpu_features_modern{
	uint32_t version_1:1;
	uint32_t iommu_protected:1;
	uint32_t ring_packed:1;
	uint32_t ordered_descriptors:1;
	uint32_t same_memory_model:1;
	uint32_t single_root_io_virtualization:1;
	uint32_t notification_data:1;
	uint32_t notification_config_data:1;
	uint32_t ring_reset_support:1;
	uint32_t reserved0:23;
}__attribute__((packed));
struct virtio_gpu_device_status{
	uint8_t acknowledge:1;
	uint8_t driver:1;
	uint8_t driver_valid:1;
	uint8_t features_valid:1;
	uint8_t reserved0:2;
	uint8_t reset_required;
	uint8_t failed:1;
}__attribute__((packed));
struct virtio_gpu_base_registers{
	volatile uint32_t device_feature_select;
	volatile uint32_t device_feature;
	volatile uint32_t driver_feature_select;
	volatile uint32_t driver_feature;
	volatile uint16_t msix_config;
	volatile uint16_t queue_count;
	volatile struct virtio_gpu_device_status device_status;	
	volatile uint8_t config_generation;
	volatile uint16_t queue_select;
	volatile uint16_t queue_size;
	volatile uint16_t queue_msix_vector;
	volatile uint16_t queue_enable;
	volatile uint16_t queue_notify_offset;
	volatile uint64_t queue_memory_desc_list_base;
	volatile uint64_t queue_command_list_base;
	volatile uint64_t queue_response_list_base;
}__attribute__((packed));
struct virtio_gpu_interrupt_registers{
	volatile uint8_t isr_status;
}__attribute__((packed));
struct virtio_gpu_device_registers{
	volatile uint32_t events_read;
	volatile uint32_t events_clear;
	volatile uint32_t scanout_count;
	volatile uint32_t capset_count;
}__attribute__((packed));
struct virtio_gpu_pcie_registers{
		
}__attribute__((packed));
struct virtio_gpu_memory_desc{
	uint64_t physical_address;
	uint32_t length;
	uint16_t flags;
	uint16_t next;
}__attribute__((packed));
struct virtio_gpu_command_header{
	uint32_t type;
	uint32_t flags;
	uint64_t fence_id;
	uint32_t context_id;
	uint32_t ring_index;
}__attribute__((packed));
struct virtio_gpu_response_header{
	uint32_t type;
	uint32_t flags;
	uint64_t fence_id;
	uint32_t context_id;
	uint32_t padding0;
}__attribute__((packed));
struct virtio_gpu_queue_info{
	struct virtio_gpu_memory_desc* pMemoryDescList;
	struct virtio_gpu_command_header* pCommandList;
	struct virtio_gpu_response_header* pResponseList;
	uint64_t pMemoryDescList_phys;	
	uint64_t pCommandList_phys;
	uint64_t pResponseList_phys;
	uint64_t maxMemoryDescCount;
	uint64_t maxCommandCount;
	uint64_t maxResponseCount;
}__attribute__((packed));
struct virtio_gpu_info{
	struct pcie_location location;
	volatile struct virtio_gpu_base_registers* pBaseRegisters;
	volatile uint32_t* pNotifyRegisters;
	uint32_t notifyOffsetMultiplier;
	volatile struct virtio_gpu_interrupt_registers* pInterruptRegisters;
	volatile struct virtio_gpu_device_registers* pDeviceRegisters;
	volatile struct virtio_gpu_pcie_registers* pPcieRegisters;
	struct virtio_gpu_queue_info virtQueueInfo;
};
int virtio_gpu_init(void);
int virtio_gpu_get_info(struct virtio_gpu_info* pInfo);
int virtio_gpu_reset(void);
int virtio_gpu_acknowledge(void);
int virtio_gpu_queue_init(struct virtio_gpu_queue_info* pQueueInfo);
int virtio_gpu_queue_deinit(struct virtio_gpu_queue_info* pQueueInfo);
#endif
