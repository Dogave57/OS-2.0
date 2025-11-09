#ifndef _SMBIOS
#define _SMBIOS
#include <stdint.h>
#define SMBIOS_FIRMARE_INFO ((uint8_t)0)
#define SMBIOS_SYS_INFO ((uint8_t)1)
#define SMBIOS_MOBO_INFO ((uint8_t)2)
#define SMBIOS_CASE_INFO ((uint8_t)3)
#define SMBIOS_CPU_INFO ((uint8_t)4)
#define SMBIOS_CACHE_INFO ((uint8_t)7)
#define SMBIOS_SYS_SLOT_INFO ((uint8_t)8)
#define SMBIOS_PHYSICAL_MEM_ARRAY ((uint8_t)16)
#define SMBIOS_MEM_DEV_INFO ((uint8_t)17)
#define SMBIOS_MEM_ARRAY_MAPPED_ADDR ((uint8_t)19)
#define SMBIOS_MEM_DEV_MAPPED_ADDR ((uint8_t)20)
#define SMBIOS_SYSBOOT_INFO ((uint8_t)32)
struct smbios_eps{
	uint32_t anchor_str;
	uint8_t checksum;
	uint8_t len;
	uint8_t majorVersion;
	uint8_t minorVersion;
	uint16_t maxStructSize;
	uint8_t entryPointRevision;
	unsigned char fmtArea[5];
	unsigned char interAnchor[5];
	uint8_t interChecksum;
	uint16_t tableLen;
	uint32_t tableAddr;
	uint16_t numberOfStructures;
	uint8_t BCDRevision;
}__attribute__((packed));
struct smbios_hdr{
	uint8_t type;
	uint8_t len;
	uint16_t handle;
}__attribute__((packed));
int smbios_init(void);
int smbios_get_entry(uint8_t type, struct smbios_hdr** ppEntry);
int smbios_get_string(struct smbios_hdr* pentry, unsigned int index, unsigned char** ppStr);
#endif
