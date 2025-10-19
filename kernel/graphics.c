#include "bootloader.h"
#include "graphics.h"
unsigned int char_position = 0;
int write_pixel_coord(struct vec2 coord, struct vec4 color){
	unsigned int pixel = (coord.y*pbootargs->graphicsInfo.height)+coord.x;
	return write_pixel(pixel, color);
}
int write_pixel(unsigned int pixel, struct vec4 color){
	if (!pbootargs)
		return -1;
	if (!pbootargs->graphicsInfo.physicalFrameBuffer)
		return -1;
	pbootargs->graphicsInfo.physicalFrameBuffer[pixel] = color;
	return 0;
}
int putchar(CHAR16 ch){
	if (!pbootargs->graphicsInfo.font_initialized){
		CHAR16 str[2] = {0};
		str[0] = ch;
		conout->OutputString(conout, str);
		return 0;
	}	
	struct vec4 color = {200,0,0,0};
	for (unsigned int x = 0;x<16;x++){
		for (unsigned int y = 0;y<16;y++){
			unsigned int char_y = (char_position*16)/pbootargs->graphicsInfo.width;
			unsigned int char_x = (char_position*16)%pbootargs->graphicsInfo.width;
			unsigned int pixel = ((char_y+y)*pbootargs->graphicsInfo.width)+char_x+x;
		}
	}	
	char_position++;
	return 0;
}
int puthex(unsigned char hex, unsigned char isUpper){
	if (hex>16)
		return -1;
	if (hex<10){
		putchar(L'0'+hex);
		return 0;
	}
	if (isUpper)
		putchar(L'A'+hex-10);
	else
		putchar(L'a'+hex-10);
	return 0;
}
int print(CHAR16* string){
	if (!string)
		return -1;
	conout->OutputString(conout, string);
	return 0;
}
int init_fonts(void){
	
	return 0;
}
