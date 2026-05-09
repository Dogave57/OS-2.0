#include "bootloader.h"
#include "stdlib/stdlib.h"
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "subsystem/gpu.h"
#include "mem/vmm.h"
#include "drivers/filesystem.h"
#include "drivers/serial.h"
#include "subsystem/text.h"
unsigned int char_position = 0;
struct uvec4_8 text_fg = {255,255,255,255};
struct uvec4_8 text_bg = {16,16,16,255};
static struct text_subsystem_info textSubsystemInfo = {0};
int text_subsystem_soft_init(void){
	pbootargs->graphicsInfo.fontDataSize = sizeof(mainfont_data);
	pbootargs->graphicsInfo.fontData = (unsigned char*)mainfont_data;
	pbootargs->graphicsInfo.font_initialized = 1;
	return 0;
}
int text_subsystem_init(void){
	if (gpu_monitor_exists(1)!=0){
		printf("no GPU host controller monitor for text subsystem\r\n");
		return -1;
	}
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(1, &pMonitorDesc)!=0){
		printf("failed to get GPU host controller monitor descriptor\r\n");
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		printf("failed to get GPU host controller driver descriptor\r\n");
		return -1;
	}
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(pMonitorDesc->gpuId, &pGpuDesc)!=0){
		printf("failed to get GPU host controller descriptor\r\n");
		return -1;
	}
	uint64_t commandContextId = 0x00;
	uint64_t commandBufferSize = PAGE_SIZE*4;
	if (gpu_cmd_context_init(pMonitorDesc->gpuId, commandBufferSize, &commandContextId)!=0){
		printf("failed to initialize GPU host controller command context\r\n");
		return -1;
	}
	textSubsystemInfo.commandContextId = commandContextId;
	textSubsystemInfo.commandBufferSize = commandBufferSize;
	textSubsystemInfo.pMonitorDesc = pMonitorDesc;
	textSubsystemInfo.pDriverDesc = pDriverDesc;
	textSubsystemInfo.pGpuDesc = pGpuDesc;
	return 0;
}
int write_pixel(struct uvec2 position, struct uvec4_8 color){
	if (!pbootargs){
		return -1;
	}
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	if (gpu_monitor_exists(1)!=0){
		struct uvec4_8 flip_color = {color.z, color.y, color.x, color.w};
		uint64_t pixelOffset = (position.y*pbootargs->graphicsInfo.width)+position.x;
		struct uvec4_8* pPixel = pbootargs->graphicsInfo.virtualFrameBuffer+pixelOffset;
		*pPixel = flip_color;
		__asm__ volatile("sfence" ::: "memory");
		mutex_unlock(&mutex);
		return 0;
	}
	if (gpu_write_pixel(1, position, color)!=0){
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
	if (gpu_monitor_exists(1)!=0){
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
	if (gpu_monitor_get_info(1, &monitorInfo)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_monitor_desc* pMonitorDesc = (struct gpu_monitor_desc*)0x0;
	if (gpu_monitor_get_desc(1, &pMonitorDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_desc* pGpuDesc = (struct gpu_desc*)0x0;
	if (gpu_get_desc(pMonitorDesc->gpuId, &pGpuDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	struct gpu_driver_desc* pDriverDesc = (struct gpu_driver_desc*)0x0;
	if (gpu_driver_get_desc(pMonitorDesc->driverId, &pDriverDesc)!=0){
		mutex_unlock(&mutex);
		return -1;
	}
	if (pGpuDesc->gpuInfo.features.acceleration&&pMonitorDesc->monitorInfo.framebufferContextId){
		gpu_cmd_context_reset(pGpuDesc->gpuId, textSubsystemInfo.commandContextId);
		struct gpu_clear_cmd_info clearCmdInfo = {0};
		memset((void*)&clearCmdInfo, 0, sizeof(struct gpu_clear_cmd_info));
		clearCmdInfo.header.commandType = GPU_CMD_TYPE_CLEAR;
		clearCmdInfo.color.x = ((float)text_bg.x)/255.0f;
		clearCmdInfo.color.y = ((float)text_bg.y)/255.0f;
		clearCmdInfo.color.z = ((float)text_bg.z)/255.0f;
		clearCmdInfo.color.w = ((float)text_bg.w)/255.0f;
		clearCmdInfo.depth = 1.0f;
		clearCmdInfo.buffers = (1<<0)|(1<<2);
		gpu_cmd_context_push_cmd(pGpuDesc->gpuId, textSubsystemInfo.commandContextId, (struct gpu_cmd_info_header*)&clearCmdInfo);
		if (gpu_cmd_context_submit(pGpuDesc->gpuId, pMonitorDesc->monitorInfo.framebufferContextId, textSubsystemInfo.commandContextId)!=0){
			printf("failed to submit GPU host controller command list to GPU host controller via GPU subsystem\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		struct gpu_transfer_from_device_info transferFromDeviceInfo = {0};
		memset((void*)&transferFromDeviceInfo, 0, sizeof(struct gpu_transfer_from_device_info));
		transferFromDeviceInfo.boxRect.x = 0x0;
		transferFromDeviceInfo.boxRect.y = 0x0;
		transferFromDeviceInfo.boxRect.width = pMonitorDesc->monitorInfo.resolution.width;
		transferFromDeviceInfo.boxRect.height = pMonitorDesc->monitorInfo.resolution.height;
		transferFromDeviceInfo.boxRect.depth = 0x01;
		if (gpu_transfer_from_device(pGpuDesc->gpuId, pMonitorDesc->monitorInfo.framebufferResourceId, transferFromDeviceInfo)!=0){
			printf("failed to transfer GPU host controller framebuffer resource data from GPU host controller to graphical text subsystem\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
		mutex_unlock(&mutex);
		return 0;	
	}
	for (uint64_t i = 0;i<monitorInfo.resolution.width*monitorInfo.resolution.height;i++){
		struct uvec2 position = {0};
		position.x = i%monitorInfo.resolution.width;
		position.y = i/monitorInfo.resolution.width;
		if (gpu_write_pixel(1, position, text_bg)!=0){
			mutex_unlock(&mutex);
			return -1;
		}
	}
	struct uvec4 syncRect = {0};
	syncRect.x = 0;
	syncRect.y = 0;
	syncRect.z = monitorInfo.resolution.width;
	syncRect.w = monitorInfo.resolution.height;
	if (gpu_sync(1, syncRect)!=0){
		serial_print(0, "failed to flush framebuffer\r\n");
		mutex_unlock(&mutex);
		return -1;
	}
	mutex_unlock(&mutex);
	return 0;
}
int writechar(unsigned int position, unsigned char ch){
	if (ch>255)
		ch = L' ';
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_resolution resolution = {0};
	resolution.width = pbootargs->graphicsInfo.width;
	resolution.height = pbootargs->graphicsInfo.height;
	if (!gpu_monitor_exists(1)){
		struct gpu_monitor_info monitorInfo = {0};
		if (gpu_monitor_get_info(1, &monitorInfo)!=0){
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
	if (!gpu_monitor_exists(1)){
		struct uvec4 syncRect = {0};
		syncRect.x = position_x;
		syncRect.y = position_y;
		syncRect.z = 8;
		syncRect.w = 16;
		if (gpu_sync(1, syncRect)!=0){
			serial_print(0, "failed to sync framebuffer\r\n");
			mutex_unlock(&mutex);
			return -1;
		}
	}
	serial_putchar(SERIAL_DEBUG_PORT, (unsigned char)ch);
	mutex_unlock(&mutex);
	return 0;
}
KAPI int putchar(unsigned char ch){
	if (!pbootargs->graphicsInfo.font_initialized)
		return -1;
	static struct mutex_t mutex = {0};
	mutex_lock(&mutex);
	struct gpu_monitor_info monitorInfo = {0};
	struct gpu_resolution resolution = {0};
	resolution.width = pbootargs->graphicsInfo.width;
	resolution.height = pbootargs->graphicsInfo.height;
	if (!gpu_monitor_exists(1)){
		struct gpu_monitor_info monitorInfo = {0};
		if (gpu_monitor_get_info(1, &monitorInfo)!=0){
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
	for (unsigned int i = 0;string[i];i++){
		putchar(string[i]);
	}
	return 0;
}
KAPI int lprint(uint16_t* lstring){
	if (!lstring)
		return -1;
	for (unsigned int i = 0;lstring[i];i++){
		putlchar(lstring[i]);
	}
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
