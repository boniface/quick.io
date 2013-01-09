/**
 * ring_queue.c
 * Copyright 2012 Andrew Stone <andrew@clovar.com>
 *
 * This file is part of ring-queue and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include <stdlib.h>
#include <string.h>

#include "ring_queue.h"
#include "../../src/qio.h"

#define RINGQ_LOCK(name) \
	while (!__sync_bool_compare_and_swap(&q->lock_##name, 0, 1)) { \
		asm volatile("pause" ::: "memory"); \
		STATS_INC(ring_queue_spin_##name) \
	} \
	__sync_synchronize();

#define RINGQ_UNLOCK(name) \
	__sync_synchronize(); \
	__sync_bool_compare_and_swap(&q->lock_##name, 1, 0);

// Forward declaration so the *next pointer works
typedef struct ringq_elem_s ringq_elem_t;

struct ringq_elem_s {
	void *data;
	ringq_elem_t *next;
};

struct ringq_s {
	/**
	 * The start of the original malloc
	 */
	void *_mallocd;
	
	/**
	 * Where elements are read from.  Points to a FULL element, the next to be read
	 * from.
	 */
	ringq_elem_t *head;
	
	/**
	 * Where elements are added to.  Points to an EMPTY element, the next one to be
	 * written to.
	 */
	ringq_elem_t *tail;
	
	/**
	 * The unfortunately required spin locks
	 */
	int lock_head;
	int lock_tail;
};

ringq_t* ringq_new(uint32_t size) {
	ringq_t *q = malloc(sizeof(*q));
	memset(q, 0, sizeof(*q));
	
	// When q->head = q->tail, the list is considered empty, but this case can also
	// happen when the queue is full, so we give ourselves a padding element for
	// accounting
	size += 1;
	
	ringq_elem_t *curr = q->_mallocd = q->head = q->tail = malloc(sizeof(*q->head) * size);
	for (uint32_t i = 1; i < size; i++) {
		curr->next = curr + 1;
		curr = curr->next;
	}
	
	curr->next = q->head;
	
	return q;
}

int ringq_add(ringq_t *q, void *data) {
	int ret = -1;
	
	RINGQ_LOCK(tail);
	
	if (q->tail->next != q->head) {
		ret = 0;
		q->tail->data = data;
		q->tail = q->tail->next;
	}
	
	RINGQ_UNLOCK(tail);
	
	return ret;
}

void* ringq_next(ringq_t *q) {
	void *data = NULL;
	
	RINGQ_LOCK(head);
	
	if (q->head != q->tail) {
	 	data = q->head->data;
		q->head = q->head->next;
	}
	
	RINGQ_UNLOCK(head);
	
	return data;
}

void ringq_free(ringq_t *q) {
	free(q->_mallocd);
	free(q);
}