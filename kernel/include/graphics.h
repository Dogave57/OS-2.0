#ifndef _GRAPHICS
#define _GRAPHICS
#include "bootloader.h"
struct vec2{
	unsigned char x, y;
};
struct vec3{
	unsigned char x, y, z;
};
struct vec4{
	unsigned char x, y, z, w;
};
int write_pixel_coord(struct vec2 coord, struct vec4 color);
int write_pixel(unsigned int pixel, struct vec4 color);
int putchar(CHAR16 ch);
int puthex(unsigned char hex, unsigned char isUpper);
int print(CHAR16* string);
#endif
