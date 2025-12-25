#ifndef _TIMER
#define _TIMER
#include <stdint.h>
#include "kernel_api.h"
int timer_reset(void);
KAPI uint64_t get_time_ms(void);
int set_time_ms(uint64_t ms);
int timer_set_tpms(uint64_t tpms);
uint64_t timer_get_tpms(void);
KAPI int sleep(uint64_t ms);
#endif
