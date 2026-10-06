#ifndef _SERIAL
#define _SERIAL
#include "bootloader.h"
#define SERIAL_DEBUG_PORT 0
int serial_init_port(unsigned int port);
int serial_init(void);
unsigned int serial_initialized(void);
KAPI int serial_putchar(unsigned int port, unsigned char ch);
KAPI int serial_print(unsigned int port, unsigned char* msg);
#endif
