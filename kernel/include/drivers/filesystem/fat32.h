#ifndef _FAT32
#define _FAT32
int fat32_openfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, uint64_t* pId);
#endif
