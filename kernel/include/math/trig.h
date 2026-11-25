#ifndef _MATH_TRIG
#define _MATH_TRIG
#include "kernel_api.h"
#include "math/vector.h"
#define PI_F (3.141592653589793)
#define PI2_F (PI_F*2)
KAPI double anglef_to_radf(double angle);
KAPI double cosf(double rad);
KAPI double sinf(double rad);
KAPI double tanf(double rad);
#endif
