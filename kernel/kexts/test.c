#include "kernel_api.h"
#include "stdlib/stdlib.h"
#include "drivers/graphics.h"
int kext_entry(void){
	print("test\r\n");
	return 0;
}
