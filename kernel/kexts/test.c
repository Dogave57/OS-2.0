#include "kernel_api.h"
#include "stdlib/stdlib.h"
#include "drivers/timer.h"
#include "drivers/graphics.h"
int kext_entry(uint64_t pid){
	uint64_t time_ms = get_time_ms();
	printf("pid: %d\r\n", pid);
	while (1){
		uint64_t elapsed_ms = get_time_ms()-time_ms;
		if (!elapsed_ms)
			elapsed_ms = 1;
		printf("program been running for %ds\r\n", elapsed_ms/1000);
		sleep(1000);
	}
	return 0;
}
