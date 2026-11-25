#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "cpu/mutex.h"
#include "stdlib/stdlib.h"
#include "subsystem/audio.h"
static struct audio_subsystem_info subsystemInfo;
int audio_subsystem_init(void){
	struct subsystem_desc* pDriverSubsystemDesc = (struct subsystem_desc*)0x00;
	if (subsystem_init(&pDriverSubsystemDesc, AUDIO_SUBSYSTEM_MAX_DRIVER_COUNT)!=0){
		printf("failed to initialize audio driver subsystem descriptor\r\n");
		return -1;
	}
	subsystemInfo.pDriverSubsystemDesc = pDriverSubsystemDesc;
	return 0x00;
}
int audio_subsystem_deinit(void){
	if (subsystem_deinit(subsystemInfo.pDriverSubsystemDesc)!=0){
		printf("failed to deinitialize audio driver subsystem descriptor\r\n");
		return -1;
	}
	return 0x00;
}
int audio_subsystem_driver_get_desc(uint64_t driverId, struct audio_subsystem_driver_desc** ppDriverDesc){
	if (!driverId||!ppDriverDesc)
		return -1;
	struct audio_subsystem_driver_desc* pDriverDesc = (struct audio_subsystem_driver_desc*)0x00;
	if (subsystem_get_entry(subsystemInfo.pDriverSubsystemDesc, driverId, (uint64_t*)&pDriverDesc)!=0){
		printf("failed to get audio driver subsystem driver descriptor entry\r\n");
		return -1;
	}
	if (!pDriverDesc){
		printf("invalid audio driver subsystem driver descriptor\r\n");
		return -1;
	}
	*ppDriverDesc = pDriverDesc;
	return 0x00;
}
int audio_subsystem_driver_register(struct audio_subsystem_driver_vtable vtable, uint64_t* pDriverId){
	if (!pDriverId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct audio_subsystem_driver_desc* pDriverDesc = (struct audio_subsystem_driver_desc*)0x00;
	if (heap_alloc(0x01, (unsigned char**)&pDriverDesc, sizeof(struct audio_subsystem_driver_desc))!=0){
		printf("failed to allocate heap volatile memory audio driver descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	memset((void*)pDriverDesc, 0x00, sizeof(struct audio_subsystem_driver_desc));
	pDriverDesc->vtable = vtable;
	uint64_t driverId = 0x00;
	if (subsystem_alloc_entry(subsystemInfo.pDriverSubsystemDesc, (unsigned char*)pDriverDesc, &driverId)!=0){
		printf("failed to allocate audio driver subsystem QWORD entry\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	pDriverDesc->driverId = driverId;
	*pDriverId = driverId;
	mutex_unlock(&mutex);
	return 0x00;	
}
int audio_subsystem_driver_unregister(uint64_t driverId){
	if (!driverId)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct audio_subsystem_driver_desc* pDriverDesc = (struct audio_subsystem_driver_desc*)0x00;
	if (audio_subsystem_driver_get_desc(driverId, &pDriverDesc)!=0){
		printf("failed to get audio driver subsystem entry descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	if (subsystem_free_entry(subsystemInfo.pDriverSubsystemDesc, driverId)!=0){
		printf("failed to free audio driver subsystem entry descriptor\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	heap_free(0x01, (unsigned char*)pDriverDesc, sizeof(struct audio_subsystem_driver_desc));
	mutex_unlock(&mutex);
	return 0x00;
}
