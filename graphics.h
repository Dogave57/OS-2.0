#ifndef _GRAPHICS
#define _GRAPHICS
#include "bootloader.h"
struct vec2{
	unsigned char x, y;
};
struct vec3{
	unsigned char x, y, z;
};
int write_pixel_coord(struct vec2 coord, struct vec3 color);
int write_pixel(unsigned int pixel, struct vec3 color);
#endif
