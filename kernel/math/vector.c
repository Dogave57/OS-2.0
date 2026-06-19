#include "mem/vmm.h"
#include "stdlib/stdlib.h"
#include "math/basic.h"
#include "math/vector.h"
KAPI struct fvec3_32 fvec3_32_add(struct fvec3_32 vec1, struct fvec3_32 vec2){
	struct fvec3_32 result = {0};
	result.x = vec1.x+vec2.x;
	result.y = vec1.y+vec2.y;
	result.z = vec1.z+vec2.z;
	return result;
}
KAPI struct fvec3_32 fvec3_32_sub(struct fvec3_32 vec1, struct fvec3_32 vec2){
	struct fvec3_32 result = {0};
	result.x = vec1.x-vec2.x;
	result.y = vec1.y-vec2.y;
	result.z = vec1.z-vec2.z;
	return result;
}
KAPI struct fvec3_64 fvec3_64_add(struct fvec3_64 vec1, struct fvec3_64 vec2){
	struct fvec3_64 result = {0};
	result.x = vec1.x+vec2.x;
	result.y = vec1.y+vec2.y;
	result.z = vec1.z+vec2.z;
	return result;
}
KAPI struct fvec3_64 fvec3_64_sub(struct fvec3_64 vec1, struct fvec3_64 vec2){
	struct fvec3_64 result = {0};
	result.x = vec1.x-vec2.x;
	result.y = vec1.y-vec2.y;
	result.z = vec1.z-vec2.z;
	return result;
}
KAPI struct fvec4_32 fvec4_32_add(struct fvec4_32 vec1, struct fvec4_32 vec2){
	struct fvec4_32 result = {0};
	result.x = vec1.x+vec2.x;
	result.y = vec1.y+vec2.y;
	result.z = vec1.z+vec2.z;
	return result;
}
KAPI struct fvec4_32 fvec4_32_sub(struct fvec4_32 vec1, struct fvec4_32 vec2){
	struct fvec4_32 result = {0};
	result.x = vec1.x-vec2.x;
	result.y = vec1.y-vec2.y;
	result.z = vec1.z-vec2.z;
	return result;
}
KAPI struct fvec4_64 fvec4_64_add(struct fvec4_64 vec1, struct fvec4_64 vec2){
	struct fvec4_64 result = {0};
	result.x = vec1.x+vec2.x;
	result.y = vec1.y+vec2.y;
	result.z = vec1.z+vec2.z;
	return result;
}
KAPI struct fvec4_64 fvec4_64_sub(struct fvec4_64 vec1, struct fvec4_64 vec2){
	struct fvec4_64 result = {0};
	result.x = vec1.x-vec2.x;
	result.y = vec1.y-vec2.y;
	result.z = vec1.z-vec2.z;
	return result;
}
KAPI struct fvec3_32 fvec3_32_mul(struct fvec3_32 vec1, struct fvec3_32 vec2){
	struct fvec3_32 result = {0};
	result.x = vec1.x*vec2.x;
	result.y = vec1.y*vec2.y;
	result.z = vec1.z*vec2.z;
	return result;
}
KAPI struct fvec3_32 fvec3_32_div(struct fvec3_32 vec1, struct fvec3_32 vec2){
	struct fvec3_32 result = {0};
	result.x = vec1.x/vec2.x;
	result.y = vec1.y/vec2.y;
	result.z = vec1.z/vec2.z;
	return result;
}
KAPI struct fvec3_64 fvec3_64_mul(struct fvec3_64 vec1, struct fvec3_64 vec2){
	struct fvec3_64 result = {0};
	result.x = vec1.x*vec2.x;
	result.y = vec1.y*vec2.y;
	result.z = vec1.z*vec2.z;
	return result;
}
KAPI struct fvec3_64 fvec3_64_div(struct fvec3_64 vec1, struct fvec3_64 vec2){
	struct fvec3_64 result = {0};
	result.x = vec1.x/vec2.x;
	result.y = vec1.y/vec2.y;
	result.z = vec1.z/vec2.z;
	return result;
}
KAPI struct fvec4_32 fvec4_32_mul(struct fvec4_32 vec1, struct fvec4_32 vec2){
	struct fvec4_32 result = {0};
	result.x = vec1.x*vec2.x;
	result.y = vec1.y*vec2.y;
	result.z = vec1.z*vec2.z;
	return result;
}
KAPI struct fvec4_32 fvec4_32_div(struct fvec4_32 vec1, struct fvec4_32 vec2){
	struct fvec4_32 result = {0};
	result.x = vec1.x/vec2.x;
	result.y = vec1.y/vec2.y;
	result.z = vec1.z/vec2.z;
	return result;
}
KAPI struct fvec4_64 fvec4_64_mul(struct fvec4_64 vec1, struct fvec4_64 vec2){
	struct fvec4_64 result = {0};
	result.x = vec1.x*vec2.x;
	result.y = vec1.y*vec2.y;
	result.z = vec1.z*vec2.z;
	return result;
}
KAPI struct fvec4_64 fvec4_64_div(struct fvec4_64 vec1, struct fvec4_64 vec2){
	struct fvec4_64 result = {0};
	result.x = vec1.x/vec2.x;
	result.y = vec1.y/vec2.y;
	result.z = vec1.z/vec2.z;
	return result;
}
KAPI struct fvec3_32 normf3_32(struct fvec3_32 vec){
	struct fvec3_32 result = {0};
	result.x = 0.0f;
	result.y = 0.0f;
	result.z = 0.0f;
	float dot = (vec.x*vec.x)+(vec.y*vec.y)+(vec.z*vec.z);
	if (!dot){
		printf("invalid dot product\r\n");
		return result;
	}
	float magnitude = (float)sqrtf((double)dot);
	result.x = vec.x/magnitude;
	result.y = vec.y/magnitude;
	result.z = vec.z/magnitude;
	return result;
}
KAPI struct fvec3_64 normf3_64(struct fvec3_64 vec){
	struct fvec3_64 result = {0};
	result.x = 0.0;
	result.y = 0.0;
	result.z = 0.0;
	double dot = (vec.x*vec.x)+(vec.y*vec.y)+(vec.z*vec.z);
	if (!dot)
		return result;
	double magnitude = sqrtf_inv(dot);
	result.x = vec.x*magnitude;
	result.y = vec.y*magnitude;
	result.z = vec.z*magnitude;
	return result;
}
KAPI struct fvec3_32 crossf3_32(struct fvec3_32 vec1, struct fvec3_32 vec2){
	struct fvec3_32 result = {0};
	result.x = (vec1.y*vec2.z)-(vec1.z*vec2.y);
	result.y = (vec1.z*vec2.x)-(vec1.x*vec2.z);
	result.z = (vec1.x*vec2.y)-(vec1.y*vec2.x);
	return result;
}
KAPI struct fvec3_64 crossf3_64(struct fvec3_64 vec1, struct fvec3_64 vec2){
	struct fvec3_64 result = {0};
	result.x = (vec1.y*vec2.z)-(vec1.z*vec2.y);
	result.y = (vec1.z*vec2.x)-(vec1.x*vec2.z);
	result.z = (vec1.x*vec2.y)-(vec1.y*vec2.x);
	return result;
}
KAPI float distf32(struct fvec2_32 vec1, struct fvec2_32 vec2){
	float result = (float)sqrtf((float)(((vec2.x-vec1.x)*(vec2.x-vec1.x))+((vec2.y-vec1.y)*(vec2.y-vec1.y))));
	return result;
}
KAPI double distf64(struct fvec2_64 vec1, struct fvec2_64 vec2){
	double result = (double)sqrtf((double)(((vec2.x-vec1.x)*(vec2.x-vec1.x))+((vec2.y-vec1.y)*(vec2.y-vec1.y))));
	return result;
}
KAPI int32_t dist32(struct vec2_32 vec1, struct vec2_32 vec2){
	int32_t result = 0x00;
	int32_t value1 = (int32_t)absq((int32_t)(vec2.x-vec1.x));
	int32_t value2 = (int32_t)absq((int32_t)(vec2.y-vec1.y));
	int32_t minValue = (int32_t)minq((int64_t)value1, (int64_t)value2);
	int32_t maxValue = (int32_t)maxq((int64_t)value1, (int64_t)value2);
	result = maxValue+(int32_t)(((((int64_t)minValue)*27146)>>16)*(int64_t)minValue);
	return result;
}
KAPI int64_t dist64(struct vec2_64 vec1, struct vec2_64 vec2){
	int64_t result = 0x00;
	int64_t value1 = (int64_t)absq(vec2.x-vec1.x);
	int64_t value2 = (int64_t)absq(vec2.y-vec1.y);
	int64_t minValue = minq(value1, value2);
	int64_t maxValue = maxq(value1, value2);
	result = maxValue+((((minValue*27146)>>16)));
	return result;
}
KAPI uint32_t distu32(struct uvec2_32 vec1, struct uvec2_32 vec2){
	uint32_t result = 0x00;
	uint32_t value1 = (uint32_t)absq(((int32_t)vec2.x)-(int32_t)vec1.x);
	uint32_t value2 = (uint32_t)absq(((int32_t)vec2.y)-(int32_t)vec1.y);
	uint32_t minValue = (uint32_t)uminq((uint64_t)value1, (uint64_t)value2);
	uint32_t maxValue = (uint32_t)umaxq((uint64_t)value1, (uint64_t)value2);
	result = maxValue+(uint32_t)(((((uint32_t)minValue)*27146)>>16)*(uint32_t)minValue);
	return result;
}
KAPI uint64_t distu64(struct uvec2_64 vec1, struct uvec2_64 vec2){
	uint64_t result = 0x00;
	uint64_t value1 = (uint64_t)absq(((int64_t)vec2.x)-(int64_t)vec1.x);
	uint64_t value2 = (uint64_t)absq(((int64_t)vec2.y)-(int64_t)vec1.y);
	uint64_t minValue = uminq(value1, value2);
	uint64_t maxValue = umaxq(value1, value2);
	result = maxValue+(((minValue*27146)>>16)*minValue);
	return result;
}
KAPI struct fvec2_32 lerpf32(struct fvec2_32 vec1, struct fvec2_32 vec2, float ratio){
	struct fvec2_32 result = {0};
	result.x = vec1.x+(ratio*(float)(vec2.x-vec1.x));
	result.y = vec1.y+(ratio*(float)(vec2.y-vec1.y));
	return result;
}
KAPI struct fvec2_64 lerpf64(struct fvec2_64 vec1, struct fvec2_64 vec2, double ratio){
	struct fvec2_64 result = {0};
	result.x = vec1.x+(ratio*(double)(vec2.x-vec1.x));
	result.y = vec1.y+(ratio*(double)(vec2.y-vec1.y));
	return result;
}
KAPI struct vec2_32 lerp32(struct vec2_32 vec1, struct vec2_32 vec2, float ratio){
	struct vec2_32 result = {0};
	result.x = vec1.x+(int32_t)roundf(ratio*(float)(vec2.x-vec1.x));
	result.y = vec1.y+(int32_t)roundf(ratio*(float)(vec2.y-vec1.y));
	return result;
}
KAPI struct vec2_64 lerp64(struct vec2_64 vec1, struct vec2_64 vec2, double ratio){
	struct vec2_64 result = {0};
	result.x = result.x+(int64_t)roundf(ratio*(double)(vec2.x-vec1.x));
	result.y = result.y+(int64_t)roundf(ratio*(double)(vec2.y-vec1.y));
	return result;
}
KAPI struct uvec2_32 lerpu32(struct uvec2_32 vec1, struct uvec2_32 vec2, float ratio){
	struct uvec2_32 result = {0};
	result.x = vec1.x+(uint32_t)roundf(ratio*(float)(((int64_t)vec2.x)-(int64_t)vec1.x));
	result.y = vec1.y+(uint32_t)roundf(ratio*(float)(((int64_t)vec2.y)-(int64_t)vec1.y));
	return result;
}
KAPI struct uvec2_64 lerpu64(struct uvec2_64 vec1, struct uvec2_64 vec2, double ratio){
	struct uvec2_64 result = {0};
	result.x = vec1.x+(uint64_t)roundf(ratio*(double)(((int64_t)vec2.x)-(int64_t)vec1.x));
	result.y = vec1.y+(uint64_t)roundf(ratio*(double)(((int64_t)vec2.y)-(int64_t)vec1.y));
	return result;
}
