#ifndef _KERNEL_API
#define _KERNEL_API
#ifdef _BUILDING_KERNEL
#define KAPI __declspec(dllexport)
#else
#define KAPI 
#endif
#endif
