#include "bootloader.h"
#include "stdlib.h"
#include "smbios.h"
struct smbios_hdr* pEntries[64]={0};
struct smbios_hdr* pSmbiosData = (struct smbios_hdr*)0x0;
int smbios_init(void){
	struct smbios_hdr* pentry = (struct smbios_hdr*)0x0;
	smbios_get_entry(SMBIOS_FIRMARE_INFO, &pentry);
	smbios_get_entry(SMBIOS_SYS_INFO, &pentry);
	smbios_get_entry(SMBIOS_MOBO_INFO, &pentry);
	smbios_get_entry(SMBIOS_CASE_INFO, &pentry);
	smbios_get_entry(SMBIOS_CPU_INFO, &pentry);
	smbios_get_entry(SMBIOS_CACHE_INFO, &pentry);
	smbios_get_entry(SMBIOS_SYS_SLOT_INFO, &pentry);
	smbios_get_entry(SMBIOS_PHYSICAL_MEM_ARRAY, &pentry);
	smbios_get_entry(SMBIOS_MEM_DEV_INFO, &pentry);
	smbios_get_entry(SMBIOS_MEM_ARRAY_MAPPED_ADDR, &pentry);
	smbios_get_entry(SMBIOS_MEM_DEV_MAPPED_ADDR, &pentry);
	smbios_get_entry(SMBIOS_SYSBOOT_INFO, &pentry);
	pSmbiosData = (struct smbios_hdr*)((uint64_t)pbootargs->smbiosInfo.pSmbios->struct_table_addr);
	return 0;
}
int smbios_get_entry(uint8_t type, struct smbios_hdr** ppEntry){
	if (!ppEntry)
		return -1;
	if (pEntries[type]){
		*ppEntry = pEntries[type];
		return 0;
	}
	struct smbios_hdr* pCurrentEntry = pSmbiosData;
	for (;;){
		if (pCurrentEntry->type==127||!pCurrentEntry->len)
			break;
		if (pCurrentEntry->type!=type){
			pCurrentEntry = (struct smbios_hdr*)(((unsigned char*)pCurrentEntry)+pCurrentEntry->len);	
			uint16_t* pblob = (uint16_t*)pCurrentEntry;
			for (;*pblob;pblob++){};
			pblob++;
			pCurrentEntry = (struct smbios_hdr*)pblob;
			continue;
		}
		*ppEntry = pCurrentEntry;
		pEntries[type] = pCurrentEntry;
		return 0;
	}
	return -1;
}
int smbios_get_string(struct smbios_hdr* pentry, unsigned int str_index, unsigned char** pStr){
	if (!pentry||!pStr)
		return -1;
	unsigned char* pblob = (unsigned char*)(((unsigned char*)pentry)+pentry->len);
	unsigned int str_start = 0;	
	unsigned int current_index = 0;
	for (unsigned int i = 0;;i++){
		if (pblob[i])
			continue;
		if (current_index==str_index){
			*pStr = pblob+str_start;
			return 0;
		}
		str_start = i+1;
		current_index++;	
		if (!pblob[i+1])
			return -1;
	}
	return -1;
}
