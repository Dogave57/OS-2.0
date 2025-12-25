#ifndef _STDLIB
#define _STDLIB
#include <stdint.h>
#include "kernel_include.h"
#include "bootloader.h"
KAPI int atoi(long long num, unsigned char* buf, unsigned int bufmax);
KAPI int printf(unsigned char* fmt, ...);
KAPI int lprintf(uint16_t* fmt, ...);
KAPI int memset(uint64_t* mem, uint64_t value, uint64_t size);
KAPI unsigned char toUpper(unsigned char ch);
KAPI int strcpy(unsigned char* dest, unsigned char* src);
KAPI int strcmp(unsigned char* str1, unsigned char* str2);
KAPI int memcpy_align64(uint64_t* dest, uint64_t* src, uint64_t qwords);
KAPI int memcpy_align32(uint32_t* dest, uint32_t* src, uint64_t dwords);
KAPI int memcpy(unsigned char* dest, unsigned char* src, uint64_t len);
#endif
