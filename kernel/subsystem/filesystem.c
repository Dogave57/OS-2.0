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
int fs_verify(uint64_t id){
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	if (subsystem_get_entry(pMountSubsystem, id, (uint64_t*)&pMount)!=0)
		return -1;
	if (!pMount->pDriver->vtable.verify)
		return -1;
	if (pMount->pDriver->vtable.verify(pMount)!=0)
		return -1;
	return 0;
}
int fs_open(uint64_t id, uint16_t* filename, uint64_t flags, uint64_t* pFileId){
	if (!pFileId)
		return -1;
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	if (subsystem_get_entry(pMountSubsystem, id, (uint64_t*)&pMount)!=0)
		return -1;
	if (!pMount->pDriver->vtable.open)
		return -1;
	void* pHandle = (void*)0x0;
	if (pMount->pDriver->vtable.open(pMount, filename, &pHandle)!=0)
		return -1;
	uint64_t fileId = 0;
	if (fs_handle_register(pHandle, &fileId)!=0)
		return -1;
	*pFileId = fileId;
	return 0;
}
int fs_close(uint64_t mount_id, uint64_t file_id){
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.close)
		return -1;
	if (pMount->pDriver->vtable.close((void*)pFileHandle)!=0)
		return -1;
	return 0;
}
int fs_read(uint64_t mount_id, uint64_t file_id, unsigned char* pBuffer, uint64_t size){
	if (!pBuffer)
		return -1;
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.read)
		return -1;
	if (pMount->pDriver->vtable.read(pMount, pFileHandle, pBuffer, size)!=0)
		return -1;
	return 0;
}
int fs_write(uint64_t mount_id, uint64_t file_id, unsigned char* pBuffer, uint64_t size){
	if (!pBuffer)
		return -1;
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.write)
		return -1;
	if (pMount->pDriver->vtable.write(pMount, pFileHandle, pBuffer, size)!=0)
		return -1;
	return 0;
}
int fs_getFileInfo(uint64_t mount_id, uint64_t file_id, struct fs_file_info* pFileInfo){
	if (!pFileInfo)
		return -1;
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.getFileInfo)
		return -1;
	struct fs_file_info fileInfo = {0};
	if (pMount->pDriver->vtable.getFileInfo(pMount, pFileHandle, &fileInfo)!=0)
		return -1;
	*pFileInfo = fileInfo;
	return 0;
}
int fs_create(uint64_t mount_id, uint16_t* filename, uint64_t fileAttribs){
	if (!filename)
		return -1;
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (!pMount->pDriver->vtable.create)
		return -1;
	if (pMount->pDriver->vtable.create(pMount, (uint16_t*)filename, fileAttribs)!=0)
		return -1;
	return 0;
}
int fs_delete(uint64_t mount_id, uint64_t file_id){
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.delete)
		return -1;
	if (pMount->pDriver->vtable.delete(pMount, pFileHandle)!=0)
		return -1;
	return 0;
}
int fs_opendir(uint64_t mount_id, uint16_t* filename, uint64_t* pFileId){
	if (!filename||!pFileId)
		return -1;
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (!pMount->pDriver->vtable.opendir)
		return -1;
	void* pFileHandle = (void*)0x0;
	uint64_t fileId = 0;
	if (pMount->pDriver->vtable.opendir(pMount, filename, &pFileHandle)!=0)
		return -1;
	if (fs_handle_register(pFileHandle, &fileId)!=0){
		if (pMount->pDriver->vtable.closedir)
			pMount->pDriver->vtable.closedir(pFileHandle);
		return -1;
	}
	*pFileId = fileId;
	return 0;
}
int fs_read_dir(uint64_t mount_id, uint64_t file_id, struct fs_file_info* pFileInfo){
	if (!pFileInfo)
		return -1;
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.readDir)
		return -1;
	struct fs_file_info fileInfo = {0};
	if (pMount->pDriver->vtable.readDir(pMount, pFileHandle, &fileInfo)!=0)
		return -1;
	*pFileInfo = fileInfo;
	return 0;
}
int fs_rewind_dir(uint64_t mount_id, uint64_t file_id){
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.rewindDir)
		return -1;
	if (pMount->pDriver->vtable.rewindDir(pMount, pFileHandle)!=0)
		return -1;
	return 0;
}
int fs_closedir(uint64_t mount_id, uint64_t file_id){
	struct fs_mount* pMount = (struct fs_mount*)0x0;
	void* pFileHandle = (void*)0x0;
	if (subsystem_get_entry(pMountSubsystem, mount_id, (uint64_t*)&pMount)!=0)
		return -1;
	if (subsystem_get_entry(pFileHandleSubsystem, file_id, (uint64_t*)&pFileHandle)!=0)
		return -1;
	if (!pMount->pDriver->vtable.closedir)
		return -1;
	if (pMount->pDriver->vtable.closedir(pFileHandle)!=0)
		return -1;
	return 0;
}
int fs_handle_register(void* pHandle, uint64_t* pHandleId){
	if (!pHandle||!pHandleId)
		return -1;
	uint64_t handleId = 0;
	if (subsystem_alloc_entry(pFileHandleSubsystem, (unsigned char*)pHandle, &handleId)!=0)
		return -1;
	*pHandleId = handleId;
	return 0;
}
int fs_handle_unregister(uint64_t handleId){
	if (subsystem_free_entry(pFileHandleSubsystem, handleId)!=0)
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
