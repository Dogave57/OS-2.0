#ifndef _USB_BOT_DRIVER
#define _USB_BOT_DRIVER
#define BOT_CMD_SIGNATURE (0x43425355)
#define BOT_STATUS_SIGNATURE (0x53425355)
#define BOT_STATUS_SUCCES (0x00)
#define BOT_STATUS_FAILURE (0x01)
#define BOT_STATUS_PHASE_ERROR (0x02)
#define BOT_DEVICE_TYPE_FLASH (0x0)
#define BOT_DEVICE_TYPE_CD (0x05)
#define BOT_DEVICE_TYPE_UNKNOWN (0x1F)
#define BOT_OPCODE_PING (0x00)
#define BOT_OPCODE_GET_INFO (0x12)
#define BOT_OPCODE_SERVICE_ACTION_IN (0x9E)
#define BOT_OPCODE_GET_STANDARD_CAPACITY (0x25)
#define BOT_OPCODE_REQUEST_SENSE (0x03)
#define BOT_OPCODE_READ32 (0x28)
#define BOT_OPCODE_WRITE32 (0x2A)
#define BOT_OPCODE_READ64 (0x88)
#define BOT_OPCODE_WRITE64 (0x8A)
#define BOT_SENSE_KEY_NO_SENSE (0x00)
#define BOT_SENSE_KEY_RECOVERED_ERROR (0x01)
#define BOT_SENSE_KEY_NOT_READY (0x02)
#define BOT_SENSE_KEY_MEDIUM_ERROR (0x03)
#define BOT_SENSE_KEY_HARDWARE_ERROR (0x04)
#define BOT_SENSE_KEY_ILLEGAL_REQUEST (0x05)
#define BOT_SENSE_KEY_UNIT_ATTENTION (0x06)
#define BOT_SENSE_KEY_DATA_PROTECT (0x07)
#define BOT_SENSE_KEY_BLANK_CHECK (0x08)
#define BOT_SENSE_KEY_VENDOR_SPECIFIC (0x09)
#define BOT_SENSE_KEY_COPY_ABORTED (0x0A)
#define BOT_SENSE_KEY_ABORTED_CMD (0x0B)
#define BOT_SENSE_KEY_EQUAL (0x0C)
#define BOT_SENSE_KEY_VOLUME_OVERFLOW (0x0D)
#define BOT_SENSE_KEY_VERIFY_FAIL (0x0E)
#define BOT_RESPONSE_CODE_FIXED_CURRENT_ERROR (0x70)
#define BOT_RESPONSE_CODE_FIXED_DEFERRED_ERROR (0x71)
#define BOT_RESPONSE_CODE_DESC_CURRENT_ERROR (0x72)
#define BOT_RESPONSE_CODE_DESC_DEFERRED_ERROR (0x73)
#define BOT_SERVICE_ACTION_GET_EXTENDED_CAPACITY (0x10)
#define BOT_MAX_OPERATION_PAGE_COUNT (64)
struct usb_bot_cmd{
	uint32_t signature;	
	uint32_t cmd_id;
	uint32_t data_transfer_len;
	uint8_t flags;
	uint8_t logical_unit_number;
	uint8_t cmd_block_len;
	uint8_t cmd_block[16];
	uint8_t padding0;
}__attribute__((packed));
struct usb_bot_cmd_status{
	uint32_t signature;
	uint32_t cmd_id;
	uint32_t data_unused;
	uint8_t status;
	uint8_t padding0[3];
}__attribute__((packed));
struct usb_bot_info_extra{
	unsigned char vendor_id[8];
	unsigned char product_id[16];
	unsigned char product_version[4];
}__attribute__((packed));
struct usb_bot_info{
	uint8_t device_type:5;
	uint8_t qualifier:3;

	uint8_t reserved0:7;
	uint8_t removable:1;

	uint8_t version;

	uint8_t response_data_format:4;
	uint8_t hic_support:1;	
	uint8_t normal_aca_support:1;
	uint8_t obsolete:2;

	uint8_t extra_len;

	uint8_t protect:1;
	uint8_t reserved1:2;
	uint8_t third_party_copy:1;
	uint8_t target_port_group:2;
	uint8_t access_ctrl_coordinator:1;
	uint8_t scc:1;

	uint8_t wide_address:1;
	uint8_t reserved2:2;
	uint8_t medium_changer:1;
	uint8_t multi_port:1;
	uint8_t vendor_specific0:1;
	uint8_t enclosure_services:1;
	uint8_t basic_queuing:1;

	uint8_t vendor_specific1:1;
	uint8_t cmd_queuing:1;
	uint8_t linked_cmds:1;
	uint8_t sync_transfer:1;
	uint8_t wide_bus:1;
	uint8_t reserved3:2;
	struct usb_bot_info_extra extra;
}__attribute__((packed));
struct usb_bot_extended_capacity{
	uint64_t last_lba;
	uint32_t block_size;

	uint8_t protection_enable:1;
	uint8_t protection_type:3;
	uint8_t reserved0:4;

	uint8_t logical_blocks_per_pbe:4;
	uint8_t protection_information_iexp:4;

	uint16_t lowest_aligned_lba:15;
	uint16_t lbpme:1;
	uint16_t lpbrz;

	uint8_t reserved1[16];
}__attribute__((packed));
struct usb_bot_standard_capacity{
	uint32_t last_lba;
	uint32_t block_size;
}__attribute__((packed));
struct usb_bot_get_drive_info_cmd{
	uint8_t opcode;
	uint8_t evpd:1;
	uint8_t reserved0:7;
	uint8_t page_code;
	uint16_t alloc_len;
	uint8_t control;
	uint8_t padding0[10];
}__attribute__((packed));
struct usb_bot_get_standard_capacity_cmd{
	uint8_t opcode;
	uint8_t reserved0:1;
	uint8_t obsolete:1;
	uint8_t reserved1:6;
	uint32_t lba;
	uint16_t reserved2;
	uint8_t pmi:1;
	uint8_t reserved3:7;
	uint8_t control;
	uint8_t padding0[6];
}__attribute__((packed));
struct usb_bot_request_sense_cmd{
	uint8_t opcode;
	uint8_t desc_flags;
	uint8_t reserved0;
	uint8_t reserved1;
	uint8_t alloc_length;
	uint8_t control;
}__attribute__((packed));
struct usb_bot_sector32_cmd{
	uint8_t opcode;
	uint8_t flags;
	uint32_t lba;
	uint8_t reserved0;
	uint16_t transfer_length;
	uint8_t control;
}__attribute__((packed));
struct usb_bot_sector64_cmd{
	uint8_t opcode;
	uint8_t flags;
	uint64_t lba;
	uint32_t transfer_len;
	uint8_t group;
	uint8_t control;
}__attribute__((packed));
struct usb_bot_get_extended_capacity_cmd{
	uint8_t opcode;
	uint8_t service_action:5;
	uint8_t reserved0:3;
	uint64_t lba;
	uint32_t alloc_len;	
	uint8_t pmi:1;
	uint8_t reserved2:7;
	uint8_t control;
}__attribute__((packed));
struct usb_bot_cmd_sense{
	uint8_t response_code;
	uint8_t reserved0;
	uint8_t sense_key;
	uint8_t info[4];
	uint8_t additional_sense_len;
	uint8_t cmd_specific[4];
	uint8_t additional_sense_code;
	uint8_t additional_sense_code_detail;
	uint8_t field_replaceable_unit_code;
	uint8_t sense_key_specific[3];
}__attribute__((packed));
struct usb_bot_packet_request{
	struct usb_bot_cmd cmd;
	struct usb_bot_cmd_status cmdStatus;	
	unsigned char* pBuffer;
	uint64_t bufferSize;
	uint8_t direction;
};
struct usb_bot_cmd_response{
	struct xhci_trb eventTrb;
	struct usb_bot_cmd_sense cmdSense;
	uint8_t senseAvailable;
};
struct usb_bot_info_desc{
	struct usb_bot_info info;
};
struct usb_bot_capacity_desc{
	uint8_t extendedSupport;
	uint64_t sectorCount;
	uint64_t blockSize;
	struct usb_bot_standard_capacity standardCapacity;
	struct usb_bot_extended_capacity extendedCapacity;
};
struct usb_bot_desc_list_info{
	struct usb_bot_drive_desc* pDescList;
	uint64_t descListSize;
};
struct usb_bot_drive_desc{
	uint8_t port;
	uint8_t interfaceId;
	uint64_t driveId;
	uint8_t inputEndpointId;
	uint8_t outputEndpointId;
	struct usb_bot_info_desc infoDesc;
	struct usb_bot_capacity_desc capacityDesc;
};
int usb_bot_driver_init(void);
int usb_bot_send_packet(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_packet_request* pPacketRequest, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_send_packet_no_data(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_packet_request* pPacketRequest, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_get_info(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_info_desc* pDriveInfo, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_get_extended_capacity(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_capacity_desc* pCapacityDesc, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_get_standard_capacity(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_capacity_desc* pCapacityDesc, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_get_capacity(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_capacity_desc* pCapacityDesc, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_request_sense(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_cmd_sense* pSense, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_read_sectors32(struct usb_bot_drive_desc* pDriveDesc, uint32_t lba, uint16_t sectorCount, unsigned char* pBuffer, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_read_sectors64(struct usb_bot_drive_desc* pDriveDesc, uint64_t lba, uint32_t sectorCount, unsigned char* pBuffer, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_read_sectors(struct usb_bot_drive_desc* pDriveDesc, uint64_t lba, uint32_t sectorCount, unsigned char* pBuffer, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_write_sectors32(struct usb_bot_drive_desc* pDriveDesc, uint32_t lba, uint16_t sectorCount, unsigned char* pBuffer, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_write_sectors64(struct usb_bot_drive_desc* pDriveDesc, uint64_t lba, uint32_t sectorCount, unsigned char* pBuffer, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_write_sectors(struct usb_bot_drive_desc* pDriveDesc, uint64_t lba, uint32_t sectorCount, unsigned char* pBuffer, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_ping(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_cmd_response* pCmdResponse);
int usb_bot_get_sense_key_name(uint8_t senseKey, const unsigned char** ppKeyName);
int usb_bot_get_response_code_name(uint8_t responseCode, const unsigned char** ppResponseName);
int usb_bot_init_bot_desc_list(void);
int usb_bot_get_bot_desc(uint8_t port, uint8_t interfaceId, struct usb_bot_drive_desc** ppBotDesc);
int usb_bot_interface_register(uint8_t port, uint8_t interfaceId);
int usb_bot_interface_unregister(uint8_t port, uint8_t interfaceId);
int usb_bot_subsystem_drive_register(uint64_t driveId);
int usb_bot_subsystem_drive_unregister(uint64_t driveId);
int usb_bot_subsystem_get_drive_info(uint64_t driveId, struct drive_info* pDriveInfo);
int usb_bot_subsystem_read_sectors(uint64_t driveId, uint64_t lba, uint64_t sectorCount, unsigned char* pBuffer);
int usb_bot_subsystem_write_sectors(uint64_t driveId, uint64_t lba, uint64_t sectorCount, unsigned char* pBuffer);
#endif
