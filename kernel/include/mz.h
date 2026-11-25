#ifndef _MZ
#define _MZ
#define MZ_MAGIC 0x5A4D
struct IMAGE_DOS_HEADER{
	uint16_t magic;
	uint16_t extraBytes;
	uint16_t pagecnt;
	uint16_t relocCnt;
	uint16_t headerSize;
	uint16_t minAlloc;
	uint16_t maxAlloc;
	uint16_t ss;
	uint16_t sp;
	uint16_t checksum;	
	uint16_t ip;
	uint16_t cs;
	uint16_t relocFileAddr;
	uint16_t overlay;
	uint16_t reserved[4];
	uint16_t oemIdent;
	uint16_t oemInfo;
	uint16_t reserved1[10];
	unsigned long exeOffset;
};
#endif
