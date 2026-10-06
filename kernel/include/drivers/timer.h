#ifndef _TIMER
#define _TIMER
#include <stdint.h>
#include "kernel_api.h"
int timer_reset(void);
KAPI uint64_t get_time_us(void);
int set_time_us(uint64_t us);
int timer_set_tpus(uint64_t tpus);
uint64_t timer_get_tpus(void);
KAPI int sleep(uint64_t ms);
KAPI int sleep_us(uint64_t us);
#endif
