/**
 * Controls the statistics monitoring and reporting for all of the processes running on the server.
 * The main process aggregates and sends all of the statistics.
 * @file stats.h
 */

#pragma once
#include "qio.h"

/**
 * The file name for the shared memory.
 */
#define SHM_NAME "/qio.stats"

/**
 * Flags to open the memory region with
 */
#define SHM_FLAGS (O_RDWR | O_CREAT)

/**
 * Permissions to put on the memory region.
 */
#define SHM_MASK 0644

/**
 * The list of all stats that are reset on every tick.
 */
#define STATS_S_FIELDS_RESETABLE \
	X(gsize, clients_new) /** The number of new client connections. */ \
	X(gsize, clients_closed) /** The number of client disconnects. */ \
	X(gsize, clients_balanced) /** The number of clients balanced. */ \
	X(gsize, client_timeouts) /** The number of clients that timed out. */ \
	X(gsize, messages_sent) /** The number of messages sent in this tick.  */ \
	X(gsize, messages_received) /** The number of messages received in this tick. */

/**
 * The fields that can be sent to graphite.
 */
#define STATS_S_GRAPHITE \
	X(gsize, clients) /** The number of clients connected. */ \
	STATS_S_FIELDS_RESETABLE

/**
 * The total definition of the stats.
 *
 * @important All types MUST be `gsize`
 */
#define STATS_S_FIELDS \
	X(gsize, time) /** The time at which these stats were grabbed. */ \
	STATS_S_GRAPHITE

/**
 * The statistics that are recorded.
 *
 * @important Each member of the struct MUST be a size_t, or this won't work.
 */
typedef struct stats_s {

	#define X(type, name) type name;
		STATS_S_FIELDS
	#undef X

} stats_t;

/**
 * The shared memory region.
 * @note Globally accessible, across all processes.
 */
stats_t *stats;

/**
 * Records stats for the last second. Should be hit every second.
 */
void stats_tick();

/**
 * Respond to a stats request on the web interface.
 *
 * @param client The client making the request.
 */
void stats_request(qsys_socket_t sock);

/**
 * Sets up the shared memory used across all the processes for stats recording
 */
gboolean stats_init();

/**
 * Increment a counter found in stats_t.
 */
#define STATS_INC(counter) g_atomic_pointer_add(&(stats->counter), 1);

/**
 * Decrement a counter found in stats_t
 */
#define STATS_DEC(counter) g_atomic_pointer_add(&(stats->counter), -1);

#ifdef TESTING
#include "../test/test_stats.h"
#endif