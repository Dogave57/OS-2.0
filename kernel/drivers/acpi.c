#include "stdlib/stdlib.h"
#include "bootloader.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "align.h"
#include "drivers/acpi.h"
struct acpi_xsdp* pXsdp = (struct acpi_xsdp*)0x0;
struct acpi_sdt_hdr* pXsdt = (struct acpi_sdt_hdr*)0x0;
struct acpi_madtEntry_ioapic* pIoApicInfo = (struct acpi_madtEntry_ioapic*)0x0;
struct aml_execution_context* pAmlContext = (struct aml_execution_context*)0x0;
int acpi_init(void){
	if (virtualMapPage((uint64_t)pbootargs->acpiInfo.pXsdp, (uint64_t)pbootargs->acpiInfo.pXsdp, PTE_RW, 1, 0, PAGE_TYPE_FIRMWARE_DATA)!=0){
		printf(L"failed to map XSDP\r\n");
		return -1;
	}
	pXsdp = (struct acpi_xsdp*)0x0;
	pXsdt = (struct acpi_sdt_hdr*)0x0;
	if (acpi_find_xsdp(&pXsdp)!=0){
		printf(L"failed to find xsdp\r\n");
		return -1;
	}
	if (acpi_find_xsdt(&pXsdt)!=0){
		printf(L"failed to find XSDT\r\n");
		return -1;
	}
	if (virtualMapPage((uint64_t)pXsdt, (uint64_t)pXsdt, PTE_RW, 1, 0, PAGE_TYPE_FIRMWARE_DATA)!=0){
		printf(L"failed to map XSDT\r\n");
		return -1;
	}
	if (pXsdt->signature!=XSDT_SIGNATURE){
		printf(L"invalid XSDT\r\n");
		return -1;
	}
	struct acpi_sdt_hdr** ptables = (struct acpi_sdt_hdr**)(pXsdt+1);
	unsigned int tablecnt = (pXsdt->len-sizeof(struct acpi_sdt_hdr))/sizeof(uint64_t);
	for (unsigned int i = 0;i<tablecnt;i++){
		uint64_t ptable = (uint64_t)ptables[i];
		if (virtualMapPage((uint64_t)ptable, (uint64_t)ptable, PTE_RW, 1, 0, PAGE_TYPE_FIRMWARE_DATA)!=0){
			printf(L"failed to map ACPI table\r\n");
			return -1;
		}
	}
	struct acpi_madt* madt = (struct acpi_madt*)0x0;
	if (acpi_find_table('CIPA', (struct acpi_sdt_hdr**)&madt)!=0){
		printf(L"failed to find MADT\r\n");
		return -1;
	}
	if (madt->hdr.signature!='CIPA'){
		printf(L"invalid MADT signature\r\n");
		return -1;
	}
	printf(L"LAPIC base: %p\r\n", madt->lapic_base);
	struct acpi_madtEntry_hdr* pMadtEntry = (struct acpi_madtEntry_hdr*)((unsigned char*)madt+0x2C);
	unsigned int madtEntries = (madt->hdr.len-sizeof(struct acpi_sdt_hdr))/sizeof(struct acpi_madtEntry_hdr);
	for (unsigned int i = 0;i<madtEntries;i++,pMadtEntry = (struct acpi_madtEntry_hdr*)((unsigned char*)pMadtEntry+pMadtEntry->len)){
		if (pMadtEntry->type!=MADT_ENTRY_IOAPIC)
			continue;
		pIoApicInfo = (struct acpi_madtEntry_ioapic*)(pMadtEntry+1);
	}
	if (!pIoApicInfo){
		printf(L"failed to get IOAPIC info\r\n");
		return -1;
	}
	if (aml_init_execution_context(&pAmlContext)!=0){
		printf(L"failed to initialize AML execution context\r\n");
		return -1;
	}
	return 0;
}
int acpi_find_table(unsigned int signature, struct acpi_sdt_hdr** ppTable){
	if (!ppTable)
		return -1;
	if (!pXsdt){
		if (acpi_find_xsdt(&pXsdt)!=0)
			return -1;
	}
	struct acpi_sdt_hdr** ptables = (struct acpi_sdt_hdr**)(pXsdt+1);
	unsigned int tablecnt = (pXsdt->len-sizeof(struct acpi_sdt_hdr))/sizeof(uint64_t);
	for (unsigned int i = 0;i<tablecnt;i++){
		struct acpi_sdt_hdr* table = ptables[i];
		if (!table)
			continue;
		if (table->signature!=signature)
			continue;
		*ppTable = table;
		return 0;
	}
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
int acpi_find_dsdt(struct acpi_sdt_hdr** ppSdtHdr){
	if (!ppSdtHdr)
		return -1;
	struct acpi_fadt* pFadt = (struct acpi_fadt*)0x0;
	if (acpi_find_table((unsigned int)'PCAF', (struct acpi_sdt_hdr**)&pFadt)!=0)
		return -1;
	virtualMapPage((uint64_t)pFadt, (uint64_t)pFadt, PTE_RW|PTE_NX, 0, 0, PAGE_TYPE_NORMAL);
	struct acpi_sdt_hdr* pSdtHdr = (struct acpi_sdt_hdr*)((uint64_t)pFadt->dsdt);
	virtualMapPage((uint64_t)pSdtHdr, (uint64_t)pSdtHdr, PTE_RW|PTE_NX, 0, 0, PAGE_TYPE_NORMAL);
	if (pSdtHdr->signature!=(uint32_t)'TDSD')
		return -1;
	*ppSdtHdr = pSdtHdr;
	return 0;
}
int aml_init_execution_context(struct aml_execution_context** ppExecutionContext){
	if (!ppExecutionContext)
		return -1;
	struct acpi_sdt_hdr* pSdt = (struct acpi_sdt_hdr*)0x0;
	if (acpi_find_dsdt(&pSdt)!=0)
		return -1;
	struct aml_execution_context* pExecutionContext = (struct aml_execution_context*)kmalloc(sizeof(struct aml_execution_context));
	if (!pExecutionContext)
		return -1;
	uint64_t amlSize = pSdt->len-sizeof(struct acpi_sdt_hdr);
	uint64_t amlPages = align_up(amlSize, PAGE_SIZE)/PAGE_SIZE;
	printf(L"aml size: %dkb\r\n", amlSize/MEM_KB);
	unsigned char* pAml = (unsigned char*)(pSdt+1);
	uint64_t stackSize = 8192;
	unsigned char* pStackBase = (unsigned char*)kmalloc(stackSize);
	if (!pStackBase){
		printf(L"failed to allocate memory for stack\r\n");
		return -1;
	}
	unsigned char* pStack = pStackBase+stackSize-8;
	for (uint64_t ip = 0;ip<amlSize;ip++){
		
	}
	pExecutionContext->ip = (unsigned char*)0x0;
	pExecutionContext->stackSize = stackSize;
	pExecutionContext->sp = pStack;
	pExecutionContext->sb = pExecutionContext->sp;
	*ppExecutionContext = pExecutionContext;
	return 0;
}
int aml_push_stack(struct aml_execution_context* pExecutionContext, uint64_t size){
	if (!pExecutionContext)
		return -1;
	pExecutionContext->sp-=size;
	return 0;
}
int aml_pop_stack(struct aml_execution_context* pExecutionContext, uint64_t size){
	pExecutionContext->sp+=size;
	return 0;
}
int aml_push_frame(struct aml_execution_context* pExecutionContext){
	if (!pExecutionContext)
		return -1;
	pExecutionContext->sb = pExecutionContext->sp;
	return 0;
}
int aml_pop_frame(struct aml_execution_context* pExecutionContext){
	if (!pExecutionContext)
		return -1;
	pExecutionContext->sp = pExecutionContext->sb;
	return 0;
}
int aml_run_instruction(struct aml_execution_context* pExecutionContext){
	if (!pExecutionContext)
		return -1;
	pExecutionContext->ip++;
	return 0;	
}
int shutdown(void){
	printf(L"shutting down...\r\n");
	unsigned char* pAml = (unsigned char*)0x0;
	uint64_t amlSize = 0;
	return 0;
}
