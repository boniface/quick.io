/**
 * Controls the statistics monitoring and reporting for all of the processes running on the server.
 * The main process aggregates and sends all of the statistics.
 * @file stats.h
 */

#pragma once
#include "qio.h"

/**
 * The time between stats flushes (in seconds)
 */
#define STATS_INTERVAL 10

/**
 * The list of all stats that are reset on every tick.
 */
#define STATS_S_COUNTERS \
	/** The number of new client connections. */ \
	X(clients_new, "clients.new") \
	\
	/** The number of client disconnects. */ \
	X(clients_closed, "clients.closed") \
	\
	/** The number of clients balanced. */ \
	X(clients_balanced, "clients.balanced") \
	\
	/** The number of clients that were rate limited and closed. */ \
	X(clients_ratelimited, "clients.ratelimited") \
	\
	/** The number of clients that timed out. */ \
	X(client_timeouts, "clients.timeouts") \
	\
	/** The number of successful handshakes. */ \
	X(client_handshakes, "clients.handshakes") \
	\
	/** The number of flash handshakes. */ \
	X(client_handshakes_flash, "clients.handshakes_flash") \
	\
	/** The number of socket write errors */ \
	X(client_failed_writes, "clients.failed_writes") \
	\
	/** The number of heartbeats sent */ \
	X(heartbeats, "heartbeats") \
	\
	/** The number of heartbeat challenges sent */ \
	X(heartbeats_inactive_challenges, "heartbeats.inactive.challenges") \
	\
	/** The number of clients that failed the heartbeat challenge */ \
	X(heartbeats_inactive_closed, "heartbeats.inactive.closed") \
	\
	/** The number of messages destined for 1 client */ \
	X(messages_sent, "messages.sent") \
	\
	/** The number of messages sent in broadcast */ \
	X(messages_broadcast, "messages.broadcast") \
	\
	/** The number of messages received in this tick. */ \
	X(messages_received, "messages.received") \
	\
	/** The number of events recieved from the OS */ \
	X(qev_event, "qev.event") \
	\
	/** The number of times a QEV client lock spun */ \
	X(qev_lock_client_spin, "qev.lock.client.spin") \
	\
	/** The number of spins on a read lock */ \
	X(qev_lock_read_spin, "qev.lock.read.spin") \
	\
	/** The number of false acquires on a read lock */ \
	X(qev_lock_read_false_acquire, "qev.lock.read.false_acquire") \
	\
	/** The number of spins on a write lock, waiting for the lock */ \
	X(qev_lock_write_spin, "qev.lock.write.spin") \
	\
	/** The number of spins on a write lock, waiting for the readers */ \
	X(qev_lock_write_wait, "qev.lock.write.wait")

/**
 * The fields that can be sent to graphite.
 */
#define STATS_S_VALUES \
	/** The number of clients connected. */ \
	X(clients, "clients.connected")

/**
 * The statistics that are recorded.
 *
 * @warning Each member of the struct MUST be a size_t, or this won't work.
 */
typedef struct stats_s {

	/**
	 * All of the stats are in one struct
	 */
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
typedef void (*stats_app_append_cb)(gchar *prefix, gchar *key, double val);

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
#define STATS_INC(counter) g_atomic_pointer_add(&(stats.counter), 1)

/**
 * Decrement a counter found in stats_t
 */
#define STATS_DEC(counter) g_atomic_pointer_add(&(stats.counter), -1)

#ifdef TESTING
#include "../test/test_stats.h"
#endif
