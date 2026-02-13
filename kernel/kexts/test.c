#include "kernel_api.h"
#include "stdlib/stdlib.h"
#include "cpu/thread.h"
#include "drivers/timer.h"
#include "drivers/gpu/framebuffer.h"
int thread_entry(uint64_t tid, uint64_t arg);
int kext_entry(uint64_t pid){
	uint64_t time_us = get_time_us();
	printf("pid: %d\r\n", pid);
	while (1){};
	return 0;
	printf("timer start\r\n");
	sleep(5000);
	printf("timer end\r\n");
	while (1){};
	for (uint64_t i = 0;i<32;i++){
		uint64_t tid = 0;
		if (thread_create((uint64_t)thread_entry, 0, 0, &tid, 0)!=0){
			printf("failed to create thread\r\n");
			return -1;
		}
	}	
	uint64_t last_elapsed_s = 0;
	while (1){
		uint64_t elapsed_us = get_time_us()-time_us;
		uint64_t elapsed_s = elapsed_us/1000000;
		if (elapsed_s==last_elapsed_s)
			continue;
		printf("program has been running for %ds\r\n", elapsed_s);
		last_elapsed_s = elapsed_s;
		sleep(900);
	}
	return 0;
}
int thread_entry(uint64_t tid, uint64_t arg){
	uint64_t time_us = get_time_us();
	uint64_t last_elapsed_s = 0;
	while (1){
		uint64_t elapsed_us = get_time_us()-time_us;
		uint64_t elapsed_s = elapsed_us/1000000;
		if (elapsed_s==last_elapsed_s)
			continue;
		printf("thread with ID %d has been running for %ds\r\n", tid, elapsed_s);
		last_elapsed_s = elapsed_s;
		sleep(900);
	}
	return 0;
}
