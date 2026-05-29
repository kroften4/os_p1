#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>
#include <stddef.h>
#include <pthread.h>

struct process_file_args {
	FILE *logfile;
	struct ts_queue *files_q;
	int img_fd;
	char *key;
	size_t keylen;
	struct stats *stats;
	size_t curr_offset;
};

void *process_file(struct process_file_args *args);
void launch_workers(size_t num_workers, struct process_file_args *args);
void run_stats(bool run_parallel, struct process_file_args *args_in);

#endif // __MAIN_H__
