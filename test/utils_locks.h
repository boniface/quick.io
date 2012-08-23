/**
 * Creates locks across processes to allow pausing things for creating conditions.
 * @file utils_locks.h
 */

#pragma once
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

// The specific utils files shouldn't require anything, they're utilities that
// operate independently of everything.  Don't include "test.h" here, or circular
// dependencies go crazy.
#include "qio.h"

/**
 * The name of the semaphore in use.
 */
#define SEM_NAME "qio_test_lock"

/**
 * Init the lock mechanisms.
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