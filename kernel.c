#include "bootloader.h"
#include "graphics.h"
struct bootloader_args* pbootargs = (struct bootloader_args*)0x0;
int kmain(struct bootloader_args* blargs){
	if (!blargs)
		return -1;
	if (!blargs->systable)
		return -1;
	pbootargs = blargs;
	EFI_SYSTEM_TABLE* systab = blargs->systable;
	EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* conout = systab->ConOut;
	struct bootloader_graphicsInfo graphicsInfo = blargs->graphicsInfo;
	unsigned int res = graphicsInfo.width*graphicsInfo.height;
	struct vec3 color = {255,0,0};
	for (unsigned int i = 0;i<res;i++){
		write_pixel(i, color);
	}
	while (1){};
	return 0;	
}
