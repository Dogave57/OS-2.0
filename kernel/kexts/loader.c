#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/filesystem.h"
#include "align.h"
#include "elf.h"
#include "kexts/loader.h"
int kext_load(uint64_t mount_id, unsigned char* filename, uint64_t* pPid){
	if (!filename||!pPid)
		return -1;
	struct elf_handle* pHandle = (struct elf_handle*)0x0;
	if (elf_load(mount_id, filename, &pHandle)!=0)
		return -1;
	elf_unload(pHandle);
	return 0;
	int status = 0;
	if (elf_execute(pHandle, &status)!=0){
		printf("failed to execute ELF binaryr\r\n");
		elf_unload(pHandle);
		return -1;
	}
	printf("program exited with status 0x0x%x\r\n", status);
	elf_unload(pHandle);
	return 0;
}
