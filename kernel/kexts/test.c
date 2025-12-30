#include "kernel_api.h"
#include "stdlib/stdlib.h"
#include "cpu/thread.h"
#include "drivers/timer.h"
#include "drivers/graphics.h"
int thread_entry(uint64_t tid, uint64_t arg);
int kext_entry(uint64_t pid){
	uint64_t time_ms = get_time_ms();
	printf("pid: %d\r\n", pid);
	uint64_t tid = 0;
	if (thread_create((uint64_t)thread_entry, 0, &tid, 0)!=0){
		printf("failed to create thread\r\n");
		return -1;
	}
	sleep(5000);
	while (1){
		uint64_t elapsed_ms = get_time_ms()-time_ms;
		if (!elapsed_ms)
			elapsed_ms = 1;
		printf("program been running for %ds\r\n", elapsed_ms/1000);
		sleep(1000);
	}
	return 0;
}
int thread_entry(uint64_t tid, uint64_t arg){
	uint64_t time_ms = get_time_ms();
	while (1){
		uint64_t elapsed_ms = get_time_ms()-time_ms;
		if (!elapsed_ms)
			elapsed_ms = 1;
		printf("thread has been running for %ds\r\n", elapsed_ms/1000);
		sleep(1000);
	}
	return 0;
}
