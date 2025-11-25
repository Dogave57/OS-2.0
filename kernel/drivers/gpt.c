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
	memset((void*)sectorData, 0, sizeof(sectorData));
	if (drive_read_sectors(id, partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	struct gpt_partition partitionEntry = *(struct gpt_partition*)(sectorData+partition_entry_offset);
	*pPartitionEntry = partitionEntry;
	return 0;
}
int gpt_set_partition(uint64_t id, uint64_t partition, struct gpt_partition partitionEntry){
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint64_t partition_entry_lba_offset = (partition*GPT_PARTITION_ENTRY_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t partition_entry_offset = (partition*GPT_PARTITION_ENTRY_SIZE)%DRIVE_SECTOR_SIZE;
	uint64_t backup_partition_table_lba = 0;
	if (gpt_get_backup_partition_table_lba(id, &backup_partition_table_lba)!=0)
		return -1;
	unsigned char sectorData[512] = {0};
	if (drive_read_sectors(id, gptHeader.partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	struct gpt_partition* pPartitionEntry = (struct gpt_partition*)(sectorData+partition_entry_offset);
	*pPartitionEntry = partitionEntry;
	if (drive_write_sectors(id, gptHeader.partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	if (drive_write_sectors(id, backup_partition_table_lba+partition_entry_lba_offset, 1, (unsigned char*)sectorData)!=0)
		return -1;
	if (gpt_sync(id)!=0)
		return -1;
	return 0;
}
int gpt_get_free_partition(uint64_t id, uint64_t* pPartition){
	if (!pPartition)
		return -1;
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	for (uint64_t i = 0;i<gptHeader.partition_count;i++){
		struct gpt_partition partition = {0};
		if (gpt_get_partition(id, i, &partition)!=0)
			return -1;
		if (partition.start_lba||partition.end_lba)
			continue;
		*pPartition = i;
		return 0;
	}
	return -1;
}
int gpt_remove_partition(uint64_t id, uint64_t partition){
	struct gpt_partition newPartitionEntry = {0};
	memset((void*)&newPartitionEntry, 0, sizeof(struct gpt_partition));
	if (gpt_set_partition(id, partition, newPartitionEntry)!=0)
		return -1;
	return 0;
}
int gpt_add_partition(uint64_t id, unsigned char* name, uint64_t start, uint64_t size, uint64_t attribs, unsigned char* type, uint64_t* pPartition){
	if (!name||!pPartition)
		return -1;
	uint64_t partition = 0;
	if (gpt_get_free_partition(id, &partition)!=0)
		return -1;
	uint64_t start_lba = align_up(start, DRIVE_SECTOR_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t partition_sectors = align_up(size, DRIVE_SECTOR_SIZE)/DRIVE_SECTOR_SIZE;
	uint64_t end_lba = start_lba+partition_sectors;
	struct gpt_partition partitionEntry = {0};
	guidcpy((unsigned char*)partitionEntry.partTypeGuid, (unsigned char*)type);
	partitionEntry.start_lba = start_lba;
	partitionEntry.end_lba = end_lba;
	partitionEntry.attributes = attribs;
	for (uint64_t i = 0;i<sizeof(partitionEntry.name)-1&&name[i];i++){
		partitionEntry.name[i] = name[i];
	}
	if (guidgen((unsigned char*)partitionEntry.uniquePartGuid)!=0)
		return -1;
	if (gpt_set_partition(id, partition, partitionEntry)!=0)
		return -1;
	*pPartition = partition;
	return 0;
}
int gpt_sync(uint64_t id){
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint32_t partition_table_checksum = 0;
	uint32_t backup_partition_table_checksum = 0;
	uint32_t header_checksum = 0;
	if (gpt_get_partition_table_checksum(id, &partition_table_checksum)!=0)
		return -1;
	if (gpt_get_backup_partition_table_checksum(id, &backup_partition_table_checksum)!=0)
		return -1;
	if (partition_table_checksum!=backup_partition_table_checksum)
		return -1;
	gptHeader.partition_checksum = partition_table_checksum;
	gptHeader.checksum = 0;
	if (get_crc32((unsigned char*)&gptHeader, sizeof(struct gpt_header), &header_checksum)!=0)
		return -1;
	gptHeader.checksum = header_checksum;
	if (gpt_set_header(id, gptHeader)!=0)
		return -1;
	if (gpt_verify(id)!=0){
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
int gpt_get_header_checksum(uint64_t id, uint32_t* pChecksum){
	if (!pChecksum)
		return -1;
	struct gpt_header gptHeader = {0};
	uint32_t checksum = 0;
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	gptHeader.checksum = 0;
	if (get_crc32((unsigned char*)&gptHeader, sizeof(struct gpt_header), &checksum)!=0)
		return -1;
	*pChecksum = checksum;
	return 0;
}
int gpt_verify(uint64_t id){
	struct gpt_header gptHeader = {0};
	if (gpt_get_header(id, &gptHeader)!=0)
		return -1;
	uint32_t partition_table_checksum = 0;
	uint32_t backup_partition_table_checksum = 0;
	uint32_t header_checksum = 0;
	if (gpt_get_partition_table_checksum(id, &partition_table_checksum)!=0)
		return -1;
	if (gpt_get_backup_partition_table_checksum(id, &backup_partition_table_checksum)!=0){
		return -1;
	}
	if (gpt_get_header_checksum(id, &header_checksum)!=0)
		return -1;
	if (gptHeader.checksum!=header_checksum){
		printf(L"header checksum no match\r\n");
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
