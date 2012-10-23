/**
 * Controls the statistics monitoring and reporting for all of the processes running on the server.
 * The main process aggregates and sends all of the statistics.
 * @file stats.h
 */

#pragma once
#include "qio.h"

/**
 * The maximum size the graphite buffer is allowed to be before flushing.
 */
#define STATS_MAX_BUFFER_SIZE 512

/**
 * The list of all stats that are reset on every tick.
 */
#define STATS_S_COUNTERS \
	X(clients_new, "clients.new.count") /** The number of new client connections. */ \
	X(clients_closed, "clients.closed.count") /** The number of client disconnects. */ \
	X(clients_balanced, "clients.balanced.count") /** The number of clients balanced. */ \
	X(clients_ratelimited, "clients.ratelimited.count") /** The number of clients that were rate limited and closed. */ \
	X(client_timeouts, "clients.timeouts.count") /** The number of clients that timed out. */ \
	X(client_handshakes, "clients.handshakes.count") /** The number of successful handshakes. */ \
	X(messages_sent, "messages.sent.count") /** The number of messages sent in this tick.  */ \
	X(messages_received, "messages.received.count") /** The number of messages received in this tick. */

/**
 * The fields that can be sent to graphite.
 */
#define STATS_S_VALUES \
	X(clients, "clients.connected") /** The number of clients connected. */ \

/**
 * The statistics that are recorded.
 *
 * @important Each member of the struct MUST be a size_t, or this won't work.
 */
typedef struct stats_s {
	
	#define X(slot, name) gsize slot;
		STATS_S_VALUES
		STATS_S_COUNTERS
	#undef X

} stats_t;

/**
 * The callback type sent to apps for easy appending to the current set of stats.
 *
 * @param prefix The prefix to be added to the key: this should be NULL if there is no prefix
 * @param key The key the application sent
 * @param the value the application sent
 */
typedef void (*stats_app_append_cb)(gchar *prefix, gchar *key, gsize val);

/**
 * The stats counters. Be VERY careful with this, and ONLY use g_atomic* functions with it.
 *
 * @note Globally accessible.
 */
stats_t stats;

/**
 * Flushes all the stats to graphite.
 */
void stats_flush();

/**
 * Gets the number of clients currently connected.
 *
 * @ingroup AppFunctions
 *
 * @return The number of clients connected.
 */
APP_EXPORT gsize stats_clients();

/**
 * Sets up the shared memory used across all the processes for stats recording
 */
gboolean stats_init();

/**
 * Increment a counter found in stats_t.
 */
#define STATS_INC(counter) g_atomic_pointer_add(&(stats.counter), 1);

/**
 * Decrement a counter found in stats_t
 */
#define STATS_DEC(counter) g_atomic_pointer_add(&(stats.counter), -1);

#ifdef TESTING
#include "../test/test_stats.h"
#endif