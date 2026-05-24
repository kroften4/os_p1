#include "logic.h"
#include "queue/ts_queue.h"
#include "recurse_dirs.h"
#include "cli.h"
#include "rc4/rc4.h"
#include "image.h"
#include "main.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int cmd_add(struct cli *cli, const struct app_config cfg)
{
	struct ts_queue *q =
		recurse_dirs_init(cli->add.entries, cli->add.entries_amount);

	FILE *log_fp = fopen("log.txt", "w");

	char *node_names[cfg.sequential_limit] = {};
	size_t file_amount = 0;
	for (; file_amount < cfg.sequential_limit; file_amount++) {
		char *node_name = recurse_dirs_next(q);
		if (node_name == NULL) {
			break;
		}
		node_names[file_amount] = node_name;
	}
	for (size_t i = 0; i < file_amount; i++) {
		ts_queue_enqueue(q, node_names[i]);
	}
	bool run_parallel = file_amount >= cfg.sequential_limit;

	int img_fd =
		open(cli->add.img_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (img_fd == -1) {
		perror("open");
		(void)fprintf(stderr, "img file: %s", cli->add.img_filename);
		return -1;
	}
	struct img_iter img_iter = {};
	img_iter_init(&img_iter, img_fd);
	struct img_file img_file = {};
	while (img_iter_next(&img_iter, &img_file) == 0) {
	}
	img_iter_cleanup(&img_iter);

	struct process_file_args args = {
		.files_q = q,
		.img_fd = img_fd,
		.curr_offset = img_file.contents_off + img_file.meta.len,
		.logfile = log_fp,
		.key = cli->add.key,
		.keylen = strlen(cli->add.key),
	};
	run_stats(run_parallel, &args);

	recurse_dirs_cleanup(q);
	close(img_fd);
	return 0;
}

struct list_entry {
	char *name;
	uint32_t size;
};

static int list_entry_cmp(const void *a, const void *b)
{
	const struct list_entry *ea = a;
	const struct list_entry *eb = b;
	return strcmp(ea->name, eb->name);
}

int cmd_list(struct cli *cli, const struct app_config cfg)
{
	int img_fd = open(cli->list.img_filename, O_RDWR, 0);
	if (img_fd == -1) {
		perror("open");
		(void)fprintf(stderr, "img file: %s", cli->list.img_filename);
		return -1;
	}

	struct img_iter img_iter = {};
	img_iter_init(&img_iter, img_fd);
	struct img_file img_file = {};

	size_t capacity = 64;
	size_t count = 0;
	struct list_entry *entries = malloc(capacity * sizeof(*entries));

	while (img_iter_next(&img_iter, &img_file) == 0) {
		if (count >= capacity) {
			capacity *= 2;
			entries = realloc(entries, capacity * sizeof(*entries));
		}

		uint32_t name_len = img_file.meta.name_len;
		size_t display_len = name_len;
		if (display_len > cfg.max_display_filename_len)
			display_len = cfg.max_display_filename_len;
		char *namebuf = malloc(display_len + 4);
		read(img_fd, namebuf, display_len);
		namebuf[display_len] = '\0';
		if (name_len > cfg.max_display_filename_len) {
			strcat(namebuf, "...");
		}

		entries[count].name = namebuf;
		entries[count].size = img_file.meta.len;
		count++;
	}
	img_iter_cleanup(&img_iter);

	qsort(entries, count, sizeof(*entries), list_entry_cmp);

	for (size_t i = 0; i < count; i++) {
		printf("%s (%u bytes)\n", entries[i].name, entries[i].size);
		free(entries[i].name);
	}
	free(entries);

	return 0;
}

int cmd_get(struct cli *cli, const struct app_config cfg)
{
	int img_fd = open(cli->get.img_filename, O_RDWR, 0);
	if (img_fd == -1) {
		perror("open");
		(void)fprintf(stderr, "img file: %s", cli->get.img_filename);
		return -1;
	}
	struct img_file img_file = {};
	if (!find_filename(img_fd, cli->get.target_filename, &img_file)) {
		(void)fprintf(stderr, "%s not found in image",
					  cli->get.target_filename);
		close(img_fd);
		return -1;
	}

	FILE *res_file = fopen(cli->get.out_filename, "w");
	if (res_file == NULL) {
		perror("fopen");
		close(img_fd);
		return -1;
	}

	size_t keylen = strlen(cli->get.key);
	uint8_t *salted_key = malloc(keylen + SALT_LEN);
	memcpy(salted_key, cli->get.key, keylen);
	memcpy(salted_key + keylen, img_file.meta.salt, sizeof(img_file.meta.salt));
	struct rc4_data rc4_data = {};
	rc4_init(&rc4_data, salted_key, keylen + SALT_LEN);

	char buf[cfg.io_buf_size];
	size_t total_read = 0;
	lseek(img_fd, img_file.contents_off, SEEK_SET);

	while (total_read < img_file.meta.len) {
		size_t bytes_read = read(img_fd, buf, cfg.io_buf_size);
		if (bytes_read <= 0) {
			break;
		}
		if (total_read + bytes_read > img_file.meta.len) {
			bytes_read = img_file.meta.len - total_read;
		}
		rc4_encrypt(&rc4_data, (uint8_t *)buf, (uint8_t *)buf, bytes_read);
		if (fwrite(buf, 1, bytes_read, res_file) == 0) {
			(void)fprintf(stderr, "fwrite failed\n");
			break;
		}
		total_read += bytes_read;
	}
	free(salted_key);
	(void)fclose(res_file);
	close(img_fd);
	return 0;
}
