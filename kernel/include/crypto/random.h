#ifndef _RANDOM
#define _RANDOM
#include <stdint.h>
uint8_t urandom8(void);
uint16_t urandom16(void);
uint32_t urandom32(void);
uint64_t urandom64(void);
int8_t random8(void);
int16_t random16(void);
int32_t random32(void);
int64_t random64(void);
uint8_t urandint8(uint8_t min, uint8_t max);
uint16_t urandint16(uint16_t min, uint16_t max);
uint32_t urandint32(uint32_t min, uint32_t max);
uint64_t urandint64(uint64_t min, uint64_t max);
int8_t randint8(int8_t min, int8_t max);
int16_t randint16(int16_t min, int16_t max);
int32_t randint32(int32_t min, int32_t max);
int64_t randint64(int64_t min, int64_t max);
int entropy_shuffle(void);
#endif
