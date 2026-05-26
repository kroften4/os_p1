#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "queue/ts_queue.h"

static void strip_trailing_slashes(char *path)
{
	size_t len = strlen(path);
	while (len > 1 && path[len - 1] == '/')
		path[--len] = '\0';
}

struct ts_queue *recurse_dirs_init(char **node_names, size_t node_names_len)
{
	struct ts_queue *q = ts_queue_new();
	for (size_t i = 0; i < node_names_len; i++) {
		char *node_name = strdup(node_names[i]);
		strip_trailing_slashes(node_name);
		ts_queue_enqueue(q, node_name);
	}
	return q;
}

char *recurse_dirs_next(struct ts_queue *q)
{
	char *node_name = NULL;
	while ((node_name = ts_queue_dequeue(q)) != NULL) {
		strip_trailing_slashes(node_name);
		struct stat st = {};
		if (stat(node_name, &st) != 0) {
			(void)fprintf(stderr, "stat: %s: %s\n", node_name, strerror(errno));
			free(node_name);
			continue;
		}
		if (S_ISREG(st.st_mode)) {
			return node_name;
		}
		if (!S_ISDIR(st.st_mode)) {
			(void)fprintf(
				stderr,
				"recurse_dirs_next: %s: not a regular file or a directory\n",
				node_name);
			free(node_name);
			continue;
		}
		DIR *dir = opendir(node_name);
		if (dir == NULL) {
			(void)fprintf(stderr, "opendir: %s: %s\n", node_name,
						  strerror(errno));
			free(node_name);
			continue;
		}
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0)
				continue;
			// node_name + '/' + entry->d_name + '\0'
			size_t new_node_size =
				strlen(node_name) + strlen(entry->d_name) + 2;
			char *new_node_name = malloc(new_node_size);
			(void)snprintf(new_node_name, new_node_size, "%s/%s", node_name,
						   entry->d_name);
			ts_queue_enqueue(q, new_node_name);
		}
		closedir(dir);
		free(node_name);
	}
	return NULL;
}

static void node_name_destructor(void *node_name) {
    free(node_name);
}

void recurse_dirs_cleanup(struct ts_queue *q)
{
    q->data_destructor = node_name_destructor;
    ts_queue_destroy(q);
}
