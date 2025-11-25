#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/drive.h"
#include "drivers/filesystem/fat32.h"
int fat32_openfile(uint64_t drive_id, uint64_t partition_id, unsigned char* filename, uint64_t* pId){
	if (!pId)
		return -1;

	return 0;
}
