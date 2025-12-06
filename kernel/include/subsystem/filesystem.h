#ifndef _FILESYSTEM_SUBSYSTEM
#define _FILESYSTEM_SUBSYSTEM
#define FILE_ATTRIBUTE_HIDDEN (1<<0)
#define FILE_ATTRIBUTE_SYSTEM (1<<1)
#define FILE_ATTRIBUTE_DIRECTORY (1<<2)
#define FILE_ATTRIBUTE_READONLY (1<<3)
struct fs_mount;
struct fs_file_info;
typedef int(*fsOpenFunc)(struct fs_mount* pMount, unsigned char* filename, void** pHandle);
typedef int(*fsCloseFunc)(void* pHandle);
typedef int(*fsReadFunc)(struct fs_mount* pMount, void* pHandle, unsigned char* pBuffer, uint64_t bufferSize);
typedef int(*fsWriteFunc)(struct fs_mount* pMount, void* pHandle, unsigned char* pBuffer, uint64_t bufferSize);
typedef int(*fsCreateFunc)(struct fs_mount* pMount, unsigned char* filename, uint64_t fileAttribs);
typedef int(*fsDeleteFunc)(struct fs_mount* pMount, void* pHandle);
typedef int(*fsVerifyFunc)(struct fs_mount* pMount);
typedef int(*fsMountFunc)(uint64_t drive_id, uint64_t partition_id, struct fs_mount* pMount);
typedef int(*fsUnmountFunc)(struct fs_mount* pMount);
typedef int(*fsOpenDirFunc)(struct fs_mount* pMount, unsigned char* filename, void** ppDirHandle);
typedef int(*fsReadDirFunc)(struct fs_mount* pMount, void* pHandle, struct fs_file_info* pFileInfo);
typedef int(*fsRewindDirFunc)(struct fs_mount* pMount, void* pHandle);
typedef int(*fsCloseDirFunc)(void* pDirHandle);
typedef int(*fsGetFileInfoFunc)(struct fs_mount* pMount, void* pHandle, struct fs_file_info* pFileInfo);
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
	fsReadDirFunc readDir;
	fsRewindDirFunc rewindDir;
	fsCloseDirFunc closedir;
	fsGetFileInfoFunc getFileInfo;
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
struct fs_file_info{
	uint64_t fileSize;
	uint64_t fileAttributes;
	unsigned char filename[256];
};
int fs_subsystem_init(void);
int fs_mount(uint64_t drive_id, uint64_t partition_id, uint64_t* pId);
int fs_unmount(uint64_t id);
int fs_verify(uint64_t id);
int fs_open(uint64_t mount_id, unsigned char* filename, uint64_t flags, uint64_t* pFileId);
int fs_close(uint64_t mount_id, uint64_t file_id);
int fs_read(uint64_t mount_id, uint64_t file_id, unsigned char* pFileBuffer, uint64_t fileSize);
int fs_write(uint64_t mount_id, uint64_t file_id, unsigned char* pFileBuffer, uint64_t fileSize);
int fs_getFileInfo(uint64_t mount_id, uint64_t file_id, struct fs_file_info* pFileInfo);
int fs_create(uint64_t mount_id, unsigned char* filename, uint64_t fileAttribs);
int fs_delete(uint64_t mount_id, uint64_t file_id);
int fs_opendir(uint64_t mount_id, unsigned char* filename, uint64_t* pFileId);
int fs_read_dir(uint64_t mount_id, uint64_t file_id, struct fs_file_info* pFileInfo);
int fs_rewind_dir(uint64_t mount_id, uint64_t file_id);
int fs_closedir(uint64_t mount_id, uint64_t file_id);
int fs_handle_register(void* pHandle, uint64_t* pHandleId);
int fs_handle_unregister(uint64_t handleId);
int fs_driver_detect(struct fs_mount* pMount, struct fs_driver_desc** ppDriverDesc);
int fs_driver_register(struct fs_driver_vtable vtable, struct fs_driver_desc** ppDriverDesc);
int fs_driver_unregister(struct fs_driver_desc* pDriverDesc);
#endif
