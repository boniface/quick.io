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

void qev_lock_read_lock(qev_lock_t *lock) {
	while (TRUE) {
		// First, just optimistically acquire a read lock
		__sync_fetch_and_add(&lock->readers, 1);
		
		// Before we're allowed to continue, make sure there are no writers
		if (__sync_bool_compare_and_swap(&lock->writer, 0, 0)) {
			break;
		} else {
			__sync_fetch_and_sub(&lock->readers, 1);
		}
		
		pause();
	}
}

void qev_lock_read_unlock(qev_lock_t *lock) {
	__sync_fetch_and_sub(&lock->readers, 1);
}

void qev_lock_write_lock(qev_lock_t *lock) {
	// Acquire the single write lock
	while (!__sync_bool_compare_and_swap(&lock->writer, 0, 1)) {
		pause();
	}
	
	// Wait until all the readers are done
	while (!__sync_bool_compare_and_swap(&lock->readers, 0, 0)) {
		pause();
	}
}

void qev_lock_write_unlock(qev_lock_t *lock) {
	__sync_bool_compare_and_swap(&lock->writer, 1, 0);
}