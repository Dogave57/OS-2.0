#ifndef _PE
#define _PE
#include <stdint.h>
#include "mz.h"
#define PE_MAGIC 0x4550
#define PE_OPT32_MAGIC 0x010B
#define PE_OPT64_MAGIC 0x020B
#define DATA_DIRECTORY_EXPORT_TABLE 0
#define DATA_DIRECTORY_IMPORT_TABLE 1
#define DATA_DIRECTORY_RESOURCE_TABLE 2
#define DATA_DIRECTORY_EXCEPTION_TABLE 3
#define DATA_DIRECTORY_CERT_TABLE_OFFSET 4
#define DATA_DIRECTORY_BASE_RELOC_TABLE 5
#define DATA_DIRECTORY_DEBUG_DATA 6
#define DATA_DIRECTORY_ARCHITECTURE_DATA 7
#define DATA_DIRECTORY_GLOBAL_PTR_REGISTER 8
#define DATA_DIRECTORY_TLS 9
#define DATA_DIRECTORY_LOAD_CONFIG_TABLE 10
#define DATA_DIRECTORY_BOUND_IMPORT_TABLE 11
#define DATA_DIRECTORY_IAT 12
#define DATA_DIRECTORY_DELAY_IMPORT_DESC 13
#define DATA_DIRECTORY_CLR_HDR 14
#define DATA_DIRECTORY_RESERVED 15
struct PE_HDR{
	uint32_t magic;
	uint16_t machine;
	uint16_t section_cnt;
	uint32_t timeDateStamp;
	uint32_t psymtab;
	uint32_t symbol_cnt;
	uint16_t optheader_size;
	uint16_t characteristics;
};
struct PE32_OPTHDR{
	uint16_t magic;
	uint8_t majorLinkerVersion;
	uint8_t minorLinkerVersion;	
	uint32_t sizeOfCode;
	uint32_t sizeOfInitializedData;
	uint32_t sizeOfUnitializedData;
	uint32_t addressOfEntryPoint;
	uint32_t baseOfCode;
	uint32_t baseOfData;
	uint32_t imageBase;
	uint32_t sectionAlignment;
	uint32_t fileAlignment;
	uint16_t majorOperatingSystemVersion;
	uint16_t minorOperatingSystemVersion;
	uint16_t majorImageVersion;
	uint16_t minorImageVersion;
	uint16_t majorSubsystemVersion;
	uint16_t minorSubsystemVersion;
	uint32_t win32VersionValue;
	uint32_t sizeOfImage;
	uint32_t sizeOfHeaders;
	uint32_t checksum;
	uint16_t subsystem;
	uint16_t dllCharacteristics;
	uint32_t sizeOfStackReserve;
	uint32_t sizeOfHeapReserve;
	uint32_t sizeOfHeapCommit;
	uint32_t loaderFlags;
	uint32_t numberOfRvaAndSizes;
};
struct PE64_OPTHDR{
	uint16_t magic;
	uint8_t majorLinkerVersion;
	uint8_t minorLinkerVersion;
	uint32_t sizeOfCode;
	uint32_t sizeOfInitializedData;
	uint32_t sizeOfUninitializedData;
	uint32_t addressOfEntryPoint;
	uint32_t baseOfCode;
	uint64_t imageBase;
	uint32_t sectionAlignment;
	uint32_t fileAlignment;
	uint16_t majorOperatingSystemVersion;
	uint16_t minorOperatingSystemVersion;
	uint16_t majorImageVersion;
	uint16_t minorImageVersion;
	uint16_t majorSubsystemVersion;
	uint16_t minorSubsystemVersion;
	uint32_t win32VersionValue;
	uint32_t sizeOfImage;
	uint32_t sizeOfHeaders;
	uint32_t checksum;
	uint16_t subsystem;
	uint16_t dllCharacteristics;
	uint64_t sizeOfStackReserve;
	uint64_t sizeOfStackCommit;
	uint64_t sizeOfHeapReserve;
	uint64_t sizeOfHeapCommit;
	uint32_t loaderFlags;
	uint32_t numberOfRvaAndSizes;
};
struct IMAGE_SECTION_HEADER{
	char sectname[8];
	uint32_t virtualSize;
	uint32_t virtualAddress;
	uint32_t sizeOfRawData;
	uint32_t pointerToRawData;
	uint32_t pointerToRelocations;
	uint32_t pointerToLineNumbers;
	uint16_t numberOfRelocations;
	uint16_t numberOfLineNumbers;
	uint32_t characteristics;
};
struct IMAGE_DATA_DIRECTORY{
	uint32_t virtualAddress;
	uint32_t size;
};
#endif
