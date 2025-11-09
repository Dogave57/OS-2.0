#ifndef _TIMER
#define _TIMER
#include <stdint.h>
int timer_reset(void);
uint64_t get_time_ms(void);
int set_time_ms(uint64_t ms);
int timer_set_tpms(uint64_t tpms);
uint64_t timer_get_tpms(void);
#endif
