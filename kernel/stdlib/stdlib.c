#include <stdarg.h>
#include "graphics.h"
#include "stdlib.h"
int atoi(long long num, CHAR16* buf, unsigned int bufmax){
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
		buf[digitcnt-i] = L'0'+digit;
		num/=10;
	}
	buf[digitcnt+1] = 0;
	return 0;
}
int printf(CHAR16* fmt, ...){
	if (!fmt)
		return -1;
	va_list args = 0;
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
			CHAR16 buf[64] = {0};
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
			CHAR16* str = (CHAR16*)va_arg(args, void*);	 
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
int memset(uint64_t* mem, uint64_t value, uint64_t size){
	if (!mem)
		return -1;
	for (uint64_t i = 0;i<size/8;i++){
		mem[i] = value;
	}
	return 0;
}
