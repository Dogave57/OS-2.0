#include "acpi.h"
#include "smp.h"
int getCpuCoreCount(unsigned int* pcount){
	if (!pcount)
		return -1;
	struct acpi_madt* pMadt = (struct acpi_madt*)0x0;
	if (acpi_find_table('CIPA', (struct acpi_sdt_hdr**)(&pMadt))!=0){
		return -1;
	}
	struct acpi_madtEntry_hdr* pCurrentHdr = (struct acpi_madtEntry_hdr*)(pMadt+1);
	unsigned int coreCount = 0;
	for (;pCurrentHdr->len;pCurrentHdr = (struct acpi_madtEntry_hdr*)(((unsigned char*)pCurrentHdr)+pCurrentHdr->len)){
		if (pCurrentHdr->type!=MADT_ENTRY_LAPIC)
			continue;
		coreCount++;
	}
	*pcount = coreCount;
	return 0;
}
