#ifndef _GPT
#define _GPT
#include "crypto/guid.h"
#define GPT_SIGNATURE ((uint64_t)0x5452415020494645)
#define GPT_HEADER_SIZE (0x5C)
#define GPT_PARTITION_ENTRY_SIZE (128)
#define GPT_ESP_GUID {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}
#define GPT_BASIC_DATA_GUID {0xAF, 0x3D, 0xC6, 0xEF, 0x8F, 0x25, 0xD2, 0x11, 0xA2, 0xA0, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}
#define GPT_LINUX_FILESYSTEM_DATA_GUID {0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define GPT_LINUX_SWAP_PARTITION_GUID {0x06, 0x57, 0xFD, 0x82, 0x72, 0x09, 0x60, 0x48, 0xB7, 0x09, 0x22, 0x08, 0x21, 0x22, 0x28, 0x2E}
#define GPT_MICROSOFT_BASIC_DATA_GUID {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}
#define GPT_MICROSOFT_RESERVED_GUID {0x00, 0x00, 0x21, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
struct gpt_partition{
	uint8_t partTypeGuid[16];
	uint8_t uniquePartGuid[16];
	uint64_t start_lba;
	uint64_t end_lba;
	uint64_t attributes;
	unsigned char name[72];
}__attribute__((packed));
struct gpt_header{
	uint64_t signature;
	uint32_t version;
	uint32_t headerSize;
	uint32_t checksum;
	uint32_t reserved0;
	uint64_t header_lba;
	uint64_t backup_header_lba;
	uint64_t first_usable_block;
	uint64_t last_usable_block;
	uint8_t disk_guid[16];
	uint64_t partition_table_lba;
	uint32_t partition_count;
	uint32_t partition_entry_size;
	uint32_t partition_checksum;
}__attribute__((packed));
int gpt_get_partition(uint64_t id, uint64_t partition, struct gpt_partition* pPartitionEntry);
int gpt_set_partition(uint64_t id, uint64_t partition, struct gpt_partition partitionEntry);
int gpt_get_free_partition(uint64_t id, uint64_t* pPartition);
int gpt_remove_partition(uint64_t id, uint64_t partition);
int gpt_add_partition(uint64_t id, unsigned char* name, uint64_t start, uint64_t size, uint64_t attribs, unsigned char* type, uint64_t* pPartition);
int gpt_sync(uint64_t id);
int gpt_get_header(uint64_t id, struct gpt_header* pGptHeader);
int gpt_set_header(uint64_t id, struct gpt_header gptHeader);
int gpt_get_backup_partition_table_lba(uint64_t id, uint64_t* pLba);
int gpt_get_partition_table_checksum(uint64_t id, uint32_t* pChecksum);
int gpt_get_backup_partition_table_checksum(uint64_t id, uint32_t* pChecksum);
int gpt_get_header_checksum(uint64_t id, uint32_t* pChecksum);
int gpt_verify(uint64_t id);
int gpt_get_partition_data_space(uint64_t drive_id, uint64_t* pSpace);
#endif
