#ifndef _STDLIB
#define _STDLIB
#include <stdint.h>
#include "bootloader.h"
int atoi(long long num, CHAR16* buf, unsigned int bufmax);
int printf(CHAR16* fmt, ...);
#endif
