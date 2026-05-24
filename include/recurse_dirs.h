#ifndef __RECURSE_DIRS_H__
#define __RECURSE_DIRS_H__

#include "queue/ts_queue.h"

struct ts_queue *recurse_dirs_init(char **node_names, size_t node_names_len);

char *recurse_dirs_next(struct ts_queue *q);

void recurse_dirs_cleanup(struct ts_queue *q);

#endif // __RECURSE_DIRS_H__
