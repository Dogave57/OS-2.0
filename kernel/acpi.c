#include "stdlib.h"
#include "bootloader.h"
#include "acpi.h"
struct acpi_xsdp* pXsdp = (struct acpi_xsdp*)0x0;
struct acpi_sdt_hdr* pXsdt = (struct acpi_sdt_hdr*)0x0;
int acpi_init(void){
	pXsdp = (struct acpi_xsdp*)0x0;
	pXsdt = (struct acpi_sdt_hdr*)0x0;
	if (acpi_find_xsdp(&pXsdp)!=0){
		printf(L"failed to find xsdp\r\n");
		return -1;
	}
	printf(L"xsdp: %p\r\n", (void*)pXsdp);
	if (acpi_find_xsdt(&pXsdt)!=0){
		printf(L"failed to find XSDT\r\n");
		return -1;
	}
	if (pXsdt->signature!=XSDT_SIGNATURE){
		printf(L"invalid XSDT signature\r\n");
		return -1;
	}
	printf(L"xsdt: %p\r\n", (void*)pXsdt);
	printf(L"xsdt signature: 0x%x\r\n", pXsdt->signature);
	return 0;
}
int acpi_find_table(unsigned int signature, struct acpi_sdt_hdr** ppTable){

	return -1;
}
int acpi_find_xsdt(struct acpi_sdt_hdr** ppXsdt){
	if (!ppXsdt)
		return -1;
	if (!pXsdp){
		if (acpi_find_xsdp(&pXsdp)!=0)
			return -1;
	}	
	*ppXsdt = (struct acpi_sdt_hdr*)pXsdp->xsdt_addr;
	return 0;
}
int acpi_find_xsdp(struct acpi_xsdp** ppXsdp){
	if (!ppXsdp)
		return -1;
	if (pXsdp){
		*ppXsdp = pXsdp;
		return 0;
	}
	if (pbootargs->acpiInfo.pXsdp){
		pXsdp = pbootargs->acpiInfo.pXsdp;
		*ppXsdp = pXsdp;
		return 0;
	}
	return -1;	
}
