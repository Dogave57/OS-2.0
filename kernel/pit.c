#include "port.h"
#include "pit.h"
int pit_set_freq(unsigned int channel, unsigned int freq){
	freq = 1193182/freq;
	outb(0x43, 0x34);	
	outb(0x00, 0x00);
	unsigned int channel_port = 0x40+channel;
	unsigned int freq_low = freq&0xFFFF;
	unsigned int freq_high = (freq>>16)&0xFFFF;
	outw(channel_port, freq_low);
	outb(0x00, 0x00);
	outw(channel_port, freq_high);
	return 0;
}
int pit_read_channel(unsigned int channel, unsigned int* pvalue){
	outb(0x43, (channel<<6));
	unsigned int channel_port = 0x40+channel;
	*(uint16_t*)pvalue = inb(channel_port);
	outb(0x00, 0x00);
	*(uint16_t*)pvalue|=(inb(channel_port)<<8);
	return 0;
}
