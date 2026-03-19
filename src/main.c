#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "fsize.h"
#include "xor/xor.h"

#define NUM_WORKERS 3
#define EXIT_SIG(sig) (128 + sig)
#define IO_BUF_SIZE 4096

static volatile bool is_interrupted = false;

void sigint_handler(int)
{
	is_interrupted = true;
}

pthread_mutex_t logfile_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t curr_file_mtx = PTHREAD_MUTEX_INITIALIZER;
struct process_file_args {
	FILE *logfile;
	char **filenames;
	char *out_dir;
	size_t file_amount;
	size_t *curr_file_idx;
	uint8_t key;
};
void *process_file(struct process_file_args *args);

int main(int argc, char *argv[])
{
	if (signal(SIGINT, sigint_handler) == SIG_ERR) {
		perror("sigint handler");
		return EXIT_FAILURE;
	}
	if (argc < 4) {
		printf("Usage: %s <file_1> [... <file_n>] <out_dir> <key>\n", argv[0]);
		return EXIT_FAILURE;
	}

	const size_t file_amount = argc - 3; // comand, out_dir, key
	size_t next_file_idx = 0;

	char *out_dir = argv[argc - 2];
	uint8_t key = argv[argc - 1][0];
	xor_set_key(key);

	FILE *log_fp = fopen("log.txt", "w");

	char **filenames = argv + 1;
	for (size_t i = 0; i < file_amount; i++) {
		printf("%zu: %s\n", i, filenames[i]);
	}

	struct process_file_args args = { .file_amount = file_amount,
									  .filenames = filenames,
									  .out_dir = out_dir,
									  .curr_file_idx = &next_file_idx,
									  .logfile = log_fp,
									  .key = key };

	pthread_t workers[NUM_WORKERS];
	for (size_t i = 0; i < NUM_WORKERS; i++) {
		pthread_create(&workers[i], NULL, (void *(*)(void *))process_file,
					   &args);
	}

	for (size_t i = 0; i < NUM_WORKERS; i++) {
		pthread_join(workers[i], NULL);
	}
	if (is_interrupted) {
		printf("Interrupted\n");
		return EXIT_SIG(SIGINT);
	}

	return EXIT_SUCCESS;
}

void write_log(FILE *logfile, char *filename, char *msg, pthread_mutex_t *mtx)
{
	if (pthread_mutex_trylock(mtx) != 0) {
		(void)fprintf(stderr, "trylock writelog\n");
	}
	char timebuf[64];
	time_t now = time(NULL);
	(void)strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S",
				   localtime(&now));
	(void)fprintf(logfile, "TIME: %s TID: %lu FILE: %s MSG: %s\n", timebuf,
				  pthread_self(), filename, msg);
	pthread_mutex_unlock(mtx);
}

void *process_file(struct process_file_args *args)
{
	char *filename = "";
	uint8_t *buf = NULL;
	while (1) {
		if (is_interrupted) {
			break;
		}
		if (pthread_mutex_trylock(&curr_file_mtx) != 0) {
			(void)fprintf(stderr, "trylock\n");
		}
		// write_log(args->logfile, filename, "starting write", &logfile_mtx);
		(void)fprintf(stderr, "written log\n");
		{
			if (*args->curr_file_idx >= args->file_amount) {
				pthread_mutex_unlock(&curr_file_mtx);
				break;
			}
			filename = args->filenames[*args->curr_file_idx];
			(void)fprintf(stderr, "filename: %s\n", filename);
			*args->curr_file_idx += 1;
		}
		pthread_mutex_unlock(&curr_file_mtx);
		(void)fprintf(stderr, "unlocked\n");
		FILE *src_fp = fopen(filename, "rb");
		if (src_fp == NULL) {
			perror(filename);
			write_log(args->logfile, filename, "error opening file",
					  &logfile_mtx);
			continue;
		}

		// TODO: add max buf size
		size_t buflen = fsize(src_fp);
		(void)fprintf(stderr, "buflen: %zu\n", buflen);
		buf = malloc(buflen);

		(void)fread(buf, sizeof(uint8_t), buflen, src_fp);
		(void)fclose(src_fp);

		xor_encrypt(buf, buf, buflen);

		size_t full_path_len = strlen(args->out_dir) + strlen(filename) + 2;
		char *full_path = malloc(full_path_len);
		(void)snprintf(full_path, full_path_len, "%s/%s", args->out_dir,
					   filename);
		FILE *dst_fp = fopen(full_path, "wb");
		if (dst_fp == NULL) {
			perror(full_path);
			write_log(args->logfile, filename, "error writing file",
					  &logfile_mtx);
			goto cleanup;
		}
		(void)fwrite(buf, sizeof(uint8_t), buflen, dst_fp);

		(void)fclose(dst_fp);

		// write_log(args->logfile, filename, "done writing", &logfile_mtx);
cleanup:
		free(buf);
	}
	return NULL;
}
