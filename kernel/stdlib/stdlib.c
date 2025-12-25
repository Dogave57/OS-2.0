#include <stdarg.h>
#include <stdint.h>
#include "drivers/graphics.h"
#include "stdlib/stdlib.h"
KAPI int atoi(long long num, unsigned char* buf, unsigned int bufmax){
	if (!buf)
		return -1;
	if (num<0){
		num*=-1;
		buf[0] = '-';
		buf++;
		bufmax--;
	}
	if (!num){
		buf[0] = '0';
		buf[1] = 0;
		return 0;
	}
	unsigned int digitcnt = 0;
	for (long long tmp = num;tmp;tmp/=10,digitcnt++){};	
	unsigned int i = 0;
	while (num&&i<bufmax-1){
		unsigned int digit = num%10;
		i++;
		buf[digitcnt-i] = '0'+digit;
		num/=10;
	}
	buf[digitcnt+1] = 0;
	return 0;
}
KAPI int printf(unsigned char* fmt, ...){
	if (!fmt)
		return -1;
	va_list args = {0};
	va_start(args, fmt);
	for (unsigned int i = 0;fmt[i];i++){
		CHAR16 ch = fmt[i];
		if (ch!='%'){
			putchar(ch);
			continue;
		}
		switch (fmt[i+1]){
			case 'd':{
			case 'l':
			if (fmt[i+1]=='l'&&fmt[i+2]!='l'&&fmt[i+3]!='d')
				break;
			long long value = (long long)0;
			if (fmt[i+1]=='d'){
				value = (long long)va_arg(args, int);
				i++;
			}
			else{
				value = (long long)va_arg(args, long long);
				i+=3;
			}
			unsigned char buf[64] = {0};
			atoi(value, buf, 63);
			print(buf);
			break;	 
			}
			case 'p':{
			case 'X':
			case 'x':
			unsigned long long value = (unsigned long long)va_arg(args, unsigned long long);
			unsigned char isUpper = 0;
			if (fmt[i+1]=='X')
				isUpper = 1;
			if (fmt[i+1]=='p'){
				putchar('0');
				putchar('x');
			}
			for (int i = 15;i>-1;i--){
				unsigned char hex = (unsigned char)((value>>(i*4))&0xF);
				puthex(hex, isUpper);
			}	
			i++;
			break;
			}
			case 's':{
			unsigned char* str = (unsigned char*)va_arg(args, void*);	 
			print(str);
			i++;
			break;	 
			}
			case 'c':{
			unsigned char ch = (unsigned char)va_arg(args, unsigned int);
	       		putchar(ch);		
			i++;
			break;
			}
		}
	}
	va_end(args);
	return 0;
}
KAPI int lprintf(uint16_t* fmt, ...){
	if (!fmt)
		return -1;
	va_list args = {0};
	va_start(args, fmt);
	for (unsigned int i = 0;fmt[i];i++){
		uint16_t ch = fmt[i];
		if (ch!=L'%'){
			putchar(ch);
			continue;
		}
		switch (fmt[i+1]){
			case L'd':{
			case L'l':
			if (fmt[i+1]==L'l'&&fmt[i+2]!=L'l'&&fmt[i+3]!=L'd')
				break;
			long long value = (long long)0;
			if (fmt[i+1]==L'd'){
				value = (long long)va_arg(args, int);
				i++;
			}
			else{
				value = (long long)va_arg(args, long long);
				i+=3;
			}
			unsigned char buf[64] = {0};
			atoi(value, buf, 63);
			print(buf);
			break;	 
			}
			case 'p':{
			case 'X':
			case 'x':
			unsigned long long value = (unsigned long long)va_arg(args, unsigned long long);
			unsigned char isUpper = 0;
			if (fmt[i+1]==L'X')
				isUpper = 1;
			if (fmt[i+1]==L'p'){
				putchar('0');
				putchar('x');
			}
			for (int i = 15;i>-1;i--){
				unsigned char hex = (unsigned char)((value>>(i*4))&0xF);
				puthex(hex, isUpper);
			}	
			i++;
			break;
			}
			case 's':{
			uint16_t* str = (uint16_t*)va_arg(args, void*);	 
			lprint(str);
			i++;
			break;	 
			}
			case 'c':{
			uint16_t ch = (uint16_t)va_arg(args, unsigned int);
	       		putlchar(ch);		
			i++;
			break;
			}
		}
	}
	va_end(args);
	return 0;
}
KAPI int memset(uint64_t* mem, uint64_t value, uint64_t size){
	if (!mem)
		return -1;
	for (uint64_t i = 0;i<size/8;i++){
		mem[i] = value;
	}
	return 0;
}
KAPI unsigned char toUpper(unsigned char ch){
	unsigned char upper = ch;
	static const unsigned char mapping[255]={
		['`'] = '~',
		['1'] = '!',
		['2'] = '@',
		['3'] = '#',
		['4'] = '$',
		['5'] = '%',
		['6'] = '^',
		['7'] = '&',
		['8'] = '*',
		['9'] = '(',
		['0'] = ')',
		[','] = '<',
		['.'] = '>',
		[';'] = ':',
		['\''] = '\"',
		['['] = '{',
		[']'] = '}',
		['\\'] = '|',
		['='] = '+',
		['-'] = '_',
	};
	if (upper>='a'&&upper<='z')
		return 'A'+(upper-'a');
	return mapping[upper];
}
KAPI int strcpy(unsigned char* dest, unsigned char* src){
	if (!dest||!src)
		return -1;
	for (uint64_t i = 0;src[i];i++){
		dest[i] = src[i];
	}
	return 0;
}
KAPI int strcmp(unsigned char* str1, unsigned char* str2){
	if (!str1||!str2)
		return -1;
	for (uint64_t i = 0;;i++){
		if (str1[i]!=str2[i])
			return -1;
		if (!str1[i]&&!str2[i])
			return 0;
	}
	return -1;
}
KAPI int memcpy_align64(uint64_t* dest, uint64_t* src, uint64_t qwords){
	if (!dest||!src)
		return -1;
	for (uint64_t i = 0;i<qwords;i++){
		dest[i] = src[i];
	}
	return 0;	
}
KAPI int memcpy_align32(uint32_t* dest, uint32_t* src, uint64_t dwords){
	if (!dest||!src)
		return -1;
	for (uint64_t i = 0;i<dwords;i++){
		dest[i] = src[i];
	}
	return 0;
}
KAPI int memcpy(unsigned char* dest, unsigned char* src, uint64_t len){
	if (!dest||!src||!len)
		return -1;
	if (!(len%sizeof(uint64_t)))
		return memcpy_align64((uint64_t*)dest, (uint64_t*)src, len/sizeof(uint64_t));
	if (!(len%sizeof(uint32_t)))
		return memcpy_align32((uint32_t*)dest, (uint32_t*)src, len/sizeof(uint32_t));
	for (uint64_t i = 0;i<len;i++){
		dest[i] = src[i];
	}
	return 0;
}
