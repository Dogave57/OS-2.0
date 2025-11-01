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
int write_pixel_coord(struct vec2 coord, struct vec3 color);
int write_pixel(unsigned int pixel, struct vec3 color);
int clear(void);
int writechar(unsigned int position, CHAR16 ch);
int putchar(CHAR16 ch);
int putchar_ascii(unsigned char ch);
int puthex(unsigned char hex, unsigned char isUpper);
int print(CHAR16* string);
int print_ascii(unsigned char* string);
int init_fonts(void);
int set_text_color(struct vec3 fg, struct vec3 bg);
int get_text_color(struct vec3* pFg, struct vec3* pBg);
#endif
