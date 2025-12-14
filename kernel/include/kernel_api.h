#ifndef _KERNEL_API
#define _KERNEL_API
#ifdef _BUILDING_KERNEL
#define KAPI __attribute__((visibility("default")))
#else
#define KAPI 
#endif
#endif
