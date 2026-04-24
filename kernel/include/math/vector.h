#ifndef _VECTOR
#define _VECTOR
#include <stdint.h>
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
struct fvec2_32{
	float x, y;
}__attribute__((packed));
struct fvec3_32{
	float x, y, z;
}__attribute__((packed));
struct fvec4_32{
	float x, y, z, w;
}__attribute__((packed));
#endif
