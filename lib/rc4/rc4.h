#ifndef __RC4_ENCRYPT_H__
#define __RC4_ENCRYPT_H__

#include <pthread.h>
#include <stdint.h>

struct rc4_data
{
    union
    {
        struct
        {
            uint8_t *s;
            uint8_t *i;
            uint8_t *j;
        };
        void *base;
    };
};

void rc4_encrypt(struct rc4_data *rc4_data, const uint8_t *src, uint8_t *dst,
                 size_t len);

void rc4_init(struct rc4_data *rc4_data, const uint8_t *key,
              size_t keylen);

void rc4_cleanup(struct rc4_data *rc4_data);

#endif // __RC4_ENCRYPT_H__
