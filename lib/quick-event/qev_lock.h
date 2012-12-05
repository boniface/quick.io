/**
 * Provides a read-write spin lock.
 * @file qev_lock.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

/**
 * Contains all the fields necessary for locking.
 *
 * @note All fields are private and should not be modified from outside.
 */
typedef struct {
	/**
	 * The number of active readers.
	 */
	guint readers;
	
	/**
	 * The number of readers waiting
	 */
	guint readers_waiting;
	
	/**
	 * If there is currently a writer
	 */
	guint writer;
} qev_lock_t;

/**
 * Create a new RW lock
 */
qev_lock_t* qev_lock_new();

/**
 * Free the lock structure.
 *
 * @note Clearing a lock that is in use will likely result in a segfault
 */
void qev_lock_free(qev_lock_t *lock);

/**
 * Obtain a read lock
 */
void qev_lock_read_lock(qev_lock_t *lock);

/**
 * Release a read lock
 */
void qev_lock_read_unlock(qev_lock_t *lock);

/**
 * Obtain a write lock
 */
void qev_lock_write_lock(qev_lock_t *lock);

/**
 * Release a write lock
 */
void qev_lock_write_unlock(qev_lock_t *lock);