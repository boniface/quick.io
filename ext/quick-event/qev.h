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

#ifndef QEV_CLIENT_T
	#define QEV_CLIENT_T struct qev_client
#endif

#ifdef __linux__
	#include "qev_linux.h"
#else
	#error No platform setup for quick-event
#endif

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

/**
 * The maximum number of events that should be asked for from the OS
 */
#define QEV_MAX_EVENTS 100

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
	 * To do atomic operations on the client, we need something of size int. So rather
	 * than use a small data type, we're stuck with this.
	 */
	volatile int _operations;
	
	/**
	 * A bitmask of flags for qev internal use.
	 */
	volatile qev_flags_t _flags;
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
int qev_listen(char *ip_address, uint16_t port);
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
int qev_listen_ssl(char *ip_address, uint16_t port, char *cert_path, char *key_path);

/**
 * Read data from the socket.
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
 * Clean up the client, fire the callback to the server that client has closed, and free the
 * client's memory.
 *
 * @param client The client that closed.
 */
void qev_client_free(void *client);

/**
 * Change to a different user. This function will refuse to run as root and return
 * and error if you try to.
 *
 * @param username The username to run as
 *
 * @return 0 on success
 * @return -1 on error
 */
int qev_chuser(char *username);

/**
 * A debugging hook: flushes all closed connections immediately, regardless of the
 * consequences.
 *
 * @note You probably only want to use this for test cases.
 */
void qev_debug_flush();

#endif