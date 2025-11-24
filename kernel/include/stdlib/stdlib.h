#ifndef _STDLIB
#define _STDLIB
#include <stdint.h>
#include "bootloader.h"
int atoi(long long num, CHAR16* buf, unsigned int bufmax);
int printf(CHAR16* fmt, ...);
int printf_ascii(unsigned char* fmt, ...);
int memset(uint64_t* mem, uint64_t value, uint64_t size);
CHAR16 toUpper(CHAR16 ch);
int strcpy(CHAR16* dest, CHAR16* src);
int strcpy_ascii(unsigned char* dest, unsigned char* src);
#endif
