#include "stdlib/stdlib.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "subsystem/subsystem.h"
#include "subsystem/filesystem.h"
struct subsystem_desc* pMountSubsystem = (struct subsystem_desc*)0x0;
struct subsystem_desc* pFileHandleSubsystem = (struct subsystem_desc*)0x0;
struct driver_list_desc driverListDesc = {0};
int fs_subsystem_init(void){
	if (subsystem_init(&pMountSubsystem, 1024)!=0)
		return -1;
	if (subsystem_init(&pFileHandleSubsystem, 4096)!=0)
		return -1;
	return 0;
}
int fs_mount(uint64_t drive_id, uint64_t partition_id, uint64_t* pId){
	if (!pId)
		return -1;
	uint64_t id = 0;
	struct fs_mount* pMount = (struct fs_mount*)kmalloc(sizeof(struct fs_mount));
	if (subsystem_alloc_entry(pMountSubsystem, (unsigned char*)pMount, &id)!=0){
		kfree((void*)pMount);
		return -1;
	}
	pMount->drive_id = drive_id;
	pMount->partition_id = partition_id;
	struct fs_driver_desc* pCurrentDriver = driverListDesc.pFirstDriver;
	while (pCurrentDriver){
		struct fs_driver_vtable vtable = pCurrentDriver->vtable;		
		if (!vtable.mount){
			pCurrentDriver = pCurrentDriver->pFlink;
			continue;		
		}
		if (vtable.mount(drive_id, partition_id, pMount)!=0){
			pCurrentDriver = pCurrentDriver->pFlink;
			continue;
		}
		pMount->pDriver = pCurrentDriver;
		*pId = id;
		return 0;
	}
	kfree((void*)pMount);
	if (subsystem_free_entry(pMountSubsystem, id)!=0)
		return -1;
	return -1;
}
int fs_unmount(uint64_t id){
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	if (subsystem_get_entry(pMountSubsystem, id, (uint64_t*)&pMount)!=0)
		return -1;
	if (!pMount->pDriver->vtable.unmount)
		return -1;
	pMount->pDriver->vtable.unmount(pMount);
	kfree((void*)pMount);
	if (subsystem_free_entry(pMountSubsystem, id)!=0)
		return -1;
	return 0;
}
int fs_driver_register(struct fs_driver_vtable vtable, struct fs_driver_desc** ppDriverDesc){
	if (!ppDriverDesc)
		return -1;
	struct fs_driver_desc* pDriverDesc = (struct fs_driver_desc*)kmalloc(sizeof(struct fs_driver_desc));
	if (!pDriverDesc)
		return -1;
	memset((void*)pDriverDesc, 0, sizeof(struct fs_driver_desc));
	pDriverDesc->vtable = vtable;
	if (!driverListDesc.pFirstDriver){
		driverListDesc.pFirstDriver = pDriverDesc;
	}
	if (driverListDesc.pLastDriver){
		driverListDesc.pLastDriver->pFlink = pDriverDesc;
		pDriverDesc->pBlink = driverListDesc.pLastDriver;
	}
	driverListDesc.pLastDriver = pDriverDesc;
	return 0;
}
int fs_driver_unregister(struct fs_driver_desc* pDriverDesc){
	if (!pDriverDesc)
		return -1;
	if (driverListDesc.pLastDriver==pDriverDesc){
		driverListDesc.pLastDriver = pDriverDesc->pBlink;
	}
	if (driverListDesc.pFirstDriver==pDriverDesc){
		driverListDesc.pFirstDriver = pDriverDesc->pFlink;
	}
	if (pDriverDesc->pFlink)
		pDriverDesc->pFlink->pBlink = pDriverDesc->pBlink;
	if (pDriverDesc->pBlink)
		pDriverDesc->pBlink->pFlink = pDriverDesc->pFlink;
	return 0;
}
