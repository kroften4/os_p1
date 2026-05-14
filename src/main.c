#include <signal.h>
#include <libgen.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include "fsize.h"
#include "xor/xor.h"

#define NUM_WORKERS 16
#define SEQUENTIAL_LIMIT 5
#define EXIT_SIG(sig) (128 + sig)
#define IO_BUF_SIZE 16384

#include <sys/stat.h>

static volatile sig_atomic_t is_interrupted = false;

enum run_mode { MODE_UNSPECIFIED, MODE_SEQUENTIAL, MODE_PARALLEL };

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

struct process_file_args {
	FILE *logfile;
	char **filenames;
	char *out_dir;
	size_t file_amount;
	size_t *curr_file_idx;
	uint8_t key;
	struct stats *stats;
};

void *process_file(struct process_file_args *args);
void write_log(FILE *logfile, char *filename, char *msg, pthread_mutex_t *mtx);
void launch_workers(enum run_mode mode, struct process_file_args *args);
struct stats run_stats(enum run_mode mode, struct process_file_args args_in);

int main(int argc, char *argv[])
{
	if (signal(SIGSEGV, sigint_handler) == SIG_ERR) {
		perror("sigint handler");
		return EXIT_FAILURE;
	}
	if (signal(SIGINT, sigint_handler) == SIG_ERR) {
		perror("sigint handler");
		return EXIT_FAILURE;
	}
	enum run_mode mode = MODE_UNSPECIFIED;

	static struct option long_options[] = {
		{ "mode", required_argument, 0, 'm' }, { 0, 0, 0, 0 }
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "m:", long_options, NULL)) != -1) {
		switch (opt) {
		case 'm':
			if (strcmp(optarg, "sequential") == 0) {
				mode = MODE_SEQUENTIAL;
			} else if (strcmp(optarg, "parallel") == 0) {
				mode = MODE_PARALLEL;
			} else {
				(void)fprintf(stderr, "Unknown mode: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		default:
			(void)fprintf(
				stderr,
				"Usage: %s --mode=<sequential|parallel> <file_1> [... <file_n>] <out_dir> <key>\n",
				argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (argc < 4) {
		printf(
			"Usage: %s --mode=<sequential|parallel> <file_1> [... <file_n>] <out_dir> <key>\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	int file_arg_idx = optind;
	int out_dir_idx = argc - 2;
	int key_idx = argc - 1;

	const size_t file_amount = (size_t)(out_dir_idx - file_arg_idx);
	size_t next_file_idx = 0;

	char *out_dir = argv[out_dir_idx];
	struct stat out_dir_stat;
	if (stat(out_dir, &out_dir_stat) != 0 || !S_ISDIR(out_dir_stat.st_mode)) {
		(void)fprintf(stderr, "Error: %s is not a directory\n", out_dir);
		return EXIT_FAILURE;
	}

	uint8_t key = argv[key_idx][0];
	xor_set_key(key);

	FILE *log_fp = fopen("log.txt", "w");

	char **filenames = argv + file_arg_idx;

	if (mode == MODE_UNSPECIFIED) {
		mode = (file_amount > SEQUENTIAL_LIMIT) ? MODE_PARALLEL :
												  MODE_SEQUENTIAL;
	}

	struct process_file_args args = {
		.file_amount = file_amount,
		.filenames = filenames,
		.out_dir = out_dir,
		.curr_file_idx = &next_file_idx,
		.logfile = log_fp,
		.key = key,
	};

	printf("~~~ PRE-CACHE ~~~~\n");
	run_stats(MODE_PARALLEL, args);
	printf("~~~~~~~~~~~~~~~~~~\n\n");
	run_stats(mode, args);
	if (mode == MODE_PARALLEL) {
		run_stats(MODE_SEQUENTIAL, args);
	} else {
		run_stats(MODE_PARALLEL, args);
	}
	xor_key_cleanup();

	if (is_interrupted) {
		printf("Interrupted\n");
		return EXIT_SIG(SIGINT);
	}

	return EXIT_SUCCESS;
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
	char *filepath = "";
	uint8_t *buf = malloc(IO_BUF_SIZE);
	while (1) {
		if (is_interrupted) {
			break;
		}
		if (pthread_mutex_trylock(&curr_file_mtx) != 0) {
			write_log(args->logfile, filepath, "trylock failed", &logfile_mtx);
		}
		write_log(args->logfile, filepath, "starting write", &logfile_mtx);
		{
			if (*args->curr_file_idx >= args->file_amount) {
				pthread_mutex_unlock(&curr_file_mtx);
				break;
			}
			filepath = args->filenames[*args->curr_file_idx];
			struct stat path_stat;
			if (stat(filepath, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
				(void)fprintf(stderr, "Skipping directory: %s\n", filepath);
				write_log(args->logfile, filepath, "skipped directory",
						  &logfile_mtx);
				*args->curr_file_idx += 1;
				continue;
			}
			// (void)fprintf(stderr, "filename: %s\n", filepath);
			*args->curr_file_idx += 1;
		}
		pthread_mutex_unlock(&curr_file_mtx);
		write_log(args->logfile, filepath, "mutex unlocked", &logfile_mtx);
		FILE *src_fp = fopen(filepath, "rb");
		if (src_fp == NULL) {
			perror(filepath);
			write_log(args->logfile, filepath, "error opening file",
					  &logfile_mtx);
			continue;
		}

		char *filename = basename(filepath);
		size_t full_path_len = strlen(args->out_dir) + strlen(filename) + 2;
		char *full_path = malloc(full_path_len);
		(void)snprintf(full_path, full_path_len, "%s/%s", args->out_dir,
					   filename);
		FILE *dst_fp = fopen(full_path, "wb");
		if (dst_fp == NULL) {
			perror(full_path);
			write_log(args->logfile, filepath, "error writing file",
					  &logfile_mtx);
			free(full_path);
			(void)fclose(src_fp);
			continue;
		}

		size_t bytes_read;
		while ((bytes_read = fread(buf, 1, IO_BUF_SIZE, src_fp)) > 0) {
			xor_encrypt(buf, buf, bytes_read);
			(void)fwrite(buf, 1, bytes_read, dst_fp);
		}

		(void)fclose(src_fp);
		(void)fclose(dst_fp);
		free(full_path);

		write_log(args->logfile, filename, "done writing", &logfile_mtx);

		struct timespec file_end_ts;
		clock_gettime(CLOCK_MONOTONIC, &file_end_ts);

		pthread_mutex_lock(&stats_mtx);
		args->stats->files_processed++;
		pthread_mutex_unlock(&stats_mtx);
	}
	free(buf);
	return NULL;
}

void launch_workers(enum run_mode mode, struct process_file_args *args)
{
	size_t num_workers = (mode == MODE_SEQUENTIAL) ? 1 : NUM_WORKERS;
	pthread_t workers[NUM_WORKERS];

	for (size_t i = 0; i < num_workers; i++) {
		pthread_create(&workers[i], NULL, (void *(*)(void *))process_file,
					   args);
	}

	for (size_t i = 0; i < num_workers; i++) {
		pthread_join(workers[i], NULL);
	}
}

struct stats run_stats(enum run_mode mode, struct process_file_args args_in)
{
	struct process_file_args *args = &args_in;
	struct stats stats = { .files_processed = 0, .total_time_us = 0.0 };
	args->stats = &stats;
	size_t next_file_idx = 0;
	args->curr_file_idx = &next_file_idx;
	struct timespec start_ts;
	clock_gettime(CLOCK_MONOTONIC, &start_ts);

	launch_workers(mode, args);

	struct timespec end_ts;
	clock_gettime(CLOCK_MONOTONIC, &end_ts);

	double total_time_us = (double)(end_ts.tv_sec - start_ts.tv_sec) * 1e6 +
						   (double)(end_ts.tv_nsec - start_ts.tv_nsec) / 1e3;
	args->stats->total_time_us = total_time_us;

	printf("=== STATISTICS ===\n");
	printf("Mode: %s\n", (mode == MODE_SEQUENTIAL) ? "sequential" : "parallel");
	printf("Total time: %.0f us\n", total_time_us);
	printf("Files processed: %zu\n", args->stats->files_processed);
	if (args->stats->files_processed > 0) {
		printf("Avg time per file: %.0f us\n",
			   total_time_us / (double)args->stats->files_processed);
	}
	printf("==================\n");

	return *args->stats;
}
