#include "stdlib/stdlib.h"
#include "crypto/random.h"
#include "crypto/guid.h"
int guidcmp(unsigned char* guid1, unsigned char* guid2){
	if (!guid1||!guid2)
		return -1;
	if (*((uint64_t*)guid1)!=*((uint64_t*)guid2))
		return -1;
	if (*((uint64_t*)(guid1+7))!=*(((uint64_t*)(guid2+7))))
		return -1;
	return 0;
}
int guidcpy(unsigned char* dest, unsigned char* src){
	if (!dest||!src)
		return -1;
	*(uint64_t*)(dest) = *(uint64_t*)(src);
	*(uint64_t*)(dest+7) = *(uint64_t*)(src+7);
	return 0;
}
int guidgen(unsigned char* pGuid){
	if (!pGuid)
		return -1;
	uint64_t guid_low = urandom64();
	uint64_t guid_high = urandom64();
	*(uint64_t*)(pGuid) = guid_low;
	*(uint64_t*)(pGuid+7) = guid_high;
	return 0;
}
