/**
 * Provides all the qev_* functions that are needed for creating an event loop.
 * @file qev.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#ifndef QEV_HEADER_
#define QEV_HEADER_

#include <glib.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#ifndef OPENSSL_THREADS
	#error OpenSSL thread support not enabled! Failing...
#endif

#ifndef QEV_CLIENT_T
	#define QEV_CLIENT_T struct qev_client
#endif

#ifdef __linux__
	#include "qev_linux.h"
#else
	#error No platform setup for quick-event
#endif

#include "qev_lock.h"
#include "qev_wqueue.h"

/**
 * Do nothing
 */
#define QEV_TEST_LOCK(name)

#ifndef QEV_LISTEN_BACKLOG
	/**
	 * How many connections should be backlogged in the kernel. Feel free to override.
	 */
	#define QEV_LISTEN_BACKLOG 1000
#endif

#ifndef QEV_MAX_EVENTS
/**
 * The maximum number of events that should be asked for from the OS
 */
#define QEV_MAX_EVENTS 100
#endif

/**
 * Indicates that a timer MUST be exclusive to a single thread
 */
#define QEV_TIMER_EXCLUSIVE 0x01

/**
 * Indicates that a timer callback should be executed only after
 * all the clients have been processed.
 */
#define QEV_TIMER_DELAYED 0x02

/**
 * An approximation of the current time, in seconds since Jan 1, 1970.
 * This is updated every so often to reflect a fairly-recent system time.
 */
__attribute__((__visibility__("default"))) gint64 qev_time;

/**
 * So that the flags can be changed on a whim everywhere
 */
typedef char qev_flags_t;

/**
 * Contains all of the information needed for quick-event to have a functioning
 * client. This struct may be embedded inside another struct, provided qev can access it.
 */
struct qev_client {
	/**
	 * The underlying socket connection.
	 */
	qev_socket_t socket;

	/**
	 * For ssl: everything needs a context
	 */
	void *ssl_ctx;

	/**
	 * For acquiring a lock on a client.
	 */
	guint _lock;

	/**
	 * For the re-entrant part of the lock: holds which thread has the lock
	 */
	void *_locking_thread;

	/**
	 * For assuring that only 1 thread will ever issue a read event on a client
	 * at a time.
	 */
	int _read_operations;

	/**
	 * A bitmask of flags for qev internal use.
	 */
	qev_flags_t _flags;
};

/**
 * The function type for timer callbacks.
 */
typedef void (*qev_timer_cb)();

/**
 * The type of function that receives all callbacks dealing with clients.
 *
 * @param client The client being dealt with
 */
typedef void (*qev_client_cb)(QEV_CLIENT_T *client);

/**
 * Setup everything needed to run quick event.
 *
 * @return 0 If everything was setup correctly and quick event can run.
 * @return -1 If there was a fatal error.
 */
int qev_init();

/**
 * Run the event loop. All this does is repeatedly call qev_dispatch()
 *
 * @note This loop runs for the duration of the program.
 */
void qev_run();

/**
 * Run the event loop in a thread. This is just the correct function type
 * for running in a thread, and does the same thing as qev_run().
 *
 * @param arg Does nothing
 *
 * @return Nothing
 */
void* qev_run_thread(void *arg);

/**
 * Run a single event iteration.
 */
void qev_dispatch();

/**
 * Instruct quick event to listen on a socket for connections and route
 * them into the event handler.
 *
 * @param ip_address The IP address to listen on
 * @param port The port to listen on
 *
 * @return 0 on success.
 * @return -1 on error.
 */
int qev_listen(const char *ip_address, const uint16_t port);

/**
 * Instruct quick event to listen on an SSL socket for connections and route
 * them into the event handler.
 *
 * @param ip_address The IP address to listen on
 * @param port The port to listen on
 * @param cert_path The path to the server's CA file
 * @param key_path The path to the server's private key file
 *
 * @return 0 on success.
 * @return -1 on error.
 */
int qev_listen_ssl(
	const char *ip_address,
	const uint16_t port,
	const char *cert_path,
	const char *key_path);

/**
 * Instruct quick event to listen on on a UDP socket for messages.
 *
 * @param ip_address The IP address to listen on
 * @param port The port to listen on
 *
 * @return 0 on success.
 * @return -1 on error.
 */
int qev_listen_udp(const char *ip_address, const uint16_t port);

/**
 * Read data from the socket.
 *
 * @attention Since read events are only ever raised in a single thread,
 * this function does no locking.
 *
 * @param client The client to read from
 * @param buff The buffer to read data in to
 * @param buff_size The maximum amount of data that can be put into the buffer
 *
 * @return -1 on error
 * @return 0 on socket close
 * @return >0 for the amount of data read
 */
int qev_read(QEV_CLIENT_T *client, char *buff, size_t buff_size);

/**
 * Writes data to the socket.
 *
 * @attention This function acquires a lock on a client, and as the lock
 * is not reentrant, it must not be held by the caller, or this will
 * block forever.
 *
 * @param client The client to write to.
 * @param buff The buffer to send to the client
 * @param buff_size The amount of data in the buffer
 *
 * @param -1 on error
 * @param >=0 the amount of data sent
 */
int qev_write(QEV_CLIENT_T *client, char *buff, size_t buff_size);

/**
 * Close and free the client when it is safe to do so. After calling this, no more read callbacks
 * will be fired on this client, and you will receive the close callback only when all the
 * underlying polling mechanisms have synced.
 *
 * @param client The client to destroy.
 */
void qev_close(QEV_CLIENT_T *client);

/**
 * Acquire a lock on a client. This function will busy wait, while yielding its
 * runtime, until it is able to acquire a lock on a client, so you probably
 * shouldn't use this for things like long-block IO operations, or other
 * things that take a while.
 *
 * @note This is a reentrant lock.
 *
 * @param client The client to acquire the lock on.
 */
void qev_client_lock(QEV_CLIENT_T *client);

/**
 * Release a lock on a client.
 *
 * @note A client can be unlocked from anything thread, but chances are, that's
 * a really bad idea.
 *
 * @param client The client to release the lock on.
 */
void qev_client_unlock(QEV_CLIENT_T *client);

/**
 * Change to a different user. This function will refuse to run as root and return
 * and error if you try to.
 *
 * @param username The username to run as
 *
 * @return 0 on success
 * @return -1 on error
 */
int qev_chuser(const char *username);

/**
 * A debugging hook: flushes all closed connections immediately, regardless of the
 * consequences.
 *
 * @note You probably only want to use this for test cases.
 */
void qev_debug_flush();

#endif
