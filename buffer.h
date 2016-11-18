#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct
{
	char *data, *cur, *last;
	size_t size;
} buffer;

inline void buffer_create(buffer*, size_t);
inline void buffer_destroy(buffer*);
inline void buffer_clear(buffer*);
inline size_t buffer_data_len(const buffer*);
inline char* buffer_data(const buffer*);
inline size_t buffer_pending_len(const buffer*);
inline size_t buffer_space_left(const buffer*);
inline void buffer_shrink(buffer*);
inline void buffer_reserve(buffer*, size_t);
inline char buffer_read_byte(buffer*);
inline void buffer_push(buffer*, char);
inline void buffer_write(buffer*, const char* data, size_t len);
inline void buffer_set_end(buffer*, char* end);

void buffer_create(buffer* buf, size_t size)
{
	buf->data = buf->cur = buf->last = (char*)malloc(size);
	buf->size = size;
}

void buffer_destroy(buffer* buf)
{
	free(buf->data);
}

void buffer_clear(buffer* buf)
{
	buf->cur = buf->last = buf->data;
}

size_t buffer_data_len(const buffer* buf)
{
	return buf->last - buf->data;
}

char* buffer_data(const buffer* buf)
{
	return buf->data;
}

size_t buffer_pending_len(const buffer* buf)
{
	return buf->last - buf->cur;
}

char* buffer_pending(const buffer* buf)
{
	return buf->cur;
}

size_t buffer_space_left(const buffer* buf)
{
	return buf->size - (buf->last - buf->data);
}

char* buffer_space(const buffer* buf)
{
	return buf->last;
}

void buffer_skip(buffer* buf, size_t count)
{
	assert(buffer_pending_len(buf) >= count);
	buf->cur += count;
}

void buffer_shrink(buffer* buf)
{
	size_t len = buf->last - buf->cur;
	memmove(buf->data, buf->cur, len);
	buf->cur = buf->data;
	buf->last = buf->cur + len;
}

void buffer_reserve(buffer* buf, size_t size)
{
	size_t space_left = buffer_space_left(buf);
	if(space_left < size)
	{
		size_t from = buf->cur - buf->data;
		size_t to = buf->last - buf->data;
		buf->size += size - space_left;
		if((buf->data = (char*)realloc(buf->data, buf->size)))
		{
			buf->cur = buf->data + from;
			buf->last = buf->data + to;
		}
	}
}

char buffer_read_byte(buffer* buf)
{
	assert(buffer_pending_len(buf) > 0);
	return *buf->cur++;
}

void buffer_push(buffer* buf, char c)
{
	assert(buffer_space_left(buf) >= 1);
	*buf->last++ = c;
}

void buffer_write(buffer* buf, const char* data, size_t len)
{
	assert(buffer_space_left(buf) >= len);
	if(data) memcpy(buf->last, data, len);
	buf->last += len;
}

void buffer_set_end(buffer* buf, char* end)
{
	assert(end <= buf->data + buf->size);
	buf->last = end;
}

#ifdef __cplusplus

struct Buffer
{
	buffer buf;

	Buffer(size_t size) { buffer_create(&buf, size); }
	Buffer(Buffer&& o) : buf(o.buf) { o.buf.data = 0; }
	//Buffer(const buffer& o) {}
	~Buffer() { buffer_destroy(&buf); }
	Buffer& operator=(Buffer&& o) { buf = o.buf; o.buf.data = 0; return *this; }
	//Buffer& operator=(const Buffer& o) { Buffer x(o); swap(x); return *this; }

	char& front() const { return begin()[0]; }
	char& back() const { return end()[-1]; }
	char* begin() const { return buffer_pending(&buf); }
	char* end() const { return buffer_space(&buf); }

	size_t dataLen() const { return buffer_data_len(&buf); }
	size_t size() const { return buffer_pending_len(&buf); }
	bool empty() const { return !size(); }
	void clear() { buffer_clear(&buf); }

	size_t spaceLeft() const { return buffer_space_left(&buf); }
	char& operator[](size_t i) { assert(i < size()); return begin()[i]; }
	void push(char c) { buffer_push(&buf, c); }
	void skip(size_t len) { buffer_skip(&buf, len); }
	//void wrote(size_t len) { assert(len <= available()); last += len; }
	void write(const char* data, size_t len) { buffer_write(&buf, data, len);  }
	void setEnd(char* end) { buffer_set_end(&buf, end); }

	void reserve(size_t len) { buffer_reserve(&buf, len); }
	void shrink() { buffer_shrink(&buf); }
};

#endif

#endif
