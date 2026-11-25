#ifndef _INTEL_HDA
#define _INTEL_HDA
#include "subsystem/pcie.h"
struct intel_hda_driver_info{
	uint64_t pcieDriverId;
	uint64_t driverId;
};
struct intel_hda_global_cap{
	uint16_t max_output_stream_count:4;
	uint16_t max_input_stream_count:4;
	uint16_t max_bidirection_stream_count:5;
	uint16_t serial_data_out_signal_count:2;
	uint16_t qword_address_support:1;
}__attribute__((packed));
struct intel_hda_global_control{
	uint32_t reset:1;
	uint32_t flush_control:1;
	uint32_t reserved0:6;
	uint32_t accept_unexpected_response:1;
	uint32_t reserved1:23;
}__attribute__((packed));
struct intel_hda_global_status{
	uint16_t reserved0:1;
	uint16_t flush_status:1;
	uint16_t reserved1:14;
}__attribute__((packed));
struct intel_hda_interrupt_control{
	uint32_t stream_interrupt_enable:30;
	uint32_t controller_interrupt_enable:1;
	uint32_t global_interrupt_enable:1;
}__attribute__((packed));
struct intel_hda_interrupt_status{
	uint32_t stream_interrupt_status:30;
	uint32_t controller_interrupt_status:1;
	uint32_t global_interrupt_status:1;
}__attribute__((packed));
struct intel_hda_corb_size{
	uint8_t corb_size:2;
	uint8_t reserved0:2;
	uint8_t corb_size_cap:4;
}__attribute__((packed));
struct intel_hda_corb_control{
	uint8_t corb_memory_error_interrupt_enable:1;
	uint8_t corb_dma_enable:1;
	uint8_t reserved0:6;
}__attribute__((packed));
struct intel_hda_corb_status{
	uint8_t corb_memory_error:1;
	uint8_t reserved0:7;
}__attribute__((packed));
struct intel_hda_rirb_size{
	uint8_t rirb_size:2;
	uint8_t reserved0:2;
	uint8_t rirb_size_cap:4;
}__attribute__((packed));
struct intel_hda_rirb_control{
	uint8_t rirb_interrupt_enable:1;
	uint8_t rirb_dma_enable:1;
	uint8_t rirb_overrun_interrupt_control:1;
	uint8_t reserved0:5;
}__attribute__((packed));
struct intel_hda_rirb_status{
	uint8_t response_interrupt:1;
	uint8_t reserved0:1;
	uint8_t response_overrun_interrupt_status:1;
	uint8_t reserved1:5;
}__attribute__((packed));
struct intel_hda_command_desc{
	union{
		struct{
			uint32_t payload:8;
			uint32_t verb_id:12;
			uint32_t node_id:8;
			uint32_t codec_address:4;
		}payload_short;
		struct{
			uint32_t payload:16;
			uint32_t verb_id:4;
			uint32_t node_id:8;
			uint32_t codec_address:4;
		}payload_long;
	};
};
struct intel_hda_response_desc{
	uint32_t response;
	uint32_t extra;
}__attribute__((packed));
struct intel_hda_base_registers{
	struct intel_hda_global_cap global_cap;
	uint8_t minor_revision;
	uint8_t major_revision;
	uint16_t max_output_payload_size;
	uint16_t max_input_payload_size;
	
	struct intel_hda_global_control global_control;
	uint16_t wake_enable;
	uint16_t wake_status;
	struct intel_hda_global_status global_status;

	uint8_t reserved0[6];
	
	uint16_t max_output_stream_payload_size;
	uint16_t max_input_stream_payload_size;

	uint8_t reserved1[4];
	
	struct intel_hda_interrupt_control interrupt_control;
	struct intel_hda_interrupt_status interrupt_status;

	uint8_t reserved2[8];
	
	uint32_t wall_clock_counter;
	uint8_t reserved3[4];
	uint32_t stream_sync;
	
	uint8_t reserved4[4];

	uint64_t corb_base;
	uint16_t corb_write_index;
	uint16_t corb_read_index;
	struct intel_hda_corb_control corb_control;
	struct intel_hda_corb_status corb_status;
	struct intel_hda_corb_size corb_size;

	uint8_t reserved5[1];

	uint64_t rirb_base;
	uint16_t rirb_write_index;
	uint16_t response_interrupt_count;
	struct intel_hda_rirb_control rirb_control;
	struct intel_hda_rirb_status rirb_status;
	struct intel_hda_rirb_size rirb_size;

	uint8_t reserved6[1];

	uint32_t immediate_cmd_output_interface;
	uint32_t immediate_cmd_input_interface;	
	uint16_t immediate_cmd_status;

	uint8_t reserved7[6];

	uint64_t dma_position_base;

	uint8_t reserved8[8];
	
	uint8_t isd0_control[3];
	uint8_t isd0_status;
	uint32_t isd0_link_index;
	uint32_t isd0_cyclic_buffer_length;
	uint16_t isd0_last_valid_index;
	uint8_t reserved9[3];
	uint16_t isd0_fifo_size;
	uint16_t isd0_format;
	uint8_t reserved10[4];
	uint64_t isd0_buffer_descriptor_list_base;
}__attribute__((packed));
struct intel_hda_interrupt_desc{
	struct intel_hda_controller_desc* pControllerDesc;
};
struct intel_hda_controller_desc{
	struct pcie_location pcieLocation;
	struct intel_hda_base_registers* pBaseRegisters;
	uint64_t pBaseRegisters_phys;
	struct intel_hda_command_desc* pCommandDescList;
	uint64_t pCommandDescList_phys;
	uint64_t commandDescListSize;
	uint16_t commandDescCount;
	uint16_t currentCommandIndex;
	uint16_t nextCommandIndex;
	uint64_t commandPushCount;
	uint64_t lastCommandPushCount;
	struct intel_hda_response_desc* pResponseDescList;
	uint64_t pResponseDescList_phys;
	uint64_t responseDescListSize;
	uint16_t responseDescCount;
	uint16_t currentResponseIndex;
	uint16_t lastResponseWriteIndex;
	struct intel_hda_interrupt_desc* pInterruptDescList;
	uint64_t interruptDescListSize;
};
int intel_hda_driver_init(void);
int intel_hda_driver_deinit(void);
int intel_hda_driver_reset(struct intel_hda_controller_desc* pControllerDesc);
int intel_hda_driver_corb_run(struct intel_hda_controller_desc* pControllerDesc);
int intel_hda_driver_command_push(struct intel_hda_controller_desc* pControllerDesc, struct intel_hda_command_desc commandDesc);
int intel_hda_driver_interrupt(uint8_t vector);
int intel_hda_driver_pcie_register(struct pcie_location location);
int intel_hda_driver_pcie_unregister(struct pcie_location location);
int intel_hda_driver_isr(void);
#endif
