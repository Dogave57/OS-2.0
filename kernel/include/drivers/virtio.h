#ifndef _VIRTIO
#define _VIRTIO
#define VIRTIO_VENDOR_ID (0x1AF4)
#define VIRTIO_MAGIC ((uint32_t)0x74726976) // VIRT
struct virtio_base_registers{
	uint32_t magic_value;
	uint32_t version;
	uint32_t device_id;
	uint32_t vendor_id;
	uint32_t device_features;
	uint32_t device_features_selector;
	uint32_t reserved0[2];
	uint32_t driver_features;
	uint32_t driver_features_selector;
	uint32_t queue_selector;
	uint32_t max_queue_size;
	uint32_t queue_size;
	uint32_t reserved1[2];
	uint32_t queue_ready;
	uint32_t reserved2[2];
	uint32_t queue_notify;
	uint32_t reserved3[3];
	uint32_t interrupt_status;
	uint32_t interrupt_ack;
	uint32_t reserved4[2];
	uint32_t reserved5[2];
	uint32_t status;
	uint32_t reserved6[3];
	uint64_t queue_desc_table_base;
	uint32_t reserved7[2];
	uint64_t queue_driver_ring_base;
	uint32_t reserved8[2];
	uint64_t queue_device_ring_base;
	uint32_t reserved9[21];
	uint32_t config_generation;
}__attribute__((packed));
#endif
