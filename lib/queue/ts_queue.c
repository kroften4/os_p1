#include <pthread.h>
#include <stdlib.h>
#include "ts_queue.h"

struct ts_queue_node *ts_queue_node_new(void)
{
	struct ts_queue_node *node = malloc(sizeof(struct ts_queue_node));
	node->data = NULL;
	node->prev = NULL;
	node->next = NULL;
	return node;
}

void __ts_queue_data_destructor_noop(void *dummy)
{
	(void)dummy;
}

void __ts_queue_node_destructor(struct ts_queue *q, struct ts_queue_node *node)
{
	q->data_destructor(node->data);
	free(node);
}

struct ts_queue *ts_queue_new(void)
{
	struct ts_queue *q = malloc(sizeof(struct ts_queue));
	q->head = NULL;
	q->tail = NULL;
	q->size = 0;
	q->data_destructor = __ts_queue_data_destructor_noop;
	pthread_mutex_init(&q->mutex, NULL);
	return q;
}

void __ts_queue_destroy_items(struct ts_queue *q)
{
	struct ts_queue_node *node = q->head;
	while (node != NULL) {
		struct ts_queue_node *trash = node;
		node = node->next;
		__ts_queue_node_destructor(q, trash);
	}
}

void ts_queue_destroy(struct ts_queue *q)
{
	__ts_queue_destroy_items(q);

	pthread_mutex_destroy(&q->mutex);
	free(q);
}

bool __ts_queue_is_empty(struct ts_queue *q)
{
	return q->head == NULL;
}

void __ts_queue_add(struct ts_queue *q, struct ts_queue_node *prev,
					struct ts_queue_node *next, struct ts_queue_node *node)
{
	q->size++;
	if (__ts_queue_is_empty(q)) {
		q->head = node;
		q->tail = node;
	} else if (prev == NULL) {
		q->head = node;
		next->prev = node;
		node->next = next;
	} else if (next == NULL) {
		q->tail = node;
		prev->next = node;
		node->prev = prev;
	} else {
		prev->next = node;
		next->prev = node;
		node->prev = prev;
		node->next = next;
	}
}

void __ts_queue_remove_nolock(struct ts_queue *q, struct ts_queue_node *prev,
							  struct ts_queue_node *next)
{
	if (__ts_queue_is_empty(q))
		return;

	struct ts_queue_node *node = NULL;
	if (prev == NULL && next == NULL) {
		node = q->tail;
	}

	q->size--;
	if (prev == NULL)
		q->head = next;
	if (next == NULL)
		q->tail = prev;

	if (prev != NULL) {
		node = prev->next;
		prev->next = next;
	}
	if (next != NULL) {
		node = next->prev;
		next->prev = prev;
	}

	__ts_queue_node_destructor(q, node);
}

void ts_queue_remove(struct ts_queue *q, struct ts_queue_node *prev,
					 struct ts_queue_node *next)
{
	pthread_mutex_lock(&q->mutex);
	__ts_queue_remove_nolock(q, prev, next);
	pthread_mutex_lock(&q->mutex);
}

void __ts_queue_enqueue_nolock(struct ts_queue *q, void *item)
{
	struct ts_queue_node *new_node = ts_queue_node_new();
	new_node->data = item;
	__ts_queue_add(q, q->tail, NULL, new_node);
}

void ts_queue_enqueue(struct ts_queue *q, void *item)
{
	pthread_mutex_lock(&q->mutex);
	__ts_queue_enqueue_nolock(q, item);
	pthread_mutex_unlock(&q->mutex);
}

void __ts_queue_dequeue_nolock(struct ts_queue *q)
{
	if (__ts_queue_is_empty(q))
		return;
	__ts_queue_remove_nolock(q, NULL, q->head->next);
}

void ts_queue_dequeue(struct ts_queue *q)
{
	pthread_mutex_lock(&q->mutex);
	__ts_queue_dequeue_nolock(q);
	pthread_mutex_unlock(&q->mutex);
}
