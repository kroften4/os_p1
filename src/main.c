#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "progress.h"
#include "fsize.h"
#include "xor/xor.h"
#include "queue/ts_queue.h"
#include "buf.h"

#define EXIT_SIG(sig) (128 + sig)

#define IO_BUF_SIZE 4096
#define PROGRESS_BAR_LEN 10
#define PROGRESS_INTERVAL_PERCENT 5

static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

static bool is_eof = false;
static volatile bool is_interrupted = false;

void sigint_handler(int)
{
	is_interrupted = true;
}

struct encrypt_file_args {
	FILE *fp;
	struct ts_queue *q;
	uint8_t key;
};
void *encrypt_file(struct encrypt_file_args *args);

struct write_file_args {
	FILE *fp;
	struct ts_queue *q;
};
void *write_file(struct write_file_args *args);

int main(int argc, char *argv[])
{
	if (signal(SIGINT, sigint_handler) == SIG_ERR) {
		perror("sigint handler");
		return EXIT_FAILURE;
	}
	if (argc != 4) {
		printf("Usage: %s <input file> <output file> <key>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *src_file = argv[1];
	char *dst_file = argv[2];
	uint8_t key = argv[3][0];

	FILE *src_fp = fopen(src_file, "rb");
	if (src_fp == NULL) {
		perror(src_file);
		return EXIT_FAILURE;
	}

	FILE *dst_fp = fopen(dst_file, "wb");
	if (src_fp == NULL) {
		perror(dst_file);
		return EXIT_FAILURE;
	}

	struct ts_queue *queue = ts_queue_new();
	queue->data_destructor = (void (*)(void *))buf_destruct;

	struct encrypt_file_args producer_args = {
		.fp = src_fp,
		.q = queue,
		.key = key,
	};
	pthread_t producer;
	pthread_create(&producer, NULL, (void *(*)(void *))encrypt_file,
				   &producer_args);

	struct write_file_args consumer_args = {
		.fp = dst_fp,
		.q = queue,
	};
	pthread_t consumer;
	pthread_create(&consumer, NULL, (void *(*)(void *))write_file,
				   &consumer_args);

	pthread_join(producer, NULL);
	pthread_join(consumer, NULL);

	if (is_interrupted) {
		printf("Gracefully stopping...\n");
	}

	ts_queue_destroy(queue);

	printf("Closing file descriptors\n");
	bool failure = false;
	if (fclose(src_fp) != 0) {
		perror(src_file);
		failure = true;
	}
	if (fclose(dst_fp) != 0) {
		perror(dst_file);
		failure = true;
	}
	if (failure) {
		return EXIT_FAILURE;
	}

	if (is_interrupted) {
		return EXIT_SIG(SIGINT);
	}

	return EXIT_SUCCESS;
}

void *write_file(struct write_file_args *args)
{
	struct buf *buf = NULL;
	struct ts_queue *q = args->q;
	while (1) {
		pthread_mutex_lock(&q->mutex);
		{
			while (__ts_queue_is_empty(q) && !is_eof && !is_interrupted) {
				pthread_cond_wait(&not_empty, &q->mutex);
			}
			if ((__ts_queue_is_empty(q) && is_eof) || is_interrupted) {
				pthread_mutex_unlock(&q->mutex);
				break;
			}
			buf = q->head->data;
		}
		pthread_mutex_unlock(&q->mutex);

		(void)fwrite(buf->data, sizeof(buf->data[0]), buf->size, args->fp);
		ts_queue_dequeue(q);
		// sleep(1);
	}
	return NULL;
}

void *encrypt_file(struct encrypt_file_args *args)
{
	struct buf *buf = NULL;
	struct ts_queue *q = args->q;
	xor_set_key(args->key);
	int size = fsize(args->fp);
	int written = 0;
	int last_percent = -PROGRESS_INTERVAL_PERCENT;
	int curr_percent = 0;
	while (1) {
		if (is_interrupted) {
			break;
		}

		buf = buf_new(IO_BUF_SIZE);

		size_t len =
			fread(buf->data, sizeof(buf->data[0]), buf->size, args->fp);
		// sleep(1);
		if (len == 0) {
			pthread_mutex_lock(&q->mutex);
			{
				is_eof = true;
				pthread_cond_signal(&not_empty);
			}
			pthread_mutex_unlock(&q->mutex);
			buf_destruct(buf);
			break;
		}
		buf->size = len;
		xor_encrypt(buf->data, buf->data, buf->size);
		written += len;

		curr_percent = (float)written / size * 100;
		if (curr_percent - last_percent >= PROGRESS_INTERVAL_PERCENT) {
			printf("\r[");
			progress_print_bar(written, size, PROGRESS_BAR_LEN, NULL);
			printf("]");
			printf(" %d%%", curr_percent);
			(void)fflush(stdout);
			last_percent = curr_percent;
		}

		pthread_mutex_lock(&q->mutex);
		{
			__ts_queue_enqueue_nolock(q, buf);
			pthread_cond_signal(&not_empty);
		}
		pthread_mutex_unlock(&q->mutex);
	}
	printf("\n");

	return NULL;
}
