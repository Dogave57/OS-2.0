#ifndef _SUBSYSTEM_AUDIO
#define _SUBSYSTEM_AUDIO
#define AUDIO_SUBSYSTEM_MAX_DRIVER_COUNT (8192)
struct audio_subsystem_driver_vtable{
	uint64_t reserved0;
};
struct audio_subsystem_driver_desc{
	uint64_t driverId;
	struct audio_subsystem_driver_vtable vtable;
};
struct audio_subsystem_info{
	struct subsystem_desc* pDriverSubsystemDesc;
};
int audio_subsystem_init(void);
int audio_subsystem_deinit(void);
int audio_subsystem_driver_register(struct audio_subsystem_driver_vtable vtable, uint64_t* pDriverId);
int audio_subsystem_driver_unregister(uint64_t driverId);
#endif
