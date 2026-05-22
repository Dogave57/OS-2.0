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
