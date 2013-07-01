/**
 * Controls the statistics monitoring and reporting for all of the processes running on the server.
 * The main process aggregates and sends all of the statistics.
 * @file stats.h
 */
#ifndef QIO_STATS_H
#define QIO_STATS_H

#include "qio.h"

/**
 * The time between stats flushes (in seconds)
 */
#define STATS_INTERVAL 10

/**
 * The list of all stats that are reset on every tick.
 */
#define STATS_S_COUNTERS \
	/** Messages from clients */ \
	X(client_message_received, "client.messages.received") \
	\
	/** Messages to clients */ \
	X(client_message_sent, "client.messages.sent") \
	\
	/** Messages to clients */ \
	X(client_failed_writes, "client.failed_writes") \
	\
	/** Times the client buffer was hit */ \
	X(conns_buffer_read, "conns.buffer_reads") \
	\
	/** Clients that were rate limited and closed. */ \
	X(conns_ratelimited, "conns.ratelimited") \
	\
	/** Successful handshakes. */ \
	X(conns_handshakes, "conns.handshakes") \
	\
	/** Times we had to wait for a client (CLIENT_WAITs). */ \
	X(conns_client_wait, "conns.client_wait") \
	\
	/** Times we had to wait for a client (CLIENT_WAITs). */ \
	X(conns_client_fatal, "conns.client_fatal") \
	\
	/** Clients that timed out. */ \
	X(conns_timeouts, "conns.timeouts") \
	\
	/** Clients balanced. */ \
	X(conns_balanced, "conns.balanced") \
	\
	/** Client connections. */ \
	X(conns_new, "conns.new") \
	\
	/** Client disconnects. */ \
	X(conns_closed, "conns.closed") \
	\
	/** Subscriptions added to a client */ \
	X(evs_client_subscribes, "evs_clients.clientsubs.added") \
	\
	/** Subscriptions rejected */ \
	X(evs_client_subscribes_rejected, "evs_clients.clientsubs.rejected") \
	\
	/** Subscriptions removed from a client */ \
	X(evs_client_unsubscribes, "evs_clients.clientsubs.removed") \
	\
	/** Subscriptions created */ \
	X(evs_client_subscription_new, "evs_clients.subscriptions.new") \
	\
	/** Subscriptions freed */ \
	X(evs_client_subscription_freed, "evs_clients.subscriptions.freed") \
	\
	/** Total number of individual messages broadcasted */ \
	X(evs_client_messages_broadcasted, "evs_clients.messages.broadcasted.total") \
	\
	/** Count of messages broadcasted to users */ \
	X(evs_client_messages_broadcasted_to_client, "evs_clients.messages.broadcasted.to_client") \
	\
	/** Count of messages broadcasted to users */ \
	X(evs_client_messages_broadcasted_client_fatal, "evs_clients.messages.broadcasted.client_fatal") \
	\
	/** Callbacks sent to single clients */ \
	X(evs_client_messages_callback, "evs_clients.messages.callbacks.sent") \
	\
	/** Callbacks sent to single clients */ \
	X(evs_client_messages_callbacks_failure, "evs_clients.messages.callbacks.failure") \
	\
	/** Error callbacks sent to single clients */ \
	X(evs_client_messages_callbacks_qio_error, "evs_clients.messages.callbacks.qio_errors") \
	\
	/** The number of heartbeats sent */ \
	X(evs_client_heartbeats, "evs_clients.heartbeats.sent") \
	\
	/** The number of heartbeat challenges sent */ \
	X(evs_client_heartbeats_inactive_challenges, "evs_clients.heartbeats.challenges") \
	\
	/** The number of clients that failed the heartbeat challenge */ \
	X(evs_client_heartbeats_inactive_closed, "evs_clients.heartbeats.closed") \
	\
	/** Callbacks sent to the server */ \
	X(evs_server_callback, "evs_server.callbacks_received") \
	\
	/** Callbacks created on clients */ \
	X(evs_server_callback_created, "evs_server.callbacks_created") \
	\
	/** Pings to the server */ \
	X(evs_server_ping, "evs_server.pings") \
	\
	/** Noops to the server */ \
	X(evs_server_noop, "evs_server.noops") \
	\
	/** Events routed through the server */ \
	X(evs_server_event, "evs_server.events") \
	\
	/** Flash handshakes. */ \
	X(handler_flash_handshake, "handler.flash.handshakes") \
	\
	/** RFC6455 handshakes */ \
	X(handler_rfc6455_handshake, "handler.rfc6455.handshakes") \
	\
	/** RFC6455 read starts */ \
	X(handler_rfc6455_start, "handler.rfc6455.starts") \
	\
	/** RFC6455 read continues */ \
	X(handler_rfc6455_continue, "handler.rfc6455.continues") \
	\
	/** RFC6455 frames prepared for writing */ \
	X(handler_rfc6455_frames_prepared, "handler.rfc6455.frames_prepared") \
	\
	/** RFC6455 close frames */ \
	X(handler_rfc6455_close, "handler.rfc6455.closes") \
	\
	/** The number of events received from the OS */ \
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
	X(clients, "clients")

/**
 * The statistics that are recorded.
 *
 * @warning Each member of the struct MUST be a size_t, or this won't work.
 */
struct stats {

	/**
	 * All of the stats are in one struct
	 */
	#define X(slot, name) gsize slot;
		STATS_S_VALUES
		STATS_S_COUNTERS
	#undef X

};

/**
 * The callback type sent to apps for easy appending to the current set of stats.
 *
 * @param prefix The prefix to be added to the key: this should be NULL if there is no prefix
 * @param key The key the application sent
 * @param the value the application sent
 */
typedef void (*stats_app_append_cb)(gchar *prefix, gchar *key, double val);

/**
 * The stats counters. Be VERY careful with this, and ONLY use g_atomic_* functions with it.
 *
 * @note Globally accessible.
 */
struct stats *stats;

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
#define STATS_INC(counter) g_atomic_pointer_add(&stats->counter, 1)

/**
 * Decrement a counter found in stats_t
 */
#define STATS_DEC(counter) g_atomic_pointer_add(&stats->counter, -1)

#ifdef TESTING
#include "../test/test_stats.h"
#endif

#endif
