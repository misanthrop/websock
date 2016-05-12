#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>
#include <string.h>
#include <openssl/sha.h>
#include "base64.h"

enum
{
	websock_header_len = 2,

	websock_opcode_continue = 0,
	websock_opcode_text = 1,
	websock_opcode_binary = 2,
	websock_opcode_close = 8,
	websock_opcode_ping = 9,
	websock_opcode_pong = 10,
	websock_opcode_mask = 0xf,
	websock_final_mask = 0x80,

	websock_len0_max = 125,
	websock_len2_max = 0xffff,
	websock_len2_code = 126,
	websock_len8_code = 127,
	websock_masked_mask = 0x80
};

typedef struct websock_handshake
{
	const char* path;
	size_t path_len;
	const char* sec_key;
	size_t sec_key_len;
} websock_handshake;

typedef struct websock_message
{
	char* data;
	size_t len;
	uint8_t opcode;
	uint8_t final;
	uint8_t masked;
} websock_message;

char* websock_accept_key(char* __restrict dest, const char* __restrict sec_key, size_t sec_key_len)
{
	static const char websocket_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	uint8_t digest[SHA_DIGEST_LENGTH];
	SHA_CTX ctx;
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, sec_key, sec_key_len);
	SHA1_Update(&ctx, websocket_guid, sizeof websocket_guid - 1);
	SHA1_Final(digest, &ctx);
	return base64_encode(digest, sizeof digest, dest);
}

char* websock_accept_handshake(char* __restrict dest, const char* __restrict sec_key, size_t sec_key_len)
{
	static const char handshake_reply[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: ";
	memcpy(dest, handshake_reply, sizeof handshake_reply - 1);
	dest += sizeof handshake_reply - 1;
	dest = websock_accept_key(dest, sec_key, sec_key_len);
	memcpy(dest, "\r\n\r\n", 4);
	dest += 4;
	return dest;
}

char* websock_write_message_len(char* __restrict dest, size_t len)
{
	uint8_t bytes;
	if(len > websock_len2_max)
	{
		*dest++ = websock_len8_code;
		bytes = 8;
	}
	else if(len > websock_len0_max)
	{
		*dest++ = websock_len2_code;
		bytes = 2;
	}
	else
		bytes = 1;
	while(bytes)
	{
		--bytes;
		*dest++ = (len >> 8 * bytes) & 0xff;
	}
	return dest;
}

char* websocket_write_message(char* __restrict dest, const char* __restrict data, size_t len, uint8_t opcode_and_flags)
{
	*dest++ = opcode_and_flags;
	dest = websock_write_message_len(dest, len);
	memcpy(dest, data, len);
	return dest + len;
}

/// returns used bytes count or 0 if more data is needed
size_t websocket_parse_handshake(char* __restrict data, size_t data_len, websock_handshake* hs)
{
	const char *end = (const char*)memmem(data, data_len, "\r\n\r\n", 4);
	if(!end)
		return 0;
	end += 4;

	if(memcmp(data, "GET ", 4))
		return 0;

	hs->path = data + 4;
	const char* path_end = (const char*)memchr(hs->path, ' ', end - hs->path);
	if(!path_end)
		return 0;
	hs->path_len = path_end - hs->path;
	path_end += 1;

	static const char sec_key_property[] = "Sec-WebSocket-Key: ";
	const char *sec_key = (const char*)memmem(path_end, end - path_end, sec_key_property, sizeof sec_key_property - 1);
	if(!sec_key)
		return 0;
	sec_key += sizeof sec_key_property - 1;

	hs->sec_key = sec_key;
	hs->sec_key_len = (const char*)memmem(sec_key, end - sec_key, "\r\n", 2) - sec_key;

	return end - data;
}

/// returns used bytes count or 0 if more data is needed
size_t websocket_decode_message(char* __restrict data, size_t data_len, websock_message* msg)
{
	if(data_len < websock_header_len)
		return 0;

	char* header = data;
	msg->opcode = header[0] & websock_opcode_mask;
	msg->final = header[0] & websock_final_mask;
	msg->masked = header[1] & websock_masked_mask;
	uint8_t mask_len = msg->masked ? 4 : 0;
	size_t len = header[1] & 0x7f;

	uint8_t len_len = len == websock_len8_code ? 8 : len == websock_len2_code ? 2 : 0;
	data += websock_header_len;

	if(len_len)
	{
		if(data_len < websock_header_len + len_len)
			return 0;
		len = data[0];
		for(int i = 1; i < len_len; ++i)
		{
			 len <<= 8;
			 len |= data[i];
		}
		data += len_len;
	}

	msg->len = len;

	if(data_len < websock_header_len + len_len + mask_len + len)
		return 0;

	if(mask_len)
	{
		const char* mask = data;
		data += mask_len;
		for(size_t i = 0; i < len; ++i)
			 data[i] ^= mask[i % mask_len];
	}

	msg->data = data;

	return data - header + len;
}

#endif
