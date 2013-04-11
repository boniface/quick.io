/**
 * Defines the functions used by QEV's internal work queue
 * @file qev_wqueue.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

/**
 * Forward-declaration of the queue type. This should be defined in the platform-
 * specific files. This is only used ever as a pointer.
 */
typedef struct qev_wqueue_s qev_wqueue_t;

/**
 * The callback function for freeing the items added to the queue.
 */
typedef void (*qev_wqueue_free_fn)(void *item);

/**
 * The tick type
 */
typedef guint64 tick_id_t;

/**
 * Creates a new, locking work queue.
 *
 * @return A new work queue
 */
qev_wqueue_t* qev_wqueue_init(qev_wqueue_free_fn free_fn);

/**
 * Tear down a work queue.
 *
 * @param wq The work queue to be destroyed.
 */
void qev_wqueue_free(qev_wqueue_t *wq);

/**
 * Put an item into the work queue.
 *
 * @param wq The work queue to add the item to.
 * @param item The item to be added.
 */
void qev_wqueue_add(qev_wqueue_t *wq, void *item);

/**
 * Registers a new thread with the work queue
 *
 * @return The thread's QEV id: this must be passed to qev_wqueue_tick()
 */
tick_id_t qev_wqueue_register(qev_wqueue_t *wq);

/**
 * Indicates that a thread has finished running one iteration of its event loop
 * and is about to go back for another iteration.
 *
 * @param id The thread's ID.
 */
void qev_wqueue_tick(qev_wqueue_t *wq, tick_id_t id);

/**
 * A debugging hook: flushes all closed connections immediately, regardless of the
 * consequences.
 *
 * @note You probably only want to use this for test cases.
 */
void qev_wqueue_debug_flush(qev_wqueue_t *wq);
