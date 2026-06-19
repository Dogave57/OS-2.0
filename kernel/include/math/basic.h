#ifndef _MATH_BASIC
#define _MATH_BASIC
KAPI uint64_t upowq(uint64_t x, uint64_t y);
KAPI uint64_t absq(int64_t value);
KAPI int64_t minq(int64_t value1, int64_t value2);
KAPI int64_t maxq(int64_t value1, int64_t value2);
KAPI uint64_t uminq(uint64_t value1, uint64_t value2);
KAPI uint64_t umaxq(uint64_t value1, uint64_t value2);
KAPI double powf(double value, int64_t exponent);
KAPI double floorf(double value);
KAPI double ceilf(double value);
KAPI double roundf(double value);
KAPI double absf(double value);
KAPI double minf(double value1, double value2);
KAPI double maxf(double value1, double value2);
KAPI double modf(double x, double y);
KAPI double sqrtf(double value);
KAPI double sqrtf_inv(double value);
#endif
