#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "align.h"
#include "crypto/crc.h"
uint32_t crc32_table[256] = {0};
uint64_t table_built = 0;
int build_crc32_table(void){
	uint32_t crc = 0;
	for (uint64_t i = 0;i<256;i++){
		crc = i;
		for (uint64_t j = 0;j<8;j++){
			if (crc&1){
				crc = (crc>>1)^CRC32_MAGIC;
				continue;
			}
			crc>>=1;
		}
		crc32_table[i] = crc;
	}
	table_built = 1;
	return 0;
}
int get_crc32(unsigned char* pBlob, uint64_t blobSize, uint32_t* pCrc){
	if (!pBlob||!blobSize||!pCrc)
		return -1;
	if (!table_built){
		if (build_crc32_table()!=0)
			return -1;
	}
	uint32_t crc = 0xFFFFFFFF;
	for (uint64_t i = 0;i<blobSize;i++){
		crc = (crc>>8) ^ crc32_table[(crc^pBlob[i])&0xFF];
	}
	crc = ~crc;
	*pCrc = crc;
	return 0;
}
