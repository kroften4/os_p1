#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include "queue/ts_queue.h"
#include "cli.h"
#include "fsize.h"
#include "recurse_dirs.h"
#include "rc4/rc4.h"
#include "rand_str.h"

#define NUM_WORKERS 5
#define SEQUENTIAL_LIMIT 5
#define EXIT_SIG(sig) (128 + sig)
#define IO_BUF_SIZE 16384
#define SALT_LEN 16
#define MAX_NAME_LEN 4096

#include <sys/stat.h>

static volatile sig_atomic_t is_interrupted = false;

enum run_mode { MODE_SEQUENTIAL, MODE_PARALLEL };

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
	struct ts_queue *files_q;
	int img_fd;
	char *key;
	size_t keylen;
	struct stats *stats;
	size_t curr_offset;
};

struct img_file_meta {
	uint32_t len;
	uint32_t name_len;
	uint8_t salt[SALT_LEN];
};

void *process_file(struct process_file_args *args);
void write_log(FILE *logfile, char *filename, char *msg, pthread_mutex_t *mtx);
void launch_workers(enum run_mode mode, struct process_file_args *args);
void run_stats(enum run_mode mode, struct process_file_args *args_in);

int main(int argc, char *argv[])
{
	// if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
	// 	perror("sigsegv handler");
	// 	return EXIT_FAILURE;
	// }
	if (signal(SIGINT, sigint_handler) == SIG_ERR) {
		perror("sigint handler");
		return EXIT_FAILURE;
	}

	struct cli cli;
	if (parse_cli(argc, argv, &cli) != 0) {
		return EXIT_FAILURE;
	}
	switch (cli.cmd) {
	case CLI_ADD: {
		srand(time(NULL));

		struct ts_queue *q =
			recurse_dirs_init(cli.add.entries, cli.add.entries_amount);

		FILE *log_fp = fopen("log.txt", "w");

		char *node_names[SEQUENTIAL_LIMIT] = {};
		size_t file_amount = 0;
		for (; file_amount < SEQUENTIAL_LIMIT; file_amount++) {
			char *node_name = recurse_dirs_next(q);
			if (node_name == NULL) {
				break;
			}
			node_names[file_amount] = node_name;
		}
		for (size_t i = 0; i < file_amount; i++) {
			ts_queue_enqueue(q, node_names[i]);
		}
		enum run_mode mode =
			(file_amount >= SEQUENTIAL_LIMIT) ? MODE_PARALLEL : MODE_SEQUENTIAL;

		int img_fd =
			open(cli.add.img_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (img_fd == -1) {
			perror("open");
			(void)fprintf(stderr, "img file: %s", cli.add.img_filename);
			break;
		}
		char buf[IO_BUF_SIZE];
		int bytesread;
		off_t curr_offset = 0;
		struct img_file_meta meta = {};
		off_t meta_size =
			sizeof(meta.len) + sizeof(meta.name_len) + sizeof(meta.salt);
		while ((bytesread = read(img_fd, buf, IO_BUF_SIZE)) >= meta_size) {
			memcpy(&meta.len, buf, sizeof(meta.len));
			memcpy(&meta.name_len, buf + sizeof(meta.len),
				   sizeof(meta.name_len));
			memcpy(&meta.salt, buf + sizeof(meta.len) + sizeof(meta.name_len),
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
					break;
				}
			}
			curr_offset += meta_size + meta.len + meta.name_len;
			lseek(img_fd, curr_offset, SEEK_SET);
		}

		struct process_file_args args = {
			.files_q = q,
			.img_fd = img_fd,
			.curr_offset = curr_offset,
			.logfile = log_fp,
			.key = cli.add.key,
			.keylen = strlen(cli.add.key),
		};
		run_stats(mode, &args);

		recurse_dirs_cleanup(q);
		close(img_fd);

		break;
	}
	case CLI_LIST: {
		int img_fd = open(cli.list.img_filename, O_RDWR, 0);
		if (img_fd == -1) {
			perror("open");
			(void)fprintf(stderr, "img file: %s", cli.list.img_filename);
			break;
		}
		char buf[IO_BUF_SIZE];
		int bytesread;
		off_t curr_offset = 0;
		struct img_file_meta meta = {};
		off_t meta_size =
			sizeof(meta.len) + sizeof(meta.name_len) + sizeof(meta.salt);
		while ((bytesread = read(img_fd, buf, IO_BUF_SIZE)) >= meta_size) {
			memcpy(&meta.len, buf, sizeof(meta.len));
			memcpy(&meta.name_len, buf + sizeof(meta.len),
				   sizeof(meta.name_len));
			memcpy(&meta.salt, buf + sizeof(meta.len) + sizeof(meta.name_len),
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
					break;
				}
			}
			size_t name_len = meta.name_len;
			size_t display_len = name_len;
			if (display_len > MAX_NAME_LEN)
				display_len = MAX_NAME_LEN;
			char *namebuf = malloc(display_len + 4);
			if (namebuf == NULL) {
				curr_offset += meta_size + meta.len + name_len;
				lseek(img_fd, curr_offset, SEEK_SET);
				continue;
			}
			memcpy(namebuf, buf + meta_size, display_len);
			if (name_len > MAX_NAME_LEN) {
				namebuf[display_len] = '.';
				namebuf[display_len + 1] = '.';
				namebuf[display_len + 2] = '.';
				namebuf[display_len + 3] = '\0';
			} else {
				namebuf[display_len] = '\0';
			}
			//TODO: sort alphabetically
			printf("%s (%u bytes)\n", namebuf, meta.len);
			free(namebuf);
			curr_offset += meta_size + meta.len + name_len;
			lseek(img_fd, curr_offset, SEEK_SET);
		}
		break;
	}
	case CLI_GET: {
		int img_fd = open(cli.get.img_filename, O_RDWR, 0);
		if (img_fd == -1) {
			perror("open");
			(void)fprintf(stderr, "img file: %s", cli.get.img_filename);
			break;
		}
		char buf[IO_BUF_SIZE];
		int bytesread;
		off_t curr_offset = 0;
		struct img_file_meta meta = {};
		off_t meta_size =
			sizeof(meta.len) + sizeof(meta.name_len) + sizeof(meta.salt);
		size_t target_len = strlen(cli.get.target_filename);
		while ((bytesread = read(img_fd, buf, IO_BUF_SIZE)) >= meta_size) {
			memcpy(&meta.len, buf, sizeof(meta.len));
			memcpy(&meta.name_len, buf + sizeof(meta.len),
				   sizeof(meta.name_len));
			memcpy(&meta.salt, buf + sizeof(meta.len) + sizeof(meta.name_len),
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
					break;
				}
			}
			if (meta.name_len != target_len) {
				curr_offset += meta_size + meta.len + meta.name_len;
				lseek(img_fd, curr_offset, SEEK_SET);
				continue;
			}
			char *namebuf = malloc(target_len + 1);
			if (namebuf == NULL) {
				break;
			}
			memcpy(namebuf, buf + meta_size, target_len);
			namebuf[target_len] = '\0';
			if (strcmp(cli.get.target_filename, namebuf) == 0) {
				printf("found\n");
				FILE *res_file = fopen(cli.get.out_filename, "w");
				if (res_file == NULL) {
					perror("fopen");
					free(namebuf);
					break;
				}
				off_t offset = curr_offset + meta_size + meta.name_len;
				lseek(img_fd, offset, SEEK_SET);
				size_t total_read = 0;
				size_t bytes_read;

				char salt[SALT_LEN + 1] = {};
				memcpy(salt, meta.salt, sizeof(meta.salt));
				salt[SALT_LEN] = '\0';
				size_t keylen = strlen(cli.get.key);
				char *salted_key = malloc(keylen + SALT_LEN + 1);
				strcpy(salted_key, cli.get.key);
				strcat(salted_key, salt);
				struct rc4_data rc4_data = {};
				rc4_init(&rc4_data, (uint8_t *)salted_key, keylen + SALT_LEN);

				while (total_read < meta.len) {
					bytes_read = read(img_fd, buf, IO_BUF_SIZE);
					if (bytes_read <= 0) {
						break;
					}
					if (total_read + bytes_read > meta.len) {
						bytes_read = meta.len - total_read;
					}
					rc4_encrypt(&rc4_data, (uint8_t *)buf, (uint8_t *)buf,
								bytes_read);
					if (fwrite(buf, 1, bytes_read, res_file) == 0) {
						(void)fprintf(stderr, "fwrite failed\n");
						break;
					}
					total_read += bytes_read;
				}
				free(salted_key);
				free(namebuf);
				(void)fclose(res_file);
				break;
			}
			free(namebuf);
			curr_offset += meta_size + meta.len + meta.name_len;
			lseek(img_fd, curr_offset, SEEK_SET);
		}
		break;
	}
	};

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
	uint8_t *buf = malloc(IO_BUF_SIZE);
	while (1) {
		if (is_interrupted) {
			break;
		}

		char *filepath = recurse_dirs_next(args->files_q);
		if (filepath == NULL) {
			break;
		}
		(void)fprintf(stderr, "filename: %s\n", filepath);

		FILE *src_fp = fopen(filepath, "rb");
		if (src_fp == NULL) {
			perror(filepath);
			write_log(args->logfile, filepath, "error opening file",
					  &logfile_mtx);
			free(filepath);
			continue;
		}

		// TODO: large files
		int src_filesize = fsize(src_fp);
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
		char salt[SALT_LEN + 1] = {};
		rand_str_gen(salt, SALT_LEN);
		memcpy(file_meta.salt, salt, sizeof(file_meta.salt));
		char *salted_key = malloc(args->keylen + SALT_LEN + 1);
		strcpy(salted_key, args->key);
		strcat(salted_key, salt);
		struct rc4_data rc4_data = {};
		rc4_init(&rc4_data, (uint8_t *)salted_key, args->keylen + SALT_LEN);

		size_t meta_size = sizeof(file_meta.len) + sizeof(file_meta.name_len) +
						   sizeof(file_meta.salt);
		size_t img_filesize = meta_size + name_len + (size_t)src_filesize;

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
		//FIXME: dont write null terminator
		strcpy(img, filepath);
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

void run_stats(enum run_mode mode, struct process_file_args *args_in)
{
	struct process_file_args *args = args_in;
	struct stats stats = { .files_processed = 0, .total_time_us = 0.0 };
	args->stats = &stats;
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
}
