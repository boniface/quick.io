/**
 * Copyright 2012 Andrew Stone <andrew@clovar.com>
 *
 * This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#define pause() asm volatile("pause" ::: "memory")

qev_lock_t* qev_lock_new() {
	return g_malloc0(sizeof(qev_lock_t));
}

void qev_lock_free(qev_lock_t *lock) {
	g_free(lock);
}

void qev_lock_static_init(qev_lock_t *lock) {
	memset(lock, 0, sizeof(*lock));
}

void qev_lock_read_lock(qev_lock_t *lock) {
	while (TRUE) {
		// Before we're allowed to continue, make sure there are no writers
		if (__sync_bool_compare_and_swap(&lock->writer, 0, 0)) {
			__sync_fetch_and_add(&lock->readers, 1);
			
			// If no writer got a lock after we got a read lock, then we're good
			if (__sync_bool_compare_and_swap(&lock->writer, 0, 0)) {
				break;
			}
			
			// If a writer got a lock, then we have to release our read lock and try again
			__sync_fetch_and_sub(&lock->readers, 1);
			QEV_STATS_INC(qev_lock_read_false_acquire);
		}
		
		QEV_STATS_INC(qev_lock_read_spin);
		g_thread_yield();
	}
}

void qev_lock_read_unlock(qev_lock_t *lock) {
	__sync_fetch_and_sub(&lock->readers, 1);
}

void qev_lock_write_lock(qev_lock_t *lock) {
	// Acquire the single write lock
	while (!__sync_bool_compare_and_swap(&lock->writer, 0, 1)) {
		QEV_STATS_INC(qev_lock_write_spin);
	}
	
	// Wait until all the readers are done
	while (!__sync_bool_compare_and_swap(&lock->readers, 0, 0)) {
		QEV_STATS_INC(qev_lock_write_wait);
	}
}

void qev_lock_write_unlock(qev_lock_t *lock) {
	__sync_bool_compare_and_swap(&lock->writer, 1, 0);
}