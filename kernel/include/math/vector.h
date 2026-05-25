#ifndef _VECTOR
#define _VECTOR
#include <stdint.h>
#include "kernel_api.h"
struct uvec2{
	uint64_t x, y;
}__attribute__((packed));
struct uvec3{
	uint64_t x, y, z;
}__attribute__((packed));
struct uvec4{
	uint64_t x, y, z, w;
}__attribute__((packed));
struct vec2{
	int64_t x, y;
}__attribute__((packed));
struct vec3{
	int64_t x, y, z;
}__attribute__((packed));
struct vec4{
	int64_t x, y, z, w;
}__attribute__((packed));
struct uvec2_8{
	unsigned char x, y;
}__attribute__((packed));
struct uvec3_8{
	unsigned char x, y, z;
}__attribute__((packed));
struct uvec4_8{
	unsigned char x, y, z, w;
}__attribute__((packed));
struct vec2_8{
	char x, y;
}__attribute__((packed));
struct vec3_8{
	char x, y, z;
}__attribute__((packed));
struct vec4_8{
	char x, y, z, w;
}__attribute__((packed));
struct uvec2_16{
	uint16_t x, y;
}__attribute__((packed));
struct uvec3_16{
	uint16_t x, y, z;
}__attribute__((packed));
struct uvec4_16{
	uint16_t x, y, z, w;
}__attribute__((packed));
struct vec2_16{
	int16_t x, y;
}__attribute__((packed));
struct vec3_16{
	int16_t x, y, z;
}__attribute__((packed));
struct vec4_16{
	int16_t x, y, z, w;
}__attribute__((packed));
struct uvec2_32{
	uint32_t x, y;
}__attribute__((packed));
struct uvec3_32{
	uint32_t x, y, z;
}__attribute__((packed));
struct uvec4_32{
	uint32_t x, y, z, w;
}__attribute__((packed));
struct vec2_32{
	int32_t x, y;
}__attribute__((packed));
struct vec3_32{
	int32_t x, y, z;
}__attribute__((packed));
struct vec4_32{
	int32_t x, y, z, w;
}__attribute__((packed));
struct fvec2_32{
	float x, y;
}__attribute__((packed));
struct fvec3_32{
	float x, y, z;
}__attribute__((packed));
struct fvec4_32{
	float x, y, z, w;
}__attribute__((packed));
struct uvec2_64{
	uint64_t x, y;
}__attribute__((packed));
struct uvec3_64{
	uint64_t x, y, z;
}__attribute__((packed));
struct uvec4_64{
	uint64_t x, y, z, w;
}__attribute__((packed));
struct vec2_64{
	int64_t x, y;
}__attribute__((packed));
struct vec3_64{
	int64_t x, y, z;
}__attribute__((packed));
struct vec4_64{
	int64_t x, y, z, w;
}__attribute__((packed));
struct fvec2_64{
	double x, y;
}__attribute__((packed));
struct fvec3_64{
	double x, y, z;
}__attribute__((packed));
struct fvec4_64{
	double x, y, z, w;
}__attribute__((packed));
KAPI struct fvec3_32 fvec3_32_add(struct fvec3_32 vec1, struct fvec3_32 vec2);
KAPI struct fvec3_32 fvec3_32_sub(struct fvec3_32 vec1, struct fvec3_32 vec2);
KAPI struct fvec3_64 fvec3_64_add(struct fvec3_64 vec1, struct fvec3_64 vec2);
KAPI struct fvec3_64 fvec3_64_sub(struct fvec3_64 vec1, struct fvec3_64 vec2);
KAPI struct fvec4_32 fvec4_32_add(struct fvec4_32 vec1, struct fvec4_32 vec2);
KAPI struct fvec4_32 fvec4_32_sub(struct fvec4_32 vec1, struct fvec4_32 vec2);
KAPI struct fvec4_64 fvec4_64_add(struct fvec4_64 vec1, struct fvec4_64 vec2);
KAPI struct fvec4_64 fvec4_64_sub(struct fvec4_64 vec1, struct fvec4_64 vec2);
KAPI struct fvec3_32 fvec3_32_mul(struct fvec3_32 vec1, struct fvec3_32 vec2);
KAPI struct fvec3_32 fvec3_32_div(struct fvec3_32 vec1, struct fvec3_32 vec2);
KAPI struct fvec3_64 fvec3_64_mul(struct fvec3_64 vec1, struct fvec3_64 vec2);
KAPI struct fvec3_64 fvec3_64_div(struct fvec3_64 vec1, struct fvec3_64 vec2);
KAPI struct fvec4_32 fvec4_32_mul(struct fvec4_32 vec1, struct fvec4_32 vec2);
KAPI struct fvec4_32 fvec4_32_div(struct fvec4_32 vec1, struct fvec4_32 vec2);
KAPI struct fvec4_64 fvec4_64_mul(struct fvec4_64 vec1, struct fvec4_64 vec2);
KAPI struct fvec4_64 fvec4_64_div(struct fvec4_64 vec1, struct fvec4_64 vec2);
KAPI struct fvec3_32 normf3_32(struct fvec3_32 vec);
KAPI struct fvec3_64 normf3_64(struct fvec3_64 vec);
KAPI struct fvec3_32 crossf3_32(struct fvec3_32 vec1, struct fvec3_32 vec2);
KAPI struct fvec3_64 crossf3_64(struct fvec3_64 vec1, struct fvec3_64 vec2);
#endif
