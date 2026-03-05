#ifndef __XOR_ENCRYPT_H__
#define __XOR_ENCRYPT_H__

#include <stddef.h>
#include <stdint.h>

void xor_encrypt(const uint8_t *src, uint8_t *dst, size_t len);

void xor_set_key(uint8_t key);

#endif // __XOR_ENCRYPT_H__
