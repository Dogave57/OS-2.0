#ifndef _SERIAL
#define _SERIAL
#include "bootloader.h"
#define SERIAL_DEBUG_PORT 0
int serial_init_port(unsigned int port);
int serial_init(void);
unsigned int serial_initialized(void);
int serial_putchar(unsigned int port, unsigned char ch);
int serial_print(unsigned int port, CHAR16* str);
#endif
