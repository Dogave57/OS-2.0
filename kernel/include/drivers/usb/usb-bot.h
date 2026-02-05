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
#define BOT_OPCODE_GET_INFO (0x12)
#define BOT_OPCODE_READ_CAPACITY (0x0A)
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
	struct usb_bot_info_extra extra;
}__attribute__((packed));
struct usb_bot_get_drive_info_cmd{
	uint8_t opcode;
	uint8_t evpd:1;
	uint8_t reserved0:7;
	uint8_t page_code;
	uint16_t alloc_len; //BIG ENDIAN
	uint8_t control;
	uint8_t padding0[10];
}__attribute__((packed));
struct usb_bot_get_capacity_cmd{
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
struct usb_bot_packet_request{
	struct usb_bot_cmd cmd;
	struct usb_bot_cmd_status* pCmdStatus;
	unsigned char* pBuffer;
	uint64_t bufferSize;
};
struct usb_bot_drive_desc{
	uint8_t port;
	uint8_t interfaceId;
	uint8_t inputEndpointId;
	uint8_t outputEndpointId;
};
int usb_bot_driver_init(void);
int usb_bot_send_packet(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_packet_request packetRequest, struct xhci_trb* pEventTrb);
int usb_bot_get_info(struct usb_bot_drive_desc* pDriveDesc, struct usb_bot_info* pDriveInfo);
int usb_bot_interface_register(uint8_t port, uint8_t interfaceId);
int usb_bot_interface_unregister(uint8_t port, uint8_t interfaceId);
#endif
