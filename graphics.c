#include "bootloader.h"
#include "graphics.h"
int write_pixel_coord(struct vec2 coord, struct vec3 color){
	unsigned int pixel = (coord.y*pbootargs->graphicsInfo.height)+coord.x;
	return write_pixel(pixel, color);
}
int write_pixel(unsigned int pixel, struct vec3 color){
	struct vec3* ppixel = (struct vec3*)pbootargs->graphicsInfo.physicalFrameBuffer;
	ppixel+=pixel;
	*ppixel = color;
	return 0;
}
