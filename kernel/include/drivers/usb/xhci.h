#ifndef _XHCI
#define _XHCI
#include <stdint.h>
#include "kernel_include.h"
#include "drivers/pcie.h"
#define XHCI_PORT_LIST_OFFSET (0x400)
#define XHCI_MAX_CMD_TRB_ENTRIES (256)
#define XHCI_MAX_EVENT_TRB_ENTRIES (256)
#define XHCI_MAX_DEVICE_COUNT (256)
#define XHCI_MAX_EVENT_SEGMENT_TABLE_ENTRIES (256)
#define XHCI_DEFAULT_PAGE_SIZE (0x0)
#define XHCI_DEFAULT_INTERRUPTER_ID (0x0)
#define XHCI_TRB_TYPE_INVALID (0x0)
#define XHCI_TRB_TYPE_NORMAL (0x01)
#define XHCI_TRB_TYPE_SETUP (0x02)
#define XHCI_TRB_TYPE_DATA (0x03)
#define XHCI_TRB_TYPE_STATUS (0x04)
#define XHCI_TRB_TYPE_ISOCH (0x05)
#define XHCI_TRB_TYPE_LINK (0x06)
#define XHCI_TRB_TYPE_EVENT (0x07)
#define XHCI_TRB_TYPE_NOP_TRANSFER (0x08)
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
#define XHCI_TRB_TYPE_GET_LATENCY_TOLERANCE (0x14)
#define XHCI_TRB_TYPE_GET_PORT_BANDWIDTH (0x15)
#define XHCI_TRB_TYPE_FORCE_HEADER (0x16)
#define XHCI_TRB_TYPE_NOP_CMD (0x17)
#define XHCI_TRB_TYPE_GET_EXTENDED_CAP (0x18)
#define XHCI_TRB_TYPE_SET_EXTENDED_CAP (0x19)
#define XHCI_EVENT_TRB_TYPE_TRANSFER_EVENT (0x20)
#define XHCI_EVENT_TRB_TYPE_CMD_COMPLETION (0x21)
#define XHCI_EVENT_TRB_TYPE_PORT_STATUS_CHANGE (0x22)
#define XHCI_EVENT_TRB_TYPE_BANDWIDTH_REQUEST (0x23)
#define XHCI_EVENT_TRB_TYPE_DOORBELL_EVENT (0x24)
#define XHCI_EVENT_TRB_TYPE_HC_ERROR (0x25)
#define XHCI_EVENT_TRB_TYPE_DEV_NOTIF (0x26)
#define XHCI_EVENT_TRB_TYPE_MFINDEX_WRAP (0x27)
#define XHCI_COMPLETION_CODE_INVALID (0x0)
#define XHCI_COMPLETION_CODE_SUCCESS (0x1)
#define XHCI_COMPLETION_CODE_DMA_ERROR (0x2)
#define XHCI_COMPLETION_CODE_OVERFLOW (0x3)
#define XHCI_COMPLETION_CODE_BUS_ERROR (0x4)
#define XHCI_COMPLETION_CODE_TRB_ERROR (0x5)
#define XHCI_COMPLETION_CODE_STALL_ERROR (0x6)
#define XHCI_COMPLETION_CODE_RESOURCE_ERROR (0x7)
#define XHCI_COMPLETION_CODE_BANDWIDTH_ERROR (0x8)
#define XHCI_COMPLETION_CODE_NO_SLOTS_AVAILABLE (0x9)
#define XHCI_COMPLETION_CODE_SLOT_NOT_ENABLED (0xB)
#define XHCI_COMPLETION_CODE_ENDPOINT_NOT_ENABLED (0xC)
#define XHCI_COMPLETION_CODE_SHORT_PACKET (0xD)
#define XHCI_COMPLETION_CODE_PARAM_ERROR (0x11)
#define XHCI_COMPLETION_CODE_CONTEXT_STATE_ERROR (0x13)
#define XHCI_COMPLETION_CODE_EVENT_RING_FULL (0x1A)
struct xhci_structure_param0{
	uint32_t max_slots:8;
	uint32_t max_interrupters:11;
	uint32_t reserved0:5;
	uint32_t max_ports:8;
}__attribute__((packed));
struct xhci_structure_param1{
	uint32_t ist:4;
	uint32_t event_ring_segment_table_max:4;
	uint32_t reserved0:13;	
	uint32_t scratchpad_pages_high:5;
	uint32_t reserved1:1;
	uint32_t scratchpad_pages_low:5;
}__attribute__((packed));
struct xhci_structure_param2{
	uint32_t u1_latency:8;
	uint32_t reserved0:8;
	uint32_t u2_latency:16;
}__attribute__((packed));
struct xhci_cap_param0{
	uint32_t long_addressing:1;
	uint32_t bandwidth_negotiation:1;
	uint32_t context_size:1;
	uint32_t port_power_ctrl:1;
	uint32_t port_indicators:1;
	uint32_t light_hc_reset_cap:1;
	uint32_t latency_tolerance_messaging:1;
	uint32_t no_second_sid:1;
	uint32_t parse_all_event_data:1;
	uint32_t stopped_short_packet:1;
	uint32_t stopped_edtla:1;
	uint32_t contiguous_frame_id_cap:1;
	uint32_t max_psa:4;
	uint32_t extended_cap_ptr:16;
}__attribute__((packed));
struct xhci_extended_cap_hdr{
	uint8_t cap_id;
	uint8_t next_offset;
	uint16_t reserved0;
}__attribute__((packed));
struct xhci_usb_legacy_support{
	struct xhci_extended_cap_hdr* pCapHeader;
	uint32_t firmware_owned:1;
	uint32_t os_owned:1;
	uint32_t reserved0:30;
}__attribute__((packed));
struct xhci_protocol_cap{
	uint8_t id;
	uint8_t next;
	uint8_t minor_version;
	uint8_t major_version;
	uint32_t type;
	uint8_t start_port;
	uint8_t port_count;
	uint16_t reserved0;
	uint32_t slot_type;
}__attribute__((packed));
struct xhci_usb_legacy_ctrl_status{
	uint32_t smi_enable:1;
	uint32_t smi_pending:1;
	uint32_t reserved0:30;
}__attribute__((packed));
struct xhci_cap_mmio{
	uint8_t cap_len;
	uint8_t reserved0;
	uint16_t hci_version;
	struct xhci_structure_param0 structure_param0;
	struct xhci_structure_param1 structure_param1;
	struct xhci_structure_param2 structure_param2;
	struct xhci_cap_param0 cap_param0;
	uint32_t doorbell_offset;
	uint32_t runtime_register_offset;
}__attribute__((packed));
union xhci_cmd_ring_ctrl{
	uint64_t raw;
	struct{
		uint64_t ring_cycle_state:1;
		uint64_t cmd_stop:1;
		uint64_t cmd_abort:1;
		uint64_t cmd_ring_running:1;
		uint64_t reserved0:2;
		uint64_t base:58;
	};
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
	uint32_t reserved0:1;
	uint32_t host_system_error:1;
	uint32_t event_int:1;
	uint32_t port_change_detect:1;
	uint32_t reserved1:3;
	uint32_t save_state_status:1;
	uint32_t restore_state_status:1;
	uint32_t save_restore_error:1;
	uint32_t controller_not_ready:1;
	uint32_t host_controller_error:1;
	uint32_t reserved2:19;
}__attribute__((packed));
struct xhci_config{
	uint32_t max_slots_enabled:8;
	uint32_t reserved0:24;
}__attribute__((packed));
struct xhci_operational_mmio{
	struct xhci_usb_cmd usb_cmd;
	struct xhci_usb_status usb_status;
	uint32_t page_size;
	uint32_t reserved0;
	uint32_t reserved1;
	struct xhci_dev_notif_ctrl device_notif_ctrl;
	union xhci_cmd_ring_ctrl cmd_ring_ctrl;
	uint32_t reserved2[4];
	uint64_t device_context_base_list_ptr;
	struct xhci_config config;
	uint32_t reserved3;
}__attribute__((packed));
struct xhci_dequeue_ring_ptr{
	uint64_t dequeue_table_segment_index:3;
	uint64_t event_handler_busy:1;
	uint64_t event_ring_dequeue_ptr:60;
}__attribute__((packed));
struct xhci_segment_table_entry{
	uint64_t base;
	uint32_t size;
	uint32_t reserved0;
}__attribute__((packed));
struct xhci_interrupter_iman{
	uint32_t interrupt_pending:1;
	uint32_t interrupt_enable:1;
	uint32_t reserved0:30;
}__attribute__((packed));
struct xhci_interrupter{
	struct xhci_interrupter_iman interrupt_management;
	uint32_t interrupt_moderation;
	uint32_t table_size;
	uint32_t reserved0;
	uint64_t table_base;
	struct xhci_dequeue_ring_ptr dequeue_ring_ptr;
}__attribute__((packed));
struct xhci_runtime_mmio{
	uint32_t microframe_index;
	uint32_t reserved0[7];
	volatile struct xhci_interrupter interrupter_list[];
}__attribute__((packed));
struct xhci_port_status{
	uint32_t connection_status:1;
	uint32_t enabled:1;
	uint32_t reserved0:1;
	uint32_t over_current_active:1;
	uint32_t port_reset:1;
	uint32_t port_link_state:4;
	uint32_t port_power:1;
	uint32_t port_speed:4;
	uint32_t port_indicator_ctrl:2;
	uint32_t link_write_strobe:1;
	uint32_t reserved1:1;
	uint32_t connect_status_change:1;
	uint32_t port_enabled_change:1;
	uint32_t warm_port_reset_change:1;
	uint32_t over_current_change:1;
	uint32_t port_reset_change:1;
	uint32_t port_link_state_change:1;
	uint32_t config_error_change:1;
	uint32_t reserved2:2;
}__attribute__((packed));
struct xhci_port{
	struct xhci_port_status port_status;
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
	uint32_t tc_bit:1;
	uint32_t isp:1;
	uint32_t no_snoop:1;
	uint32_t chain_bit:1;
	uint32_t ioc:1;
	uint32_t immediate_data:1;
	uint32_t reserved0:2;
	uint32_t block_event_int:1;
	uint32_t type:6;
	uint32_t reserved1:16;
}__attribute__((packed));
struct xhci_trb{
	union{
		struct{
			uint64_t ptr;
			struct xhci_trb_status status;
			struct xhci_trb_control control;
		}generic;
		struct{
			uint64_t trb_ptr;
			uint32_t reserved0:24;
			uint32_t completion_code:8;
			struct xhci_trb_control control;
		}event;
		struct{
			uint64_t trb_ptr;
			uint32_t reserved0:24;
			uint32_t completion_code:8;
			uint32_t cycle_bit:1;
			uint32_t reserved1:9;
			uint32_t type:6;
			uint32_t vf_id:8;
			uint32_t slot_id:8;
		}enable_slot_event;
		struct{
			uint64_t input_context_ptr;
			uint32_t reserved0;
			uint32_t cycle_bit:1;
			uint32_t reserved1:8;
			uint32_t block_set_address:1;
			uint32_t type:6;
			uint32_t reserved2:8;
			uint32_t slot_id:8;
		}address_device_cmd;
		struct{
			uint32_t reserved0;
			uint32_t reserved1;
			uint32_t reserved2;
			uint32_t cycle_bit:1;
			uint32_t reserved3:9;
			uint32_t type:6;
			uint32_t reserved4:8;
			uint32_t slot_id:8;
		}disable_slot_cmd;
		struct{
			uint32_t param0;
			uint32_t param1;
			struct xhci_trb_status status;
			struct xhci_trb_control control;
		}command;
	};
}__attribute__((packed));
struct xhci_slot_context{
	uint32_t route_string:20;
	uint32_t speed:4;
	uint32_t reserved0:1;
	uint32_t mtt:1;
	uint32_t hub:1;
	uint32_t context_entries:5;

	uint32_t max_exit_latency:16;
	uint32_t root_hub_port_num:8;
	uint32_t port_count:8;

	uint32_t tt_hub_slot_id:8;
	uint32_t tt_port_num:8;
	uint32_t ttt:2;
	uint32_t reserved1:4;
	uint32_t interrupter_target:10;

	uint32_t usb_device_address:8;
	uint32_t reserved2:19;
	uint32_t slot_state:5;
	
	uint32_t reserved3[4];
}__attribute__((packed));
struct xhci_endpoint_context{
	uint32_t state:3;
	uint32_t reserved0:5;
	uint32_t mult:2;
	uint32_t max_primary_streams:5;
	uint32_t linear_stream_array:1;
	uint32_t interval:8;
	uint32_t reserved1:8;
	
	uint32_t reserved2:1;
	uint32_t error_count:2;
	uint32_t type:3;
	uint32_t reserved3:1;
	uint32_t max_burst_size:8;
	uint32_t max_packet_size:16;
	
	uint64_t dequeue_cycle_state:1;
	uint64_t dequeue_ptr:63;

	uint32_t average_trb_len:16;
	uint32_t max_esit_payload_low:16;	

	uint32_t reserved4[3];
}__attribute__((packed));
struct xhci_input_context{
	uint32_t drop_flags;
	uint32_t add_flags;
	uint32_t reserved0[6];
}__attribute__((packed));
struct xhci_device_context{
	struct xhci_slot_context slotContext;
	struct xhci_endpoint_context endpointList[31];
}__attribute__((packed));
struct xhci_cmd_desc{
	volatile struct xhci_trb* pCmdTrb;
	volatile struct xhci_trb* pEventTrb;
	uint64_t pCmdTrb_phys;
	uint64_t pEventTrb_phys;
	uint64_t trbIndex;
};
struct xhci_cmd_ring_info{
	struct xhci_cmd_desc* pCmdDescList;
	uint64_t cmdDescListSize;
	volatile struct xhci_trb* pRingBuffer;
	uint64_t pRingBuffer_phys;
	uint64_t ringBufferSize;
	unsigned char cycle_state;
	uint64_t currentEntry;	
	uint64_t maxEntries;
};
struct xhci_event_trb_ring_info{
	uint64_t dequeueTrb;
	uint64_t dequeueTrbBase_phys;
	volatile struct xhci_trb* dequeueTrbBase;
	volatile struct xhci_segment_table_entry* pSegmentTable;
	uint64_t pSegmentTable_phys;
	uint64_t maxSegmentTableEntryCount;
	volatile struct xhci_trb* pRingBuffer;
	uint64_t pRingBuffer_phys;
	uint64_t maxEntries;
};
struct xhci_interrupter_info{
	uint8_t vector;
	struct xhci_event_trb_ring_info eventRingInfo;
};
struct xhci_device_context_list_info{
	uint64_t* pContextList;
	uint64_t pContextList_phys;
	uint64_t maxDeviceCount;
};
struct xhci_device_context_desc{
	volatile struct xhci_device_context* pDeviceContext;
	uint64_t pDeviceContext_phys;
	volatile struct xhci_input_context* pInputContext;
	uint64_t pInputContext_phys;
	uint64_t slotId;
	uint8_t port;
};
struct xhci_info{
	uint64_t pBaseMmio;
	uint64_t pBaseMmio_physical;
	volatile struct xhci_cap_mmio* pCapabilities;
	volatile struct xhci_operational_mmio* pOperational;
	volatile struct xhci_runtime_mmio* pRuntime;
	volatile struct xhci_extended_cap_hdr* pExtendedCapList;
	volatile uint32_t* pDoorBells;
	struct xhci_cmd_ring_info* pCmdRingInfo;
	struct xhci_interrupter_info interrupterInfo;
	struct xhci_device_context_list_info deviceContextListInfo;
	struct pcie_location location;
};
int xhci_init(void);
int xhci_get_info(struct xhci_info* pInfo);
int xhci_get_location(struct pcie_location* pLocation);
int xhci_read_qword(volatile uint64_t* pQword, uint64_t* pValue);
int xhci_write_qword(volatile uint64_t* pQword, uint64_t value);
int xhci_get_port_list(volatile struct xhci_port** ppPortList);
int xhci_get_port(uint8_t port, volatile struct xhci_port** ppPort);
int xhci_get_port_speed(uint8_t port, uint8_t* pSpeed);
int xhci_device_exists(uint8_t port);
int xhci_reset_port(uint8_t port);
int xhci_get_port_count(uint8_t* pPortCount);
int xhci_init_cmd_list(struct xhci_cmd_ring_info** ppRingInfo);
int xhci_deinit_cmd_list(struct xhci_cmd_ring_info* pRingInfo);
int xhci_alloc_cmd(struct xhci_cmd_ring_info* pRingInfo, struct xhci_trb trb, struct xhci_cmd_desc** ppCmdDesc);
int xhci_get_cmd(struct xhci_cmd_ring_info* pRingInfo, uint64_t trbIndex, volatile struct xhci_trb** ppTrbEntry);
int xhci_write_cmd(struct xhci_cmd_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb trbEntry);
int xhci_read_cmd(struct xhci_cmd_ring_info* pRingInfo, uint64_t trbIndex, struct xhci_trb* pTrbEntry);
int xhci_ring(uint64_t doorbell_vector);
int xhci_reset(void);
int xhci_start(void);
int xhci_stop(void);
int xhci_is_running(void);
int xhci_is_ready(void);
int xhci_get_cmd_ring_base(uint64_t* pBase);
int xhci_set_cmd_ring_base(uint64_t base);
int xhci_get_cmd_ring_running(void);
int xhci_get_interrupter_base(uint64_t interrupter_id, volatile struct xhci_interrupter** ppBase);
int xhci_init_trb_event_list(struct xhci_event_trb_ring_info* pRingInfo);
int xhci_deinit_trb_event_list(struct xhci_event_trb_ring_info* pRingInfo);
int xhci_init_interrupter(void);
int xhci_send_ack(uint64_t interrupter_id);
int xhci_get_dequeue_trb_phys(uint64_t* ppTrbEntry);
int xhci_get_dequeue_trb(volatile struct xhci_trb** ppTrbEntry);
int xhci_update_dequeue_trb(void);
int xhci_get_event_trb(volatile struct xhci_trb** ppTrbEntry);
int xhci_get_event_trb_phys(uint64_t* ppTrbEntry);
int xhci_get_trb_type_name(uint64_t type, const unsigned char** ppName);
int xhci_get_error_name(uint64_t error_code, const unsigned char** ppName);
int xhci_interrupter(void);
int xhci_interrupter_isr(void);
int xhci_dump_interrupter(uint64_t interrupter_id);
int xhci_init_device_context_list(void);
int xhci_get_driver_cycle_state(unsigned char* pCycleState);
int xhci_get_hc_cycle_state(unsigned char* pCycleState);
int xhci_init_scratchpad(void);
int xhci_get_extended_cap(uint8_t cap_id, volatile struct xhci_extended_cap_hdr** ppCapHeader);
int xhci_enable_slot(uint64_t* pSlotId);
int xhci_disable_slot(uint64_t slotId);
int xhci_alloc_device_context(uint8_t port, struct xhci_device_context_desc** ppContextDesc);
int xhci_free_device_context(struct xhci_device_context_desc* pContextDesc);
#endif
