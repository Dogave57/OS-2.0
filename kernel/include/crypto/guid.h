#ifndef _GUID
#define _GUID
#define GUID_SIZE (16)
int guidcmp(unsigned char* guid1, unsigned char* guid2);
int guidcpy(unsigned char* dest, unsigned char* src);
int guidgen(unsigned char* pGuid);
#endif
