#include "bootloader.h"
#include "stdlib/stdlib.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "drivers/filesystem.h"
#include "drivers/serial.h"
#include "drivers/gpu/framebuffer.h"
unsigned int char_position = 0;
struct uvec3_8 text_fg = {255,255,255};
struct uvec3_8 text_bg = {0,0,0};
int write_pixel_coord(struct uvec2 coord, struct uvec3_8 color){
	unsigned int pixel = (coord.y*pbootargs->graphicsInfo.height)+coord.x;
	return write_pixel(pixel, color);
}
int write_pixel(unsigned int pixel, struct uvec3_8 color){
	if (!pbootargs){
		return -1;
	}
	if (!pbootargs->graphicsInfo.virtualFrameBuffer){
		return -1;
	}
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct uvec4_8 flip_color = {color.z, color.y, color.x, 0};
	struct uvec4_8* pPixel = pbootargs->graphicsInfo.virtualFrameBuffer+pixel;
	*pPixel = flip_color;
	mutex_unlock(&mutex);
	return 0;	
}
KAPI int clear(void){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	char_position = 0;
	for (unsigned int i = 0;i<pbootargs->graphicsInfo.width*pbootargs->graphicsInfo.height;i++){
		write_pixel(i, text_bg);
	}
	mutex_unlock(&mutex);
	return 0;
}
int writechar(unsigned int position, unsigned char ch){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (ch>255)
		ch = L' ';
	unsigned int font_offset = ((8*16)/8)*ch;
	unsigned int position_x = position%pbootargs->graphicsInfo.width;
	unsigned int position_y = position/pbootargs->graphicsInfo.width;
	position_y*=16;
	for (unsigned int x = 0;x<8;x++){
		for (unsigned int y = 0;y<16;y++){
			unsigned int pixel = (((position_y+y)*pbootargs->graphicsInfo.width))+position_x+x;
			unsigned int font_byte = ((y*8)+x)/8;
			unsigned int font_bit = 8-(((y*8)+x)%8);
			unsigned char* pfontbyte = (unsigned char*)(pbootargs->graphicsInfo.fontData+font_offset+font_byte);
			if ((*pfontbyte)&(1<<font_bit))
				write_pixel(pixel, text_fg);
			else
				write_pixel(pixel, text_bg);
		}
	}	
	serial_putchar(SERIAL_DEBUG_PORT, (unsigned char)ch);
	mutex_unlock(&mutex);
	return 0;
}
KAPI int putchar(unsigned char ch){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (!pbootargs->graphicsInfo.font_initialized){
		CHAR16 str[2] = {0};
		str[0] = ch;
		conout->OutputString(conout, str);
		mutex_unlock(&mutex);
		return 0;
	}
	if (char_position>=pbootargs->graphicsInfo.width*(pbootargs->graphicsInfo.height/16)){
		clear();
	}
	switch (ch){
		case '\n':
		char_position+=pbootargs->graphicsInfo.width;
		char_position-=(char_position%pbootargs->graphicsInfo.width);
		serial_putchar(SERIAL_DEBUG_PORT, '\n');
		mutex_unlock(&mutex);
		return 0;
		case '\r':
		char_position-=(char_position%pbootargs->graphicsInfo.width);
		serial_putchar(SERIAL_DEBUG_PORT, '\r');
		mutex_unlock(&mutex);
		return 0;
		default:
		break;
		case '\b':
		if (char_position<8)
			break;
		char_position-=8;
		writechar(char_position, L' ');
		serial_putchar(SERIAL_DEBUG_PORT, '\b');
		mutex_unlock(&mutex);
		return 0;
	}
	writechar(char_position, ch);	
	char_position+=8;
	mutex_unlock(&mutex);
	return 0;
}
KAPI int putlchar(uint16_t ch){
	if (putchar((unsigned char)ch)!=0)
		return -1;
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
KAPI int print(unsigned char* string){
	if (!string)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);	
	for (unsigned int i = 0;string[i];i++){
		putchar(string[i]);
	}
	mutex_unlock(&mutex);
	return 0;
}
KAPI int lprint(uint16_t* lstring){
	if (!lstring)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	for (unsigned int i = 0;lstring[i];i++){
		putlchar(lstring[i]);
	}
	mutex_unlock(&mutex);
	return 0;
}
int init_fonts(void){
	pbootargs->graphicsInfo.fontDataSize = sizeof(mainfont_data);
	pbootargs->graphicsInfo.fontData = (unsigned char*)mainfont_data;
	pbootargs->graphicsInfo.font_initialized = 1;
	return 0;
}
KAPI int set_text_color(struct uvec3_8 fg, struct uvec3_8 bg){
	text_fg = fg;
	text_bg = bg;
	return 0;
}
KAPI int get_text_color(struct uvec3_8* pFg, struct uvec3_8* pBg){
	if (pFg)
		*pFg = text_fg;
	if (pBg)
		*pBg = text_bg;
	return 0;
}
