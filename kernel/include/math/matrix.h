#ifndef _MATH_MATRIX
#define _MATH_MATRIX
#include "kernel_api.h"
KAPI int matrix4x4_f32_multiply(float* pMatrix1, float* pMatrix2, float* pOutput);
KAPI int matrix4x4_f64_multiply(double* pMatrix1, double* pMatrix2, double* pOutput);
KAPI int matrix4x4_f32_transpose(float* pMatrix, float* pOutput);
KAPI int matrix4x4_f64_transpose(double* pMatrix, double* pOutput);
#endif
