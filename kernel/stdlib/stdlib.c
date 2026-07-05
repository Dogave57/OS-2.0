#include <stdarg.h>
#include <stdint.h>
#include "cpu/thread.h"
#include "cpu/mutex.h"
#include "math/basic.h"
#include "subsystem/text.h"
#include "stdlib/stdlib.h"
unsigned char printfLock = 0;
KAPI int itoa64(int64_t value, unsigned char* pBuffer, uint64_t bufferSize, uint64_t* pBufferLength){
	if (!pBuffer||!bufferSize||!pBufferLength)
		return -1;
	uint64_t bufferLength = 0x00;
	if (value<0){
		value*=-1;
		*pBuffer = '-';
		bufferLength++;
	}
	if (!value){
		pBuffer[0x00] = '0';
		bufferLength = 0x01;
		*pBufferLength = bufferLength;
		return 0;
	}
	uint64_t digitCount = 0x00;
	for (int64_t tmp = value;tmp;tmp/=10,digitCount++){};	
	for (uint64_t i = 0x00;value;i++){
		uint64_t digit = value%0x0A;
		i++;
		if (bufferLength+digitCount-i>bufferSize-0x01)
			break;
		pBuffer[bufferLength+digitCount-i] = digit+'0';
		bufferLength++;
		value/=0x0A;
	}
	*pBufferLength = bufferLength;
	return 0;
}
KAPI int utoa64(uint64_t value, unsigned char* pBuffer, uint64_t bufferSize, uint64_t* pBufferLength){
	if (!pBuffer||!bufferSize||!pBufferLength)
		return -1;
	uint64_t bufferLength = 0x00;
	if (!value){
		pBuffer[bufferLength] = '0';
		bufferLength++;
		*pBufferLength = bufferLength;
		return 0;
	}
	uint64_t digitCount = 0x00;
	for (uint64_t temp = value;temp;temp/=0x0A,digitCount++){};
	for (uint64_t i = 0;value;i++){
		uint64_t digit = value%0x0A;
		i++;
		if (bufferLength+digitCount-i>bufferSize-0x01)
			break;
		pBuffer[bufferLength+digitCount-i] = digit+'0';
		bufferLength++;
		value/=0x0A;
	}
	*pBufferLength = bufferLength;
	return 0;
}
KAPI int printf(unsigned char* fmt, ...){
	if (!fmt)
		return -1;
	va_list args = {0};
	va_start(args, fmt);
	for (unsigned int i = 0;fmt[i];i++){
		unsigned char ch = fmt[i];
		if (ch!='%'){
			putchar(ch);
			continue;
		}
		switch (fmt[i+1]){
			case 'd':{
			case 'l':
			if (fmt[i+1]=='l'&&fmt[i+2]!='l'&&fmt[i+3]!='d')
				break;
			long long value = (long long)0x00;
			if (fmt[i+1]=='d'){
				value = (long long)va_arg(args, int);
				i++;
			}
			else{
				value = (long long)va_arg(args, long long);
				i+=3;
			}
			unsigned char buffer[64] = {0};
			uint64_t bufferLength = 0x00;
			itoa64(value, (unsigned char*)buffer, 0x40, &bufferLength);
			buffer[bufferLength] = 0x00;
			print(buffer);
			break;	 
			}
			case 'f':{
			double value = (double)va_arg(args, double);
			uint64_t pointIndex = 0;
			uint64_t qwordValue = 0;
			uint64_t digitCount = 0;
			if (!value){
				print("0.0");
				i++;
				break;
			}
			if (value<0.0){
				value = value*-1.0;
				putchar('-');
			}
			if (value==(double)floorf(value)){
				printf("%d", (uint64_t)floorf(value));
				i++;
				break;
			}
			uint8_t smallFloat = 0;
			if (value<1.0){
				value+=1.0;
				smallFloat = 1;
			}
			for (pointIndex = 0;;pointIndex++){
				if (value!=(double)((uint64_t)value)&&pointIndex<16){
					value*=10.0;
					continue;
				}
				qwordValue = (uint64_t)value;
				break;
			}
			for (digitCount = 0;qwordValue;digitCount++){
				qwordValue/=10;
			}
			unsigned char buf[256] = {0};
			buf[digitCount+0x01] = 0x00;
			qwordValue = (uint64_t)value;
			for (uint64_t i = 0;qwordValue&&i<256;i++){
				uint64_t digitValue = qwordValue%10;
				if (i==pointIndex){
					buf[digitCount-i] = '.';
					i++;
				}
				buf[digitCount-i] = '0'+digitValue;
				qwordValue/=10;
			}
			if (smallFloat)
				buf[0] = '0';
			print(buf);
			i++;
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
			putlchar(ch);
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
			unsigned char buffer[64] = {0};
			uint64_t bufferLength = 0x00;
			itoa64(value, (unsigned char*)buffer, 0x40, &bufferLength);
			buffer[bufferLength] = 0x01;
			print((unsigned char*)buffer);
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
			case L's':{
			uint16_t* str = (uint16_t*)va_arg(args, void*);	 
			lprint(str);
			i++;
			break;	 
			}
			case L'c':{
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
KAPI int snprintf(unsigned char* pData, uint64_t dataSize, uint64_t* pDataLength, unsigned char* pFormat, ...){
	if (!pData||!pDataLength||!pFormat)
		return -1;	
	va_list args = {0};
	va_start(args, pFormat);
	uint64_t currentEntry = 0x00;
	for (unsigned int i = 0;pFormat[i];i++){
		if (dataSize&&i>=dataSize)
			break;
		unsigned char ch = pFormat[i];
		if (ch!='%'){
			pData[currentEntry] = ch;
			currentEntry++;
			continue;
		}
		switch (pFormat[i+1]){
			case 'd':{
			case 'l':
			if (pFormat[i+1]=='l'&&pFormat[i+2]!='l'&&pFormat[i+3]!='d')
				break;
			long long value = (long long)0x00;
			if (pFormat[i+1]=='d'){
				value = (long long)va_arg(args, int);
				i++;
			}
			else{
				value = (long long)va_arg(args, long long);
				i+=3;
			}
			uint64_t bufferLength = 0x00;
			itoa64(value, (unsigned char*)(pData+currentEntry), dataSize-currentEntry, &bufferLength);
			currentEntry+=bufferLength;
			break;	 
			}
			case 'f':{
			double value = (double)va_arg(args, double);
			uint64_t pointIndex = 0;
			uint64_t qwordValue = 0;
			uint64_t digitCount = 0;
			if (!value){
				print("0.0");
				i++;
				break;
			}
			if (value<0.0){
				value = value*-1.0;
				pData[currentEntry] = '-';
				currentEntry++;
			}
			if (value==(double)floorf(value)){
				unsigned char baseValue[64] = {0};
				i++;
				break;
			}
			uint8_t smallFloat = 0;
			if (value<1.0){
				value+=1.0;
				smallFloat = 1;
			}
			for (pointIndex = 0;;pointIndex++){
				if (value!=(double)((uint64_t)value)&&pointIndex<16){
					value*=10.0;
					continue;
				}
				qwordValue = (uint64_t)value;
				break;
			}
			for (digitCount = 0;qwordValue;digitCount++){
				qwordValue/=10;
			}
			qwordValue = (uint64_t)value;
			if (smallFloat){
				pData[currentEntry] = '0';
				currentEntry++;
			}
			for (uint64_t i = 0;qwordValue&&i<256;i++){
				uint64_t digitValue = qwordValue%10;
				if (i==pointIndex){
					pData[currentEntry+digitCount-i] = '0';
					i++;
				}
				pData[currentEntry+digitCount-i] = '0'+digitValue;
				qwordValue/=10;
			}
			currentEntry+=digitCount+0x01;
			i++;
			break;	 
			}
			case 'p':{
			case 'X':
			case 'x':
			unsigned long long value = (unsigned long long)va_arg(args, unsigned long long);
			unsigned char isUpper = 0;
			if (pFormat[i+0x01]=='X')
				isUpper = 0x01;
			if (pFormat[i+0x01]=='p'){
				pData[currentEntry] = '0';
				currentEntry++;
				pData[currentEntry] = 'x';
				currentEntry++;
			}
			for (int i = 15;i>-1;i--){
				unsigned char hex = (unsigned char)((value>>(i*4))&0xF);
				unsigned char value = 0x00;
				if (value<0x0A)
					value = '0'+hex;
				if (value>=0x0A)
					value = isUpper ? 'A'+(hex-0x0A) : 'a'+(hex-0x0A);
				pData[currentEntry] = value;
				currentEntry++;
			}	
			i++;
			break;
			}
			case 's':{
			unsigned char* str = (unsigned char*)va_arg(args, void*);	 
			for (uint64_t i = 0;str[i]!=0x00;i++){
				*(pData+currentEntry) = str[i];
				currentEntry++;
			}
			i++;
			break;	 
			}
			case 'c':{
			unsigned char ch = (unsigned char)va_arg(args, unsigned int);
			pData[currentEntry] = ch;
			currentEntry++;
			i++;
			break;
			}
		}
	}
	va_end(args);		
	*pDataLength = currentEntry;
	return 0;
}
KAPI int memset(uint8_t* mem, uint8_t value, uint64_t size){
	if (!mem)
		return -1;
	for (uint64_t i = 0;i<size;i++){
		mem[i] = value;
	}
	return 0;
}
KAPI int memset_16(uint16_t* mem, uint16_t value, uint64_t size){
	if (!mem)
		return -1;
	for (uint64_t i = 0;i<size/sizeof(uint16_t);i++){
		mem[i] = value;
	}
	return 0;
}
KAPI int memset_32(uint32_t* mem, uint32_t value, uint64_t size){
	if (!mem)
		return -1;
	for (uint64_t i = 0;i<size/sizeof(uint32_t);i++){
		mem[i] = value;
	}
	return 0;
}
KAPI int memset_64(uint64_t* mem, uint64_t value, uint64_t size){
	if (!mem)
		return -1;
	for (uint64_t i = 0;i<size/sizeof(uint64_t);i++){
		mem[i] = value;
	}
	return 0;
}
KAPI unsigned char toUpper(unsigned char ch){
	unsigned char upper = ch;
	static const unsigned char mapping[256]={
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
		['\b'] = '\b',
		['\t'] = '\t',
		['\n'] = '\n',
	};
	if (upper>='a'&&upper<='z')
		return 'A'+(upper-'a');
	if (upper>='A'&&upper<'Z')
		return upper;
	return mapping[upper];
}
KAPI unsigned char toLower(unsigned char ch){
	unsigned char lower = ch;
	static const unsigned char mapping[256]={
		['~']='`',
		['!']='1',
		['@']='2',
		['#']='3',
		['$']='4',
		['%']='5',
		['^']='6',
		['&']='7',
		['*']='8',
		['(']='9',
		[')']='0',
		['<']=',',
		['>']='.',
		[';']=':',
		['\"']='\'',
		['{']='[',
		['}']=']',
		['|']='\\',
		['+']='=',
		['_']='-',
		['\b']='\b',
		['\t']='\t',
		['\n']='\n'
	};
	if (lower>='A'&&lower<='Z')
		return 'a'+(lower-'A');
	if (lower>='a'&&lower<='z')
		return lower;
	return mapping[lower];
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
