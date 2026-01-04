#ifndef _SUBSYSTEM_GPU
#define _SUBSYSTEM_GPU
struct gpu_driver_vtable{
	
};
struct gpu_driver_desc{
	struct gpu_driver_vtable vtable;
};
int gpu_subsystem_init(void);	
#endif
