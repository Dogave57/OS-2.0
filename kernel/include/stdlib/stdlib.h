#ifndef _STDLIB
#define _STDLIB
#include <stdint.h>
#include "bootloader.h"
int atoi(long long num, unsigned char* buf, unsigned int bufmax);
int printf(unsigned char* fmt, ...);
int lprintf(uint16_t* fmt, ...);
int memset(uint64_t* mem, uint64_t value, uint64_t size);
unsigned char toUpper(unsigned char ch);
int strcpy(unsigned char* dest, unsigned char* src);
int memcpy_align64(uint64_t* dest, uint64_t* src, uint64_t qwords);
int memcpy_align32(uint32_t* dest, uint32_t* src, uint64_t dwords);
int memcpy(unsigned char* dest, unsigned char* src, uint64_t len);
#endif
