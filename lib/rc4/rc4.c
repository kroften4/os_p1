#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "rc4.h"

#define _RC4_S_SIZE ((size_t)256)
#define _RC4_DATA_SIZE (_RC4_S_SIZE + 2)

static void xor_swap(uint8_t *arr, const uint8_t *i, const uint8_t *j)
{
	arr[*j] = arr[*i] ^ arr[*j];
	arr[*i] = arr[*j] ^ arr[*i];
	arr[*j] = arr[*j] ^ arr[*i];
}

void rc4_encrypt(struct rc4_data *rc4, const uint8_t *src, uint8_t *dst,
				 size_t len)
{
	mprotect(rc4->base, _RC4_DATA_SIZE, PROT_READ | PROT_WRITE);
	for (size_t i = 0; i < len; i++) {
		(*rc4->i)++;
		*rc4->j = *rc4->j + rc4->s[*rc4->i];
		xor_swap(rc4->s, rc4->i, rc4->j);
		dst[i] = src[i] ^ rc4->s[rc4->s[*rc4->j] + rc4->s[*rc4->i]];
	}
	mprotect(rc4->base, _RC4_DATA_SIZE, PROT_NONE);
}

void rc4_init(struct rc4_data *rc4_data, const uint8_t *key,
			  const size_t keylen)
{
	rc4_data->base = mmap(NULL, _RC4_DATA_SIZE, PROT_NONE,
						  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (rc4_data->base == MAP_FAILED) {
		perror("mmap");
		return;
	}
	rc4_data->s = rc4_data->base;
	rc4_data->i = (uint8_t *)rc4_data->base + _RC4_S_SIZE;
	rc4_data->j = (uint8_t *)rc4_data->base + _RC4_S_SIZE + 1;
	mprotect(rc4_data->base, _RC4_DATA_SIZE, PROT_WRITE | PROT_READ);
	for (size_t i = 0; i < _RC4_S_SIZE; i++) {
		rc4_data->s[i] = i;
	}
	size_t j = 0;
	for (size_t i = 0; i < _RC4_S_SIZE; i++) {
		j = (j + rc4_data->s[i] + key[i % keylen]) % _RC4_S_SIZE;
		*rc4_data->i = i;
		*rc4_data->j = j;
		xor_swap(rc4_data->s, rc4_data->i, rc4_data->j);
	}
	*rc4_data->i = 0;
	*rc4_data->j = 0;
	mprotect(rc4_data->base, _RC4_DATA_SIZE, PROT_NONE);
}

void rc4_cleanup(struct rc4_data *rc4_data)
{
	mprotect(rc4_data->base, _RC4_DATA_SIZE, PROT_WRITE | PROT_READ);
	memset(rc4_data->base, 0, _RC4_DATA_SIZE);
	mprotect(rc4_data->base, _RC4_DATA_SIZE, PROT_NONE);
	munmap(rc4_data->base, _RC4_DATA_SIZE);
}
