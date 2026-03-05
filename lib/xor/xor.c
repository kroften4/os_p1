#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "xor.h"

static uint8_t global_key = 0;

void xor_encrypt(const uint8_t *src, uint8_t *dst, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		dst[i] = src[i] ^ global_key;
	}
}

void xor_set_key(uint8_t key)
{
	global_key = key;
}
