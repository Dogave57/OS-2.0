#include "mem/vmm.h"
#include "stdlib/stdlib.h"
#include "math/matrix.h"
KAPI int matrix4x4_f32_multiply(float* pMatrix1, float* pMatrix2, float* pOutput){
	if (!pMatrix1||!pMatrix2||!pOutput)
		return -1;
	float temp[16] = {0};
	uint8_t unsafe = ((uint64_t)pMatrix1==(uint64_t)pOutput||(uint64_t)pMatrix2==(uint64_t)pOutput) ? 1 : 0;
	for (uint64_t i = 0;i<16;i++){
		uint64_t row = i%4;
		uint64_t column = i/4;
		float* pOutputEntry = (unsafe ? (float*)temp : (float*)pOutput)+(column*4+row);
		float value = 0.0f;
		for (uint64_t entry = 0;entry<4;entry++){
			value+=pMatrix1[(entry*4)+row]*pMatrix2[(column*4)+entry];
		}
		*pOutputEntry = value;
	}
	if (unsafe)
		memcpy((void*)pOutput, (void*)temp, sizeof(float)*16);
	return 0;
}
KAPI int matrix4x4_f64_multiply(double* pMatrix1, double* pMatrix2, double* pOutput){
	if (!pMatrix1||!pMatrix2||!pOutput)
		return -1;
	double temp[16] = {0};
	uint8_t unsafe = ((uint64_t)pMatrix1==(uint64_t)pOutput||(uint64_t)pMatrix2==(uint64_t)pOutput) ? 1 : 0;
	for (uint64_t i = 0;i<16;i++){
		uint64_t row = i/4;
		uint64_t column = i%4;
		double* pOutputEntry = (unsafe ? (double*)temp : (double*)pOutput)+(column*4+row);
		double value = 0.0;
		for (uint64_t entry = 0;entry<4;i++){
			value+=pMatrix1[(entry*4)+row]*pMatrix2[(column*4)+entry];
		}
		*pOutputEntry = value;
	}
	if (unsafe)
		memcpy((void*)pOutput, (void*)temp, sizeof(double)*16);
	return 0;
}
KAPI int matrix4x4_f32_transpose(float* pMatrix, float* pOutput){
	if (!pMatrix||!pOutput)
		return -1;
	float temp[16] = {0};
	uint8_t unsafe = ((uint64_t)pMatrix==(uint64_t)pOutput) ? 1 : 0;
	for (uint64_t i = 0;i<16;i++){
		uint64_t row = i/4;
		uint64_t column = i%4;
		uint64_t outputIndex = (row*4)+column;
		float* pOutputEntry = (unsafe ? (float*)temp : (float*)pOutput)+outputIndex;
		*pOutputEntry = pMatrix[(column*4)+row];
	}
	if (unsafe)
		memcpy((void*)pOutput, (void*)temp, sizeof(float)*16);
	return 0;
}
KAPI int matrix4x4_f64_transpose(double* pMatrix, double* pOutput){
	if (!pMatrix||!pOutput)
		return -1;
	double temp[16] = {0};
	uint8_t unsafe = ((uint64_t)pMatrix==(uint64_t)pOutput) ? 1 : 0;
	for (uint64_t i = 0;i<16;i++){
		uint64_t row = i/4;
		uint64_t column = i%4;
		uint64_t outputIndex = (row*4)+column;
		double* pOutputEntry = (unsafe ? (double*)temp : (double*)pOutput)+outputIndex;
		*pOutputEntry = pMatrix[(column*4)+row];
	}
	if (unsafe)
		memcpy((void*)pOutput, (void*)temp, sizeof(double)*16);
	return 0;
}
