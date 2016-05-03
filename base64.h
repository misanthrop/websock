#ifndef BASE64_H
#define BASE64_H

#include <stdint.h>
#include <stddef.h>

inline char* base64_encode(const uint8_t* __restrict data, size_t len, char* __restrict result);

char* base64_encode(const uint8_t* __restrict data, size_t len, char* __restrict result)
{
	static const char tab[] = {
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };

	uint32_t triple;
	size_t i = 0, rest = len % 3;
	while(i < len - rest)
	{
		triple = data[i++] << 16;
		triple += data[i++] << 8;
		triple += data[i++];
		*result++ = tab[(triple >> 3 * 6) & 0x3f];
		*result++ = tab[(triple >> 2 * 6) & 0x3f];
		*result++ = tab[(triple >> 1 * 6) & 0x3f];
		*result++ = tab[(triple >> 0 * 6) & 0x3f];
	}

	switch(rest)
	{
	case 1:
		triple = data[i] << 16;
		*result++ = tab[(triple >> 3 * 6) & 0x3f];
		*result++ = tab[(triple >> 2 * 6) & 0x3f];
		*result++ = '=';
		*result++ = '=';
		break;
	case 2:
		triple = (data[i] << 16) + (data[i + 1] << 8);
		*result++ = tab[(triple >> 3 * 6) & 0x3f];
		*result++ = tab[(triple >> 2 * 6) & 0x3f];
		*result++ = tab[(triple >> 1 * 6) & 0x3f];
		*result++ = '=';
		break;
	}
	return result;
}

#endif
