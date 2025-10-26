#include "bootloader.h"
#include "stdlib.h"
#include "filesystem.h"
#include "graphics.h"
unsigned int char_position = 0;
struct vec3 text_fg = {200,0,0};
struct vec3 text_bg = {0,0,0};
int write_pixel_coord(struct vec2 coord, struct vec3 color){
	unsigned int pixel = (coord.y*pbootargs->graphicsInfo.height)+coord.x;
	return write_pixel(pixel, color);
}
int write_pixel(unsigned int pixel, struct vec3 color){
	if (!pbootargs)
		return -1;
	if (!pbootargs->graphicsInfo.physicalFrameBuffer)
		return -1;
	struct vec4 flip_color = {color.z, color.y, color.x, 0};
	struct vec4* pPixel = pbootargs->graphicsInfo.physicalFrameBuffer+pixel;
	*pPixel = flip_color;
	return 0;	
}
int clear(void){
	char_position = 0;
	for (unsigned int i = 0;i<pbootargs->graphicsInfo.width*pbootargs->graphicsInfo.height;i++){
		write_pixel(i, text_bg);
	}
	return 0;
}
int putchar(CHAR16 ch){
	if (!pbootargs->graphicsInfo.font_initialized){
		CHAR16 str[2] = {0};
		str[0] = ch;
		conout->OutputString(conout, str);
		return 0;
	}
	switch (ch){
		case '\n':
		char_position+=(pbootargs->graphicsInfo.width);
		return 0;
		case '\r':
		char_position+=pbootargs->graphicsInfo.width-(char_position%(pbootargs->graphicsInfo.width/8));
		return 0;
		default:
			
		break;
	}
	struct vec3 color = {200,0,0};
	unsigned int font_offset = ((8*16)/8)*ch;
	for (unsigned int x = 0;x<8;x++){
		for (unsigned int y = 0;y<16;y++){
			unsigned int char_y = 0;
			unsigned int char_x = 0;
			if (char_position){
				char_x = (char_position*8)%pbootargs->graphicsInfo.width;
				char_y = (char_position*8)/pbootargs->graphicsInfo.width;
			}
			unsigned int pixel = ((char_y+y)*pbootargs->graphicsInfo.width)+char_x+x;
			unsigned int font_byte = ((y*8)+x)/8;
			unsigned int font_bit = 8-(((y*8)+x)%8);
			unsigned char* pfontbyte = (unsigned char*)(pbootargs->graphicsInfo.fontData+font_offset+font_byte);
			if ((*pfontbyte)&(1<<font_bit))
				write_pixel(pixel, text_fg);
			else
				write_pixel(pixel, text_bg);
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
	for (unsigned int i = 0;string[i];i++){
		putchar(string[i]);
	}
	return 0;
}
int init_fonts(void){
	EFI_FILE_PROTOCOL* rootVolume = (EFI_FILE_PROTOCOL*)0x0;
	EFI_STATUS status = filesystemProtocol->OpenVolume(filesystemProtocol, &rootVolume);
	if (status!=EFI_SUCCESS){
		printf(L"failed to open root volume when initializing fonts %x\r\n", status);
		return -1;
	}	
	EFI_FILE_PROTOCOL* fontsDir = (EFI_FILE_PROTOCOL*)0x0;
	status = rootVolume->Open(rootVolume, &fontsDir, L"FONTS", EFI_FILE_MODE_READ, 0);
	if (status!=EFI_SUCCESS){
		printf(L"failed to open fonts directory when initiializing fonts %x\r\n", status);
		return -1;
	}
	unsigned char* fontData = (unsigned char*)0x0;
	unsigned int fontDataSize = 0;
	if (efi_readfile(fontsDir, L"font.bin", (void**)&fontData, &fontDataSize)!=0){
		printf(L"failed to read fonts file\r\n");
		fontsDir->Close(fontsDir);
		rootVolume->Close(rootVolume);
		return -1;
	}
	fontsDir->Close(fontsDir);
	rootVolume->Close(rootVolume);
	pbootargs->graphicsInfo.fontData = fontData;
	pbootargs->graphicsInfo.fontDataSize = fontDataSize;
	pbootargs->graphicsInfo.font_initialized = 1;
	return 0;
}
int set_text_color(struct vec3 fg, struct vec3 bg){
	text_fg = fg;
	text_bg = bg;
	return 0;
}
int get_text_color(struct vec3* pFg, struct vec3* pBg){
	if (pFg)
		*pFg = text_fg;
	if (pBg)
		*pBg = text_bg;
	return 0;
}
