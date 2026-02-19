#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t global_key = 0;

void cezar_enc(const uint8_t *src, uint8_t *dst, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		dst[i] = src[i] ^ global_key;
	}
}

void cezar_key(uint8_t key)
{
	global_key = key;
}
