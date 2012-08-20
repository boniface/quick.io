#pragma once
#include "qio.h"

/**
 * Creates locks across processes to allow pausing things for creating conditions.
 * @file utils_locks.h
 */

/**
 * The name of the semaphore in use.
 */
#define SEM_NAME "qio_test_lock"

/**
 * Init the lock mechanisms.
 *
 * @param file The path to the server (this is what a lock is created on).
 */
gboolean test_lock_init();

/**
 * Close down the lock used in the processes.
 */
void test_lock_close();

/**
 * Acquire the lock.
 */
void test_lock_acquire();

/**
 * Release the lock.
 */
void test_lock_release();