#include "stdlib/stdlib.h"
#include "cpu/thread.h"
#include "math/vector.h"
#include "drivers/gpu/framebuffer.h"
struct gpu_driver_desc driverDesc = {0};
int gpu_subsystem_init(void){

	return 0;
}
int gpu_driver_register(struct gpu_driver_vtable vtable){
	static unsigned char lock = 0;
	while (lock){
		thread_yield();
	} 
	lock = 1;
	driverDesc.vtable = vtable;
	lock = 0;
	return 0;
}
int gpu_write_pixel(uint64_t index, struct vec4 color){
	
	return 0;
}
