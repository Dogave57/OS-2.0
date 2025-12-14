#include "stdlib/stdlib.h"
#include "drivers/timer.h"
#include "crypto/random.h"
uint64_t crypto_entropy = 0xDEADBEEFdeadbeef;
uint8_t urandom8(void){
	uint8_t result = (uint8_t)crypto_entropy&0xFF;
	entropy_shuffle();
	return result;
}
uint16_t urandom16(void){
	uint16_t result = (uint16_t)crypto_entropy&0xFFFF;
	entropy_shuffle();
	return result;
}
uint32_t urandom32(void){
	uint32_t result = (uint32_t)crypto_entropy&0xFFFFFFFF;
	entropy_shuffle();
	return result;
}
uint64_t urandom64(void){
	uint64_t result = (uint64_t)crypto_entropy;
	entropy_shuffle();
	return result;
}
int8_t random8(void){
	int8_t result = (int8_t)crypto_entropy&0xFF;
	entropy_shuffle();
	return result;
}
int16_t random16(void){
	int16_t result = (int16_t)crypto_entropy&0xFFFF;
	entropy_shuffle();	
	return result;
}
int32_t random32(void){
	int32_t result = (int32_t)crypto_entropy&0xFFFFFFFF;
	entropy_shuffle();
	return result;
}
int64_t random64(void){
	int64_t result = (int64_t)crypto_entropy;
	crypto_entropy*=(12945845%result);
	entropy_shuffle();
	return result;
}
uint8_t urandint8(uint8_t min, uint8_t max){
	uint8_t num = urandom8();
	uint8_t result = min+(num%(max-min));	
	return result;
}
uint16_t urandint16(uint16_t min, uint16_t max){
	uint16_t num = urandom16();
	uint16_t result = min+(num%(max-min));
	return result;
}
uint32_t urandint32(uint32_t min, uint32_t max){
	uint32_t num = urandom32();
	uint32_t result = min+(num%(max-min));
	return result;
}
uint64_t urandint64(uint64_t min, uint64_t max){
	uint64_t num = urandom64();
	uint64_t result = min+(num%(max-min));
	return result;
}
int8_t randint8(int8_t min, int8_t max){
	int8_t num = random8();
	if (num<0)
		num*=-1;
	int8_t result = min+(num%(max-min));
	return result;
}
int16_t randint16(int16_t min, int16_t max){
	int16_t num = random16();
	if (num<0)
		num*=-1;
	int16_t result = min+(num%(max-min));
	return result;
}
int32_t randint32(int32_t min, int32_t max){
	int32_t num = random32();
	if (num<0)
		num*=-1;
	int32_t result = min+(num%(max-min));
	return result;
}
int64_t randint64(int64_t min, int64_t max){
	int64_t num = random64();
	if (num<0)
		num*=-1;
	int64_t result = min+(num%(max-min));
	return result;
}
int entropy_shuffle(void){
	crypto_entropy*=0xfac1ffffeee;
	crypto_entropy^=crypto_entropy<<24;
	crypto_entropy+=get_time_ms();
	crypto_entropy*=0xfffeead234;
	crypto_entropy^=crypto_entropy>>31;
	crypto_entropy*=((uint64_t)entropy_shuffle)&0xFFFFFFFF;
	return 0;
}
