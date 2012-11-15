/**
 * Copyright 2012 Andrew Stone <andrew@clovar.com>
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

/**
 * The actual list
 */
struct qev_wqueue_s {
	/**
	 * There need to be 2 queues at all times: 1 queue contains whatever is happening
	 * in this tick, and it cannot be touched until the next tick is done to ensure
	 * that nothing out of the tick is being added.
	 *
	 * To visualize:
	 *   1) Thread A is working on client 1 and does some long, blocking operation
	 *   2) Thread B waits on its poll, times out, and issues a tick
	 *   3) Thread B gets a notification on client 1 from its poll and begins
	 *      work on him only to block until Thread A finishes
	 *   3) Thread A finishes, adds client 1 to this queue, removes him from polling, and ticks.
	 *   4) Thread B does some work and blocks
	 *   5) At this point, the tick is technically complete, but thread B is now holding
	 *      client 1, so we can't just wipe him out, but we know that no other calls to the poll
	 *      will result in client 1 being referenced anywhere else.
	 *   6) We wait another tick, and then it is safe to free client 1
	 */
	GQueue *queue[2];
	
	/**
	 * The current queue that we are working on.
	 */
	int curr_queue;
	
	/**
	 * The number of threads with access to this list
	 */
	int threads;
	
	/**
	 * The thread mask for testing if all threads have synced for this tick
	 */
	tick_id_t threads_mask;
	
	/**
	 * All of the threads that have checked in on this tick.
	 */
	tick_id_t checked_in;
	
	/**
	 * The free function to be called when elements are to be removed.
	 */
	qev_wqueue_free_fn free_fn;
	
	/**
	 * The lock for the queue
	 */
	GMutex lock;
};

qev_wqueue_t* qev_wqueue_init(qev_wqueue_free_fn free_fn) {
	qev_wqueue_t *wq = g_malloc0(sizeof(*wq));
	
	g_mutex_init(&wq->lock);
	wq->free_fn = free_fn;
	
	for (gsize i = 0; i < G_N_ELEMENTS(wq->queue); i++) {
		wq->queue[i] = g_queue_new();
	}
	
	return wq;
}

void qev_wqueue_add(qev_wqueue_t *wq, void *item) {
	g_mutex_lock(&wq->lock);
	
	g_queue_push_tail(wq->queue[wq->curr_queue], item);
	
	g_mutex_unlock(&wq->lock);
}

tick_id_t qev_wqueue_register(qev_wqueue_t *wq) {
	tick_id_t thread = ((tick_id_t)1) << __sync_fetch_and_add(&wq->threads, 1);
	__sync_or_and_fetch(&wq->threads_mask, thread);
	return thread;
}

void qev_wqueue_tick(qev_wqueue_t *wq, tick_id_t id) {
	// If the current tick is all synced
	GQueue *q = NULL;
	
	g_mutex_lock(&wq->lock);
	
	wq->checked_in |= id;
	if ((wq->checked_in ^ wq->threads_mask) == 0) {
		// Starting a new tick, so no one is checked in
		wq->checked_in = 0;
		
		// Replace the queue currently in our way with a new queue
		// We'll empty out the old queue once we're unlocked
		wq->curr_queue = (wq->curr_queue + 1) % G_N_ELEMENTS(wq->queue);
		q = wq->queue[wq->curr_queue];
		wq->queue[wq->curr_queue] = g_queue_new();
	}
	
	g_mutex_unlock(&wq->lock);
	
	if (q != NULL) {
		void *item;
		while ((item = g_queue_pop_head(q)) != NULL) {
			wq->free_fn(item);
		}
		
		g_queue_clear(q);
	}
}

void qev_wqueue_debug_flush(qev_wqueue_t *wq) {
	g_mutex_lock(&wq->lock);
	
	void *item;
	for (gsize i = 0; i < G_N_ELEMENTS(wq->queue); i++) {
		while ((item = g_queue_pop_head(wq->queue[i])) != NULL) {
			wq->free_fn(item);
		}
	}
	
	g_mutex_unlock(&wq->lock);
}