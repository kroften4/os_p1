#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "image.h"

void img_iter_init(struct img_iter *iter, int image_fd)
{
	iter->curr_offset = 0;
	iter->buf = malloc(IMG_META_SIZE);
	iter->fd = image_fd;
}

int img_iter_next(struct img_iter *iter, struct img_file *value)
{
	lseek(iter->fd, iter->curr_offset, SEEK_SET);
	struct img_file_meta meta = {};
	off_t meta_size = IMG_META_SIZE;

	int bytesread = read(iter->fd, iter->buf, IMG_META_SIZE);
	if (bytesread < meta_size) {
		value = NULL;
		return -1;
	}

	memcpy(&meta.len, iter->buf, sizeof(meta.len));
	memcpy(&meta.name_len, iter->buf + sizeof(meta.len), sizeof(meta.name_len));
	memcpy(&meta.salt, iter->buf + sizeof(meta.len) + sizeof(meta.name_len),
		   sizeof(meta.salt));
	if (meta.len == 0 && meta.name_len == 0) {
		bool zero_salt = true;
		for (size_t i = 0; i < sizeof(meta.salt); i++) {
			if (meta.salt[i] != 0) {
				zero_salt = false;
				break;
			}
		}
		if (zero_salt) {
			value = NULL;
			return -1;
		}
	}
	value->meta = meta;
	value->name_off = iter->curr_offset + meta_size;
	value->contents_off = value->name_off + value->meta.name_len;
	iter->curr_offset += meta_size + meta.len + meta.name_len;
	return 0;
}

void img_iter_cleanup(struct img_iter *iter)
{
	free(iter->buf);
}

bool find_filename(int img_fd, char *filename, struct img_file *file_data)
{
	size_t target_filename_len = strlen(filename);
	//TODO: big name
	char *namebuf = malloc(target_filename_len + 1);

	struct img_iter img_iter = {};
	img_iter_init(&img_iter, img_fd);
	bool found = false;
	while (img_iter_next(&img_iter, file_data) == 0) {
		if (file_data->meta.name_len != target_filename_len) {
			continue;
		}
		//TODO: EOF
		read(img_fd, namebuf, target_filename_len);
		namebuf[target_filename_len] = '\0';
		if (strcmp(filename, namebuf) == 0) {
			found = true;
			break;
		}
	}
	free(namebuf);
	img_iter_cleanup(&img_iter);
	return found;
}
