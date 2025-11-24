#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "subsystem/drive.h"
#include "crypto/crc.h"
#include "align.h"
#include "drivers/timer.h"
#include "drivers/gpt.h"
uint64_t last_drive_id = 0xFFFFFFFFFFFFFFFF;
struct gpt_header last_gpt_header = {0};
int gpt_get_partition(uint64_t id, uint64_t partition, struct gpt_partition* pPartitionEntry){
	if (!pPartitionEntry)
		return -1;
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint64_t partition_table_lba = gptHeader.partition_table_lba;
	uint64_t backup_partition_table_lba = 0;
	if (gpt_get_backup_partition_table_lba(id, &backup_partition_table_lba)!=0)
		return -1;
	uint64_t partition_entry_lba_offset = (partition*GPT_PARTITION_ENTRY_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t partition_entry_offset = (partition*GPT_PARTITION_ENTRY_SIZE)%DRIVE_SECTOR_SIZE;
	unsigned char sectorData[512] = {0};
	if (drive_read_sectors(id, partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	struct gpt_partition partitionEntry = *(struct gpt_partition*)(sectorData+partition_entry_offset);
	*pPartitionEntry = partitionEntry;
	return 0;
}
int gpt_set_partition(uint64_t id, uint64_t partition, struct gpt_partition partitionEntry){
	//corrupts GPT disk
	return -1;
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint64_t partition_table_lba = gptHeader.partition_table_lba;
	uint64_t backup_partition_table_lba = 0;
	if (gpt_get_backup_partition_table_lba(id, &backup_partition_table_lba)!=0)
		return -1;
	uint64_t partition_entry_lba_offset = (partition*GPT_PARTITION_ENTRY_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t partition_entry_offset = (partition*GPT_PARTITION_ENTRY_SIZE)%DRIVE_SECTOR_SIZE;
	unsigned char sectorData[512] = {0};
	if (drive_read_sectors(id, partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	struct gpt_partition* pPartitionEntry = (struct gpt_partition*)(sectorData+partition_entry_offset);
	*pPartitionEntry = partitionEntry;
	if (drive_write_sectors(id, partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	if (drive_write_sectors(id, backup_partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	if (gpt_sync(id)!=0)
		return -1;
	return 0;
}
int gpt_sync(uint64_t id){
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint32_t partition_table_checksum = 0;
	uint32_t backup_partition_table_checksum = 0;
	if (gpt_get_partition_table_checksum(id, &partition_table_checksum)!=0)
		return -1;
	if (gpt_get_backup_partition_table_checksum(id, &backup_partition_table_checksum)!=0)
		return -1;
	if (partition_table_checksum!=backup_partition_table_checksum)
		return -1;
	gptHeader.partition_checksum = partition_table_checksum;
	if (gpt_set_header(id, gptHeader)!=0)
		return -1;
	if (gpt_verify(0)!=0){
		return -1;
	}
	return 0;
}
int gpt_get_header(uint64_t id, struct gpt_header* pGptHeader){
	if (!pGptHeader)
		return -1;
	if (id==last_drive_id){
		*pGptHeader = last_gpt_header;
		return 0;
	}
	unsigned char sectorData[512] = {0};
	if (drive_read_sectors(id, 1, 1, (unsigned char*)sectorData)!=0)
		return -1;
	struct gpt_header gptHeader = *(struct gpt_header*)(sectorData);
	if (gptHeader.signature!=GPT_SIGNATURE)
		return -1;
	*pGptHeader = gptHeader;
	last_drive_id = id;
	last_gpt_header = gptHeader;
	return 0;
}
int gpt_set_header(uint64_t id, struct gpt_header newGptHeader){
	if (newGptHeader.signature!=GPT_SIGNATURE)
		return -1;
	unsigned char sectorData[512] = {0};
	if (drive_read_sectors(id, 1, 1, (unsigned char*)sectorData)!=0){
		printf(L"failed to read GPT header sector\r\n");
		return -1;
	}
	struct gpt_header* pGptHeader = (struct gpt_header*)sectorData;
	*pGptHeader = newGptHeader;
	if (drive_write_sectors(id, 1, 1, (unsigned char*)sectorData)!=0){
		printf(L"failed to write to GPT header\r\n");
		return -1;
	}
	if (drive_write_sectors(id, newGptHeader.backup_header_lba, 1, (unsigned char*)sectorData)!=0){
		printf(L"failed to write to backup header\r\n");
		return -1;
	}
	if (last_drive_id==id){
		last_gpt_header = newGptHeader;
	}
	return 0;
}
int gpt_get_backup_partition_table_lba(uint64_t id, uint64_t* pLba){
	if (!pLba)
		return -1;
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint64_t partitionTableSize = gptHeader.partition_count*GPT_PARTITION_ENTRY_SIZE;
	uint64_t partitionTableSectors = align_up(partitionTableSize, DRIVE_SECTOR_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t lba = gptHeader.backup_header_lba-partitionTableSectors;
	*pLba = lba;
	return 0;
}
int gpt_get_partition_table_checksum(uint64_t id, uint32_t* pChecksum){
	if (!pChecksum)
		return -1;
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint64_t partition_table_lba = gptHeader.partition_table_lba;
	uint64_t partition_table_size = gptHeader.partition_count*GPT_PARTITION_ENTRY_SIZE;
	uint64_t partition_table_sectors = align_up(partition_table_size, DRIVE_SECTOR_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t partition_table_pages = align_up(partition_table_size, PAGE_SIZE)/PAGE_SIZE;
	struct gpt_partition* pPartitionTable = (struct gpt_partition*)0x0;
	uint32_t checksum = 0;
	if (virtualAllocPages((uint64_t*)&pPartitionTable, partition_table_pages, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	if (drive_read_sectors(id, partition_table_lba, partition_table_sectors, (unsigned char*)pPartitionTable)!=0){
		virtualFreePages((uint64_t)pPartitionTable, partition_table_pages);
		return -1;
	}
	if (get_crc32((unsigned char*)pPartitionTable, partition_table_size, &checksum)!=0){
		virtualFreePages((uint64_t)pPartitionTable, partition_table_pages);
		return -1;
	}
	virtualFreePages((uint64_t)pPartitionTable, partition_table_pages);
	*pChecksum = checksum;
	return 0;
}
int gpt_get_backup_partition_table_checksum(uint64_t id, uint32_t* pChecksum){
	if (!pChecksum)
		return -1;
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint64_t partition_table_lba = 0;
	if (gpt_get_backup_partition_table_lba(id, &partition_table_lba)!=0)
		return -1;
	uint64_t partition_table_size = gptHeader.partition_count*GPT_PARTITION_ENTRY_SIZE;
	uint64_t partition_table_sectors = align_up(partition_table_size, DRIVE_SECTOR_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t partition_table_pages = align_up(partition_table_size, PAGE_SIZE)/PAGE_SIZE;
	struct gpt_partition* pPartitionTable = (struct gpt_partition*)0x0;
	uint32_t checksum = 0;
	if (virtualAllocPages((uint64_t*)&pPartitionTable, partition_table_pages, PTE_RW|PTE_NX, 0, PAGE_TYPE_NORMAL)!=0)
		return -1;
	if (drive_read_sectors(id, partition_table_lba, partition_table_sectors, (unsigned char*)pPartitionTable)!=0){
		virtualFreePages((uint64_t)pPartitionTable, partition_table_pages);
		return -1;
	}
	if (get_crc32((unsigned char*)pPartitionTable, partition_table_size, &checksum)!=0){
		virtualFreePages((uint64_t)pPartitionTable, partition_table_pages);
		return -1;
	}
	virtualFreePages((uint64_t)pPartitionTable, partition_table_pages);
	*pChecksum = checksum;
	return 0;
}
int gpt_verify(uint64_t id){
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint32_t partition_table_checksum = 0;
	uint32_t backup_partition_table_checksum = 0;
	if (gpt_get_partition_table_checksum(id, &partition_table_checksum)!=0)
		return -1;
	if (gpt_get_backup_partition_table_checksum(id, &backup_partition_table_checksum)!=0){
		return -1;
	}
	if (gptHeader.partition_checksum!=partition_table_checksum){
		printf(L"normal partition table checksum no match\r\n");
		return -1;
	}
	if (gptHeader.partition_checksum!=backup_partition_table_checksum){
		printf(L"backup partition table checksum no match\r\n");
		return -1;
	}
	return 0;
}
