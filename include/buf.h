#ifndef __BUF_H__
#define __BUF_H__

#include <stddef.h>
#include <stdint.h>

struct buf {
	size_t size;
	uint8_t *data;
};

void buf_destruct(struct buf *buf);

struct buf *buf_new(size_t size);

#endif // __BUF_H__
