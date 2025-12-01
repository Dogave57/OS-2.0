#ifndef _FILESYSTEM_SUBSYSTEM
#define _FILESYSTEM_SUBSYSTEM
struct fs_mount;
typedef int(*fsOpenFunc)(struct fs_mount* pMount, uint16_t* filename, void* pHandle);
typedef int(*fsCloseFunc)(void* pHandle);
typedef int(*fsReadFunc)(struct fs_mount* pMount, void* pHandle, unsigned char* pBuffer, uint64_t bufferSize);
typedef int(*fsWriteFunc)(struct fs_mount* pMount, void* pHandle, unsigned char* pBuffer, uint64_t bufferSize);
typedef int(*fsCreateFunc)(struct fs_mount* pMount, uint16_t* filename);
typedef int(*fsDeleteFunc)(void* pHandle);
typedef int(*fsVerifyFunc)(struct fs_mount* pMount);
typedef int(*fsMountFunc)(uint64_t drive_id, uint64_t partition_id, struct fs_mount* pMount);
typedef int(*fsUnmountFunc)(struct fs_mount* pMount);
typedef int(*fsOpenDirFunc)(struct fs_mount* pMount, uint16_t* filename, void* pDirHandle);
typedef int(*fsCloseDirFunc)(void* pDirHandle);
struct fs_driver_vtable{
	fsOpenFunc open;
	fsCloseFunc close;
	fsReadFunc read;
	fsWriteFunc write;
	fsCreateFunc create;
	fsDeleteFunc delete;
	fsVerifyFunc verify;
	fsMountFunc mount;
	fsUnmountFunc unmount;
	fsOpenDirFunc opendir;
	fsCloseDirFunc closedir;
};
struct fs_driver_desc{
	struct fs_driver_vtable vtable;
	struct fs_driver_desc* pFlink;
	struct fs_driver_desc* pBlink;
};
struct driver_list_desc{
	struct fs_driver_desc* pFirstDriver;
	struct fs_driver_desc* pLastDriver;
};
struct fs_location{
	uint64_t drive_id;
	uint64_t partition_id;
};
struct fs_mount{
	struct fs_location fsLocation;
	uint64_t drive_id;
	uint64_t partition_id;
	struct fs_driver_desc* pDriver;
	void* pMountData;
};
struct fs_handle{
	void* pHandleData;
};
struct fs_driver_desc{
	struct fs_driver_vtable vtable;
	struct fs_driver_desc* pFlink;
	struct fs_driver_desc* pBlink;
};
int fs_subsystem_init(void);
int fs_mount(uint64_t drive_id, uint64_t partition_id, uint64_t* pId);
int fs_unmount(uint64_t id);
int fs_driver_detect(struct fs_mount* pMount, struct fs_driver_desc** ppDriverDesc);
int fs_driver_register(struct fs_driver_vtable vtable, struct fs_driver_desc** ppDriverDesc);
int fs_driver_unregister(struct fs_driver_desc* pDriverDesc);
#endif
