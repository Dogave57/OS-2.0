#include "bootloader.h"
#include "stdlib/stdlib.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "subsystem/gpu.h"
#include "drivers/filesystem.h"
#include "drivers/serial.h"
#include "drivers/gpu/framebuffer.h"
unsigned int char_position = 0;
struct uvec4_8 text_fg = {255,255,255,0};
struct uvec4_8 text_bg = {0,0,0,0};
int write_pixel(struct uvec2 position, struct uvec4_8 color){
	if (!pbootargs){
		return -1;
	}
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (gpu_monitor_exists(0)!=0){
		struct uvec4_8 flip_color = {color.z, color.y, color.x, color.w};
		uint64_t pixelOffset = (position.y*pbootargs->graphicsInfo.width)+position.x;
		struct uvec4_8* pPixel = pbootargs->graphicsInfo.virtualFrameBuffer+pixelOffset;
		*pPixel = flip_color;
		mutex_unlock(&mutex);
		return 0;
	}
	if (gpu_write_pixel(0, position, color)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;	
}
KAPI int clear(void){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	char_position = 0;
	if (gpu_monitor_exists(0)!=0){
		for (unsigned int i = 0;i<pbootargs->graphicsInfo.width*pbootargs->graphicsInfo.height;i++){
			struct uvec2 position = {0};
			position.x = i%pbootargs->graphicsInfo.width;
			position.y = i/pbootargs->graphicsInfo.width;
			write_pixel(position, text_bg);
		}
		mutex_unlock(&mutex);
		return 0;
	}
	struct gpu_monitor_info monitorInfo = {0};
	if (gpu_monitor_get_info(0, &monitorInfo)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	for (uint64_t i = 0;i<monitorInfo.resolution.width*monitorInfo.resolution.height;i++){
		struct uvec2 position = {0};
		position.x = i%monitorInfo.resolution.width;
		position.y = i/monitorInfo.resolution.width;
		if (gpu_write_pixel(0, position, text_bg)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
	}
	serial_print(0, "test\r\n");
	struct uvec4 syncRect = {0};
	syncRect.x = 0;
	syncRect.y = 0;
	syncRect.z = monitorInfo.resolution.width;
	syncRect.w = monitorInfo.resolution.height;
	serial_print(0, "syncing framebuffer\r\n");
	if (gpu_sync(0, syncRect)!=0){
		serial_print(0, "failed to flush framebuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	serial_print(0, "done syncing framebuffer\r\n");
	mutex_unlock(&mutex);
	return 0;
}
int writechar(unsigned int position, unsigned char ch){
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (ch>255)
		ch = L' ';
	struct gpu_resolution resolution = {0};
	resolution.width = pbootargs->graphicsInfo.width;
	resolution.height = pbootargs->graphicsInfo.height;
	if (!gpu_monitor_exists(0)){
		struct gpu_monitor_info monitorInfo = {0};
		if (gpu_monitor_get_info(0, &monitorInfo)!=0){
			serial_print(0, "failed to get new monitor info\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		resolution = monitorInfo.resolution;
	}
	unsigned int font_offset = ((8*16)/8)*ch;
	unsigned int position_x = position%resolution.width;
	unsigned int position_y = position/resolution.width;
	position_y*=16;
//	serial_print(0, "writing raw pixel data to framebuffer\r\n");
	for (unsigned int x = 0;x<8;x++){
		for (unsigned int y = 0;y<16;y++){
			unsigned int pixelX = position_x+x;
			unsigned int pixelY = position_y+y;
			unsigned int pixel = (pixelY*resolution.width)+pixelX;
			unsigned int font_byte = ((y*8)+x)/8;
			unsigned int font_bit = 8-(((y*8)+x)%8);
			unsigned char* pfontbyte = (unsigned char*)(pbootargs->graphicsInfo.fontData+font_offset+font_byte);
			struct uvec2 position = {0};
			position.x = pixelX;
			position.y = pixelY;
			if ((*pfontbyte)&(1<<font_bit))
				write_pixel(position, text_fg);
			else
				write_pixel(position, text_bg);
		}
	}	
//	serial_print(0, "done writing raw pixel data to framebuffer\r\n");
	if (!gpu_monitor_exists(0)){
		struct uvec4 syncRect = {0};
		syncRect.x = position_x;
		syncRect.y = position_y;
		syncRect.z = 8;
		syncRect.w = 16;
		serial_print(0, "syncing framebuffer\r\n");
		if (gpu_sync(0, syncRect)!=0){
			serial_print(0, "failed to sync framebuffer\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		serial_print(0, "done syncing framebuffer\r\n");
	}
//	serial_print(0, "done\r\n");
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
	struct gpu_monitor_info monitorInfo = {0};
	struct gpu_resolution resolution = {0};
	resolution.width = pbootargs->graphicsInfo.width;
	resolution.height = pbootargs->graphicsInfo.height;
	if (!gpu_monitor_exists(0)){
		struct gpu_monitor_info monitorInfo = {0};
		if (gpu_monitor_get_info(0, &monitorInfo)!=0){
			printf("failed to get monitor info\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		resolution = monitorInfo.resolution;
	}
	if (char_position>=resolution.width*(resolution.height/16)){
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
KAPI int set_text_color(struct uvec4_8 fg, struct uvec4_8 bg){
	text_fg = fg;
	text_bg = bg;
	return 0;
}
KAPI int get_text_color(struct uvec4_8* pFg, struct uvec4_8* pBg){
	if (pFg)
		*pFg = text_fg;
	if (pBg)
		*pBg = text_bg;
	return 0;
}
