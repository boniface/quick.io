/**
 * Utilities for getting stats from the server for test cases.
 * @file utils_stats.h
 */

#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

// The specific utils files shouldn't require anything, they're utilities that
// operate independently of everything.  Don't include "test.h" here, or circular
// dependencies go crazy.
#include "qio.h"

/**
 * The name of the shared memory region.
 */
#define TEST_SHM_NAME "/qio.test.stats"

/**
 * Flags to open the memory region with
 */
#define TEST_SHM_FLAGS (O_RDWR | O_CREAT)

/**
 * Permissions to put on the memory region.
 */
#define TEST_SHM_MASK 0644

/**
 * The data used for recording stats.
 */
typedef struct test_stats_s {
	/**
	 * The number of hups that were seen.
	 */
	gsize conns_hups;
	
	/**
	 * The number of messages that clients sent.
	 */
	gsize conns_messages;
	
	/**
	 * The number of timeouts that were encountered.
	 */
	gsize conns_timeouts;
	
	/**
	 * The number of bad clients forcibly closed.
	 */
	gsize conns_bad_clients;
	
	/**
	 * The number of times clients were put into CLIENT_WAIT
	 */
	gsize conns_client_wait;
	
	/**
	 * The number of times app_prefork is hit.
	 */
	gsize apps_prefork;
	
	/**
	 * The number of times app_postfork is hit.
	 */
	gsize apps_postfork;
	
	/**
	 * The number of times app_client_connect is hit.
	 */
	gsize apps_client_connect;
	
	/**
	 * The number of times app_client_close is hit.
	 */
	gsize apps_client_close;
	
	/**
	 * The number of times app_evs_client_subscribe is hit.
	 */
	gsize apps_client_subscribe;
	
	/**
	 * The number of times app_evs_client_unsubscribe is hit.
	 */
	gsize apps_client_unsubscribe;
	
	/**
	 * The number of times the test_handler is hit.
	 */
	gsize apps_client_handler;
	
	/**
	 * The number of times the test_handler_on is hit.
	 */
	gsize apps_client_handler_on;
	
	/**
	 * The number of times the test_handler_off is hit.
	 */
	gsize apps_client_handler_off;
} test_stats_t;

/**
 * Sets up the shared memory for stats.
 */
void utils_stats_setup();

/**
 * Close and destroy the shared memory.
 */
void utils_stats_teardown();

/**
 * Gets the stats shared memory object.  No need to store a reference to this: just
 * call it as needed.
 *
 * @ingroup ModuleFunctions
 *
 * @note You should use the Glib Atomic operations for the slots in this.
 * @see http://developer.gnome.org/glib/2.32/glib-Atomic-Operations.html
 */
MODULE_EXPORT test_stats_t* utils_stats();

/**
 * A helper for incrementing a slot in test_stats_t
 */

#ifdef TESTING
	#define UTILS_STATS_INC(slot) g_atomic_pointer_add(&(utils_stats()->slot), 1)
#else
	#define UTILS_STATS_INC(slot)
#endif