#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "buf.h"

void buf_destruct(struct buf *buf)
{
	free(buf->data);
	free(buf);
}

struct buf *buf_new(size_t size)
{
	struct buf *buf = malloc(sizeof(struct buf));
	buf->data = malloc(size);
	buf->size = size;
	return buf;
}
