#ifndef _XHCI
#define _XHCI
#include <stdint.h>
#include "kernel_include.h"
#include "drivers/pcie.h"
#define XHCI_PORT_LIST_OFFSET (0x400)
#define XHCI_MAX_CMD_TRB_ENTRIES (256)
#define XHCI_TRB_TYPE_NORMAL (0x01)
#define XHCI_TRB_TYPE_SETUP (0x02)
#define XHCI_TRB_TYPE_DATA (0x03)
#define XHCI_TRB_TYPE_STATUS (0x04)
#define XHCI_TRB_TYPE_ISOCH (0x05)
#define XHCI_TRB_TYPE_LINK (0x06)
#define XHCI_TRB_TYPE_EVENT (0x07)
#define XHCI_TRB_TYPE_NOP (0x08)
#define XHCI_TRB_TYPE_ENABLE_SLOT (0x09)
#define XHCI_TRB_TYPE_DISABLE_SLOT (0x0A)
#define XHCI_TRB_TYPE_ADDRESS_DEVICE (0x0B)
#define XHCI_TRB_TYPE_CONFIG_ENDPOINT (0x0C)
#define XHCI_TRB_TYPE_UPDATE_CONTEXT (0x0D)
#define XHCI_TRB_TYPE_RESET_ENDPOINT (0x0E)
#define XHCI_TRB_TYPE_STOP_ENDPOINT (0x0F)
#define XHCI_TRB_TYPE_SET_EVENT_DEQUEUE_PTR (0x10)
#define XHCI_TRB_TYPE_RESET_DEVICE (0x11)
#define XHCI_TRB_TYPE_FORCE_EVENT (0x12)
#define XHCI_TRB_TYPE_GET_BANDWIDTH (0x13)
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
}__attribute__((packed));
struct xhci_usb_cmd{
	uint32_t run:1;
	uint32_t hc_reset:1;
	uint32_t interrupter_enable:1;
	uint32_t periodic_enable:1;
	uint32_t async_enable:1;
	uint32_t doorbell_cmd:1;
	uint32_t reserved0:2;
	uint32_t host_ctrl_os:1;
	uint32_t reserved1:23;
}__attribute__((packed));
struct xhci_usb_status{
	uint32_t halted:1;
	uint32_t host_system_error:1;
	uint32_t event_int:1;
	uint32_t port_change_detect:1;
	uint32_t reserved0:5;
	uint32_t save_state_status:1;
	uint32_t restore_state_status:1;
	uint32_t controller_not_ready:1;
	uint32_t host_controller_error:1;
	uint32_t reserved1:19;
}__attribute__((packed));
struct xhci_operational_mmio{
	struct xhci_usb_cmd usb_cmd;
	struct xhci_usb_status usb_status;
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
struct xhci_trb_status{
	uint32_t transfer_len:17;
	uint32_t td_size:5;
	uint32_t event_ring_target:10;
}__attribute__((packed));
struct xhci_trb_control{
	uint32_t cycle_bit:1;
	uint32_t link_trb:1;
	uint32_t interrupter_enable:1;
	uint32_t chain_bit:1;
	uint32_t ioc:1;
	uint32_t reserved0:2;
	uint32_t type:6;
	uint32_t reserved1:19;
}__attribute__((packed));
struct xhci_trb{
	union{
		struct{
			uint64_t ptr;
			uint32_t transfer_len:24;
			uint32_t status:8;
			struct xhci_trb_control control;
		}generic;
		struct{
			uint64_t trb_ptr;
			uint32_t status;
			struct xhci_trb_control control;
		}event;
		struct{
			uint32_t param0;
			uint32_t param1;
			uint32_t status;
			struct xhci_trb_control control;
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
	uint64_t* pFreeList;
	uint64_t* pEntryList;
	uint64_t pRingBuffer_phys;
	uint64_t maxEntries;
	uint64_t entryCount;
	uint64_t freeEntryCount;	
	uint64_t entryListSize;
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
int xhci_init_trb_list(struct xhci_trb_ring_info* pRingInfo);
int xhci_deinit_trb_list(struct xhci_trb_ring_info* pRingInfo);
int xhci_alloc_trb(struct xhci_trb_ring_info* pRingInfo, struct xhci_trb trb, uint64_t* pTrbIndex);
int xhci_free_trb(struct xhci_trb_ring_info* pRingInfo, uint64_t trbIndex);
int xhci_get_trb(uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry);
int xhci_ring(uint64_t doorbell_vector);
#endif
