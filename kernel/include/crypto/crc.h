#ifndef _CRC
#define _CRC
#define CRC32_MAGIC 0xEDB88320
int get_crc32(unsigned char* pBlob, uint64_t blobSize, uint32_t* pCrc);
#endif
