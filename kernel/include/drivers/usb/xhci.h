#ifndef _XHCI
#define _XHCI
#include <stdint.h>
#include "kernel_include.h"
#include "drivers/pcie.h"
#define XHCI_PORT_LIST_OFFSET 0x400
#define XHCI_MAX_CMD_TRB_ENTRIES 256
struct xhci_cap_mmio{
	uint8_t cap_len;
	uint8_t reserved0;
	uint16_t hci_version;
	uint32_t structure_params[3];
	uint32_t cap_params0;
	uint32_t doorbell_offset;
	uint32_t runtime_register_offset;
	uint32_t cap_params1;
}__attribute__((packed));
struct xhci_cmd_ring_ctrl{
	uint64_t ring_base:60;
	uint64_t reserved0:1;
	uint64_t cycle_state:1;
	uint64_t running:1;
	uint64_t reserved1:2;
}__attribute__((packed));
struct xhci_dev_notif_ctrl{
	uint32_t interrupt_enable:1;
	uint32_t reserved0:31;
};
struct xhci_operational_mmio{
	uint32_t usb_cmd;
	uint32_t usb_status;
	uint32_t page_size;
	uint32_t reserved0;
	struct xhci_dev_notif_ctrl device_notif_ctrl;
	struct xhci_cmd_ring_ctrl cmd_ring_ctrl;
	uint32_t reserved1[4];
	uint64_t device_context_base_list_ptr;
	uint32_t config;
	uint32_t reserved2;
}__attribute__((packed));
struct xhci_runtime_mmio{
	uint32_t interrupt_management;
	uint32_t interrupt_moderation;
	uint32_t event_ring_entry_count;
	uint32_t reserved0;
	uint32_t event_ring_base;
	uint32_t event_ring_dequeue_ptr;
	uint32_t reserved1[10];
}__attribute__((packed));
struct xhci_port{
	uint32_t port_status;
	uint32_t port_power_ctrl;
	uint32_t port_link_info;
	uint32_t reserved0;
}__attribute__((packed));
struct xhci_trb{
	union{
		struct{
			uint64_t ptr;
			uint32_t transfer_len:24;
			uint32_t status:8;
			uint32_t control;
		}generic;
		struct{
			uint64_t trb_ptr;
			uint32_t status;
			uint32_t control;
		}event;
		struct{
			uint32_t param0;
			uint32_t param1;
			uint32_t status;
			uint32_t control;
		}command;
	};
}__attribute__((packed));
struct xhci_slot_context{
	uint32_t route_string:20;
	uint32_t speed:4;
	uint32_t reserved0:8;

	uint32_t reserved1;

	uint32_t max_exit_latency:16;
	uint32_t root_hub_port:8;
	uint32_t endpoint_count:8;

	uint32_t unused[5];
}__attribute__((packed));
struct xhci_endpoint_context{
	uint32_t state:3;
	uint32_t reserved0:5;
	uint32_t max_bursts:2;
	uint32_t max_packet_size:16;
	uint32_t poll_interval:8;

	uint32_t reserved1;

	uint64_t tr_dequeue_ptr;

	uint32_t average_trb_len:16;
	uint32_t max_esit_payload:16;

	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
}__attribute__((packed));
struct xhci_device_context{
	struct xhci_slot_context slot;
	struct xhci_endpoint_context endpoints[31];
}__attribute__((packed));
struct xhci_trb_ring_info{
	volatile struct xhci_trb* pRingBuffer;
	uint64_t pRingBuffer_phys;
	uint64_t maxEntries;
	uint64_t entryCount;
};
struct xhci_info{
	uint64_t pBaseMmio;
	uint64_t pBaseMmio_physical;
	volatile struct xhci_cap_mmio* pCapabilities;
	volatile struct xhci_operational_mmio* pOperational;
	volatile struct xhci_runtime_mmio* pRuntime;
	volatile uint32_t* pDoorBells;
	struct xhci_trb_ring_info cmdRingInfo;
	struct pcie_location location;
};
int xhci_init(void);
int xhci_get_info(struct xhci_info* pInfo);
int xhci_get_location(struct pcie_location* pLocation);
int xhci_get_port_list(volatile struct xhci_port** ppPortList);
int xhci_get_port(uint8_t port, volatile struct xhci_port** ppPort);
int xhci_device_exists(uint8_t port);
int xhci_get_port_count(uint8_t* pPortCount);
int xhci_init_cmd_list(void);
int xhci_push_cmd(struct xhci_trb trb, uint64_t* pTrbIndex);
int xhci_get_cmd(uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry);
int xhci_ring(uint64_t doorbell_vector);
#endif
