#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "cli.h"
#include "logic.h"
#include "fsize.h"
#include "image.h"
#include "rand_str.h"
#include "recurse_dirs.h"
#include "rc4/rc4.h"
#include "main.h"

#define NUM_WORKERS 5
#define SEQUENTIAL_LIMIT 5
#define EXIT_SIG(sig) (128 + sig)
#define IO_BUF_SIZE 16384
#define SALT_LEN 16

#include <sys/stat.h>

static volatile sig_atomic_t is_interrupted = false;

struct stats {
	size_t files_processed;
	double total_time_us;
};

void sigsegv_handler(int sig)
{
	is_interrupted = true;
	(void)fprintf(stderr, "caught %d\n", sig);
	exit(EXIT_SIG(sig));
}

void sigint_handler(int)
{
	is_interrupted = true;
}

pthread_mutex_t logfile_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t curr_file_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stats_mtx = PTHREAD_MUTEX_INITIALIZER;


int main(int argc, char *argv[])
{
	if (signal(SIGINT, sigint_handler) == SIG_ERR) {
		perror("sigint handler");
		return EXIT_FAILURE;
	}

	struct cli cli;
	if (parse_cli(argc, argv, &cli) != 0) {
		return EXIT_FAILURE;
	}
	struct app_config cfg = { .sequential_limit = SEQUENTIAL_LIMIT,
							  .max_display_filename_len = 1024,
							  .io_buf_size = IO_BUF_SIZE };
	int cmd_return_code;
	switch (cli.cmd) {
	case CLI_ADD:
		srand(time(NULL));
		cmd_return_code = cmd_add(&cli, cfg);
		break;
	case CLI_LIST:
		cmd_return_code = cmd_list(&cli, cfg);
		break;
	case CLI_GET:
		cmd_return_code = cmd_get(&cli, cfg);
		break;
	};

	if (is_interrupted) {
		printf("Interrupted\n");
		return EXIT_SIG(SIGINT);
	}

	if (cmd_return_code == 0)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

void write_log(FILE *logfile, char *filename, char *msg, pthread_mutex_t *mtx)
{
	if (pthread_mutex_trylock(mtx) != 0) {
		// (void)fprintf(stderr, "trylock write_log\n");
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
	uint8_t *buf = malloc(IO_BUF_SIZE);
	while (1) {
		if (is_interrupted) {
			break;
		}

		char *filepath = recurse_dirs_next(args->files_q);
		if (filepath == NULL) {
			break;
		}
		write_log(args->logfile, filepath, "start processing", &logfile_mtx);

		FILE *src_fp = fopen(filepath, "rb");
		if (src_fp == NULL) {
			perror(filepath);
			write_log(args->logfile, filepath, "error opening file",
					  &logfile_mtx);
			free(filepath);
			continue;
		}

		// TODO: large files
		long src_filesize = fsize(src_fp);
		if (src_filesize < 0) {
			(void)fprintf(stderr, "%s: failed to read file size", filepath);
			free(filepath);
			(void)fclose(src_fp);
			continue;
		}
		size_t name_len = strlen(filepath);
		struct img_file_meta file_meta = {
			.len = src_filesize,
			.name_len = name_len,
		};

		uint8_t *salted_key = malloc(args->keylen + SALT_LEN);
		rand_str_gen((char *)file_meta.salt, SALT_LEN);
		memcpy(salted_key, args->key, args->keylen);
		memcpy(salted_key + args->keylen, file_meta.salt, SALT_LEN);
		struct rc4_data rc4_data = {};
		rc4_init(&rc4_data, salted_key, args->keylen + SALT_LEN);

		size_t img_filesize = IMG_META_SIZE + name_len + (size_t)src_filesize;

		pthread_mutex_lock(&curr_file_mtx);
		long page_size = sysconf(_SC_PAGESIZE);
		off_t map_offset = (args->curr_offset / page_size) * page_size;
		size_t delta = args->curr_offset - map_offset;
		size_t map_len = delta + img_filesize;
		ftruncate(args->img_fd, map_offset + map_len);

		char *const img_map = mmap(NULL, map_len, PROT_READ | PROT_WRITE,
								   MAP_SHARED, args->img_fd, map_offset);
		args->curr_offset += img_filesize;
		pthread_mutex_unlock(&curr_file_mtx);

		if (img_map == MAP_FAILED) {
			(void)fprintf(stderr, "%s: mmap: %s\n", filepath, strerror(errno));
			free(filepath);
			free(salted_key);
			rc4_cleanup(&rc4_data);
			(void)fclose(src_fp);
			continue;
		}

		write_log(args->logfile, filepath, "writing metadata", &logfile_mtx);
		char *img = img_map + delta;
		memcpy(img, &file_meta.len, sizeof(file_meta.len));
		img += sizeof(file_meta.len);
		memcpy(img, &file_meta.name_len, sizeof(file_meta.name_len));
		img += sizeof(file_meta.name_len);
		memcpy(img, &file_meta.salt, sizeof(file_meta.salt));
		img += sizeof(file_meta.salt);

		write_log(args->logfile, filepath, "writing filename", &logfile_mtx);
		uint8_t *filepath_u8 = (uint8_t *)filepath;
		// dont write '\0'
		memcpy(img, filepath_u8, strlen(filepath));
		img += name_len;

		write_log(args->logfile, filepath, "writing contents", &logfile_mtx);
		size_t bytes_read;
		while ((bytes_read = fread(buf, 1, IO_BUF_SIZE, src_fp)) > 0) {
			rc4_encrypt(&rc4_data, buf, (uint8_t *)img, bytes_read);
			img += bytes_read;
		}
		munmap(img_map, map_len);
		rc4_cleanup(&rc4_data);

		(void)fclose(src_fp);

		write_log(args->logfile, filepath, "done writing", &logfile_mtx);
		free(filepath);
		free(salted_key);

		struct timespec file_end_ts;
		clock_gettime(CLOCK_MONOTONIC, &file_end_ts);

		pthread_mutex_lock(&stats_mtx);
		args->stats->files_processed++;
		pthread_mutex_unlock(&stats_mtx);
	}
	free(buf);
	return NULL;
}

void launch_workers(const size_t num_workers, struct process_file_args *args)
{
	pthread_t *workers = malloc(sizeof(pthread_t) * num_workers);

	for (size_t i = 0; i < num_workers; i++) {
		pthread_create(&workers[i], NULL, (void *(*)(void *))process_file,
					   args);
	}

	for (size_t i = 0; i < num_workers; i++) {
		pthread_join(workers[i], NULL);
	}
}

void run_stats(bool run_parallel, struct process_file_args *args_in)
{
	struct process_file_args *args = args_in;
	struct stats stats = { .files_processed = 0, .total_time_us = 0.0 };
	args->stats = &stats;
	struct timespec start_ts;
	clock_gettime(CLOCK_MONOTONIC, &start_ts);

	size_t num_workers = run_parallel ? NUM_WORKERS : 1;
	launch_workers(num_workers, args);

	struct timespec end_ts;
	clock_gettime(CLOCK_MONOTONIC, &end_ts);

	double total_time_us = (double)(end_ts.tv_sec - start_ts.tv_sec) * 1e6 +
						   (double)(end_ts.tv_nsec - start_ts.tv_nsec) / 1e3;
	args->stats->total_time_us = total_time_us;

	printf("=== STATISTICS ===\n");
	printf("Workers: %zu\n", num_workers);
	printf("Total time: %.0f us\n", total_time_us);
	printf("Files processed: %zu\n", args->stats->files_processed);
	if (args->stats->files_processed > 0) {
		printf("Avg time per file: %.0f us\n",
			   total_time_us / (double)args->stats->files_processed);
	}
	printf("==================\n");
}
