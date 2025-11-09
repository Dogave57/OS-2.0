#ifndef _PIT
#define _PIT
int pit_set_freq(unsigned int channel, unsigned int freq);
int pit_read_channel(unsigned int channel, unsigned int* pvalue);
#endif
