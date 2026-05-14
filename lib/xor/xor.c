#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "xor.h"

static uint8_t *global_key_ptr = NULL;
static pthread_mutex_t key_mtx = PTHREAD_MUTEX_INITIALIZER;

void xor_encrypt(const uint8_t *src, uint8_t *dst, size_t len)
{
	pthread_mutex_lock(&key_mtx);
	if (global_key_ptr == NULL) {
		(void)fprintf(stderr, "key is not set\n");
		pthread_mutex_unlock(&key_mtx);
		return;
	}
#ifndef TEST_SIGSEGV
	mprotect(global_key_ptr, 1, PROT_READ);
#endif
	for (size_t i = 0; i < len; i++) {
		dst[i] = src[i] ^ *global_key_ptr;
	}
	mprotect(global_key_ptr, 1, PROT_NONE);
	pthread_mutex_unlock(&key_mtx);
}

void xor_set_key(uint8_t key)
{
	pthread_mutex_lock(&key_mtx);
	if (global_key_ptr == NULL) {
		global_key_ptr = mmap(global_key_ptr, 1, PROT_NONE,
							  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (global_key_ptr == MAP_FAILED) {
			perror("mmap");
			pthread_mutex_unlock(&key_mtx);
			return;
		}
	}
	mprotect(global_key_ptr, 1, PROT_WRITE);
	*global_key_ptr = key;
	mprotect(global_key_ptr, 1, PROT_NONE);
	pthread_mutex_unlock(&key_mtx);
}

void xor_key_cleanup()
{
	pthread_mutex_lock(&key_mtx);
	if (global_key_ptr == NULL) {
		pthread_mutex_unlock(&key_mtx);
		return;
	}
	munmap(global_key_ptr, 1);
	pthread_mutex_unlock(&key_mtx);
}
