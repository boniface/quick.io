/**
 * Provides a fixed-size, lock-free queue.
 *
 * @file ring_queue.h
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of ring-queue and is released under the
 * MIT License: http://opensource.org/licenses/MIT
 */

#include <stdint.h>

/**
 * The data type of the ring queue: none of the fields should be modified
 * externally.
 */
typedef struct ringq_s ringq_t;

/**
 * Creates a new ring queue of size.
 *
 * @param size The maximum size of the ring queue.
 *
 * @return The ring queue.
 */
ringq_t* ringq_new(uint32_t size);

/**
 * Adds a pointer to the queue.
 *
 * @param q A pointer to your ring queue.
 * @param data The pointer to be added to the queue.
 *
 * @return 0 on success.
 * @return -1 if the queue is full.
 */
int ringq_add(ringq_t *q, void *data);

/**
 * Gets the next value in the queue.
 *
 * @param q A pointer to your ring queue.
 *
 * @return The next pointer in the queue.
 * @return NULL if the queue is empty.
 */
void* ringq_next(ringq_t *q);

/**
 * Destroys a ring queue.
 *
 * @attention Make sure your queue is empty before calling this,
 * or you are going to get some memory leaks.
 *
 * @param q A pointer to your ring queue.
 */
void ringq_free(ringq_t *q);