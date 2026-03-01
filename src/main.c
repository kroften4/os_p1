#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "caesar.h"

#define IO_BUF_SIZE 4096

static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static int full_count = 0;

static bool is_done;

static int fsize(FILE *fp)
{
	int curr = ftell(fp);
	if (fseek(fp, 0, SEEK_END) < 0)
		return -1;
	int res = ftell(fp);
	if (fseek(fp, curr, SEEK_SET) < 0)
		return -1;
	return res;
}

struct encrypt_file_args {
	char *filename;
	uint8_t *bufs[2];
	size_t *buf_sizes[2];
	uint8_t curr_buf_idx;
	uint8_t key;
};
void *encrypt_file(struct encrypt_file_args *args)
{
	uint8_t *buf;
	size_t *buflen;
	FILE *fp = fopen(args->filename, "rb");
	xor_set_key(args->key);
	int size = fsize(fp);
	int written = 0;
	while (1) {
		pthread_mutex_lock(&mtx);
		{
			while (full_count == 2 && is_done) {
				pthread_cond_wait(&not_full, &mtx);
			}
			if (!is_done) {
				pthread_cond_signal(&not_empty);
				pthread_mutex_unlock(&mtx);
				break;
			}
			buf = args->bufs[args->curr_buf_idx];
			buflen = args->buf_sizes[args->curr_buf_idx];
		}
		pthread_mutex_unlock(&mtx);

		size_t len = fread(buf, sizeof(buf[0]), IO_BUF_SIZE, fp);
		// sleep(1);
		if (len == 0) {
			pthread_mutex_lock(&mtx);
			{
				is_done = false;
				pthread_cond_signal(&not_empty);
			}
			pthread_mutex_unlock(&mtx);
			break;
		}
		xor_encrypt(buf, buf, len);
		written += len;
		printf("\r%d/%d", written, size);
		(void)fflush(stdout);

		pthread_mutex_lock(&mtx);
		{
			*buflen = len;
			full_count++;
			pthread_cond_signal(&not_empty);
		}
		pthread_mutex_unlock(&mtx);

		args->curr_buf_idx = 1 - args->curr_buf_idx;
	}

	(void)fclose(fp);
	return NULL;
}

struct write_file_args {
	char *filename;
	uint8_t *bufs[2];
	size_t *buf_sizes[2];
	uint8_t curr_buf_idx;
};
void *write_file(struct write_file_args *args)
{
	uint8_t *buf;
	size_t *buflen;
	FILE *fp = fopen(args->filename, "wb");
	while (1) {
		pthread_mutex_lock(&mtx);
		{
			while (full_count == 0 && is_done) {
				pthread_cond_wait(&not_empty, &mtx);
			}
			if (full_count == 0 && !is_done) {
				pthread_mutex_unlock(&mtx);
				break;
			}
			buf = args->bufs[args->curr_buf_idx];
			buflen = args->buf_sizes[args->curr_buf_idx];
		}
		pthread_mutex_unlock(&mtx);

		(void)fwrite(buf, sizeof(buf[0]), *buflen, fp);
		// sleep(1);

		pthread_mutex_lock(&mtx);
		{
			full_count--;
			pthread_cond_signal(&not_full);
		}
		pthread_mutex_unlock(&mtx);

		args->curr_buf_idx = 1 - args->curr_buf_idx;
	}
	(void)fclose(fp);
	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		printf("Usage: %s <input file> <output file> <key>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *src_file = argv[1];
	char *dst_file = argv[2];
	uint8_t key = argv[3][0];

	uint8_t *buf1 = malloc(IO_BUF_SIZE);
	uint8_t *buf2 = malloc(IO_BUF_SIZE);
	size_t buf1_size = 0;
	size_t buf2_size = 0;

	is_done = true;

	pthread_t producer;
	struct encrypt_file_args producer_args = {
		.filename = src_file,
		.bufs = { buf1, buf2 },
		.buf_sizes = { &buf1_size, &buf2_size },
		.curr_buf_idx = 0,
		.key = key,
	};
	pthread_create(&producer, NULL, (void *(*)(void *))encrypt_file,
		       &producer_args);

	struct write_file_args consumer_args = {
		.filename = dst_file,
		.bufs = { buf1, buf2 },
		.buf_sizes = { &buf1_size, &buf2_size },
		.curr_buf_idx = 0,
	};
	pthread_t consumer;
	pthread_create(&consumer, NULL, (void *(*)(void *))write_file,
		       &consumer_args);

	pthread_join(producer, NULL);
	pthread_join(consumer, NULL);
	printf("\n");
	return EXIT_SUCCESS;
}
