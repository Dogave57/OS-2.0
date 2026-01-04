#ifndef _VECTOR
#define _VECTOR
#include <stdint.h>
struct uvec2{
	uint64_t x, y;
};
struct uvec3{
	uint64_t x, y, z;
};
struct uvec4{
	uint64_t x, y, z, w;
};
struct vec2{
	int64_t x, y;
};
struct vec3{
	int64_t x, y, z;
};
struct vec4{
	int64_t x, y, z, w;
};
struct uvec2_8{
	unsigned char x, y;
};
struct uvec3_8{
	unsigned char x, y, z;
};
struct uvec4_8{
	unsigned char x, y, z, w;
};
struct vec2_8{
	char x, y;
};
struct vec3_8{
	char x, y, z;
};
struct vec4_8{
	char x, y, z, w;
};
#endif
