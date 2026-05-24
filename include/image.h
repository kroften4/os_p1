#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <fcntl.h>
#include <stdint.h>

#define SALT_LEN 16

struct img_file_meta
{
    uint32_t len;
    uint32_t name_len;
    uint8_t salt[SALT_LEN];
};

struct img_file
{
    struct img_file_meta meta;
    off_t name_off;
    off_t contents_off;
};

#define IMG_META_SIZE sizeof(struct img_file_meta)

struct img_iter_val
{
    struct img_file_meta meta;
    off_t file_offset;
};
struct img_iter
{
    char *buf;
    off_t curr_offset;
    int fd;
};

void img_iter_init(struct img_iter *iter, int image_fd);

int img_iter_next(struct img_iter *iter, struct img_file *value);

void img_iter_cleanup(struct img_iter *iter);

bool find_filename(int img_fd, char *filename, struct img_file *file_data);

#endif // __IMAGE_H__
